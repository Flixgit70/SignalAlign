#include "AnalysisEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>

namespace signalalign
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

int fftOrderForSize(int size)
{
    int order = 0;
    while ((1 << order) < size)
        ++order;
    return order;
}

double smoothStep(double x) noexcept
{
    x = juce::jlimit(0.0, 1.0, x);
    return x * x * (3.0 - 2.0 * x);
}
}

AnalysisEngine::AnalysisEngine(juce::AudioProcessorValueTreeState& state)
    : juce::Thread("SignalAlign Analysis"), parameters(state)
{
    pendingRef.reserve(131072);
    pendingMeas.reserve(131072);
    configureFFT(65536);
    startThread(juce::Thread::Priority::normal);
}

AnalysisEngine::~AnalysisEngine()
{
    signalThreadShouldExit();
    notify();
    stopThread(2000);
}

void AnalysisEngine::setSampleRate(double newSampleRate) noexcept
{
    if (newSampleRate > 1000.0)
        sampleRate.store(newSampleRate, std::memory_order_relaxed);
}

void AnalysisEngine::pushAudio(const float* ref, const float* meas, int numSamples) noexcept
{
    if (ref == nullptr || meas == nullptr || numSamples <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    if (size1 > 0)
    {
        std::memcpy(fifoRef.data() + start1, ref, static_cast<size_t>(size1) * sizeof(float));
        std::memcpy(fifoMeas.data() + start1, meas, static_cast<size_t>(size1) * sizeof(float));
    }

    if (size2 > 0)
    {
        std::memcpy(fifoRef.data() + start2, ref + size1, static_cast<size_t>(size2) * sizeof(float));
        std::memcpy(fifoMeas.data() + start2, meas + size1, static_cast<size_t>(size2) * sizeof(float));
    }

    fifo.finishedWrite(size1 + size2);
    notify();
}

AnalysisSnapshot AnalysisEngine::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return snapshot;
}

void AnalysisEngine::setFrozen(bool shouldFreeze) noexcept
{
    frozen.store(shouldFreeze, std::memory_order_relaxed);
}

void AnalysisEngine::requestReset() noexcept
{
    resetRequested.store(true, std::memory_order_release);
    notify();
}

int AnalysisEngine::getRequestedFFTSize() const
{
    const auto index = juce::jlimit(0, 6, static_cast<int>(parameters.getRawParameterValue(ParamIDs::fftSize)->load()));
    static constexpr int sizes[] { 2048, 4096, 8192, 16384, 32768, 65536, 131072 };
    return sizes[index];
}

double AnalysisEngine::getOverlapFraction() const
{
    const auto index = juce::jlimit(0, 3, static_cast<int>(parameters.getRawParameterValue(ParamIDs::overlap)->load()));
    static constexpr double overlaps[] { 0.0, 0.5, 0.75, 0.875 };
    return overlaps[index];
}

float AnalysisEngine::getAveragingAlpha() const
{
    const auto index = juce::jlimit(0, 3, static_cast<int>(parameters.getRawParameterValue(ParamIDs::averaging)->load()));
    static constexpr float alpha[] { 0.35f, 0.15f, 0.06f, 0.025f };
    return alpha[index];
}

double AnalysisEngine::getLowFrequency() const
{
    return parameters.getRawParameterValue(ParamIDs::lowFreq)->load();
}

double AnalysisEngine::getHighFrequency() const
{
    return parameters.getRawParameterValue(ParamIDs::highFreq)->load();
}

double AnalysisEngine::getSearchMilliseconds() const
{
    const auto index = juce::jlimit(0, 8, static_cast<int>(parameters.getRawParameterValue(ParamIDs::searchMs)->load()));
    static constexpr double values[] { 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 750.0, 1000.0 };
    return values[index];
}

double AnalysisEngine::getSmoothingSeconds() const
{
    const auto index = juce::jlimit(0, 5, static_cast<int>(parameters.getRawParameterValue(ParamIDs::smoothing)->load()));
    static constexpr double values[] { 0.0, 0.25, 0.5, 1.0, 2.0, 5.0 };
    return values[index];
}

double AnalysisEngine::getMinConfidence() const
{
    return juce::jlimit(0.0, 1.0,
                        static_cast<double>(parameters.getRawParameterValue(ParamIDs::minConfidence)->load()) / 100.0);
}

double AnalysisEngine::getMinCoherence() const
{
    return juce::jlimit(0.0, 1.0,
                        static_cast<double>(parameters.getRawParameterValue(ParamIDs::minCoherence)->load()) / 100.0);
}

double AnalysisEngine::getMinPeakRatio() const
{
    return juce::jlimit(1.0, 3.0,
                        static_cast<double>(parameters.getRawParameterValue(ParamIDs::minPeakRatio)->load()));
}

double AnalysisEngine::getMaxPhaseDiffMs() const
{
    return juce::jlimit(0.0, 20.0,
                        static_cast<double>(parameters.getRawParameterValue(ParamIDs::maxPhaseDiffMs)->load()));
}

double AnalysisEngine::getMaxStabilityMs() const
{
    return juce::jlimit(0.0, 2.0,
                        static_cast<double>(parameters.getRawParameterValue(ParamIDs::maxStabilityMs)->load()));
}

void AnalysisEngine::configureFFT(int newFftSize)
{
    fftSize = newFftSize;
    fftOrder = fftOrderForSize(fftSize);
    hopSize = juce::jmax(1, static_cast<int>(std::lround(fftSize * (1.0 - getOverlapFraction()))));

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        static_cast<size_t>(fftSize), juce::dsp::WindowingFunction<float>::hann, false);

    fftInRef.assign(static_cast<size_t>(fftSize), {});
    fftInMeas.assign(static_cast<size_t>(fftSize), {});
    fftOutRef.assign(static_cast<size_t>(fftSize), {});
    fftOutMeas.assign(static_cast<size_t>(fftSize), {});
    phatSpectrum.assign(static_cast<size_t>(fftSize), {});
    correlation.assign(static_cast<size_t>(fftSize), {});

    const auto bins = static_cast<size_t>(fftSize / 2 + 1);
    avgAutoRef.assign(bins, 0.0f);
    avgAutoMeas.assign(bins, 0.0f);
    avgCross.assign(bins, {});
    binCoherence.assign(bins, 0.0f);
    binEnergyWeight.assign(bins, 0.0f);
    binQualityWeight.assign(bins, 0.0f);
    averagesInitialised = false;
    averageFrameCount = 0;

    pendingRef.clear();
    pendingMeas.clear();
    resetDelaySmoothing();
    resetContinuityTracking();
    resetStabilityTracking();
}

void AnalysisEngine::reconfigureIfNeeded()
{
    const auto requestedSize = getRequestedFFTSize();
    const auto requestedHop = juce::jmax(1, static_cast<int>(std::lround(requestedSize * (1.0 - getOverlapFraction()))));

    if (requestedSize != fftSize)
        configureFFT(requestedSize);
    else
        hopSize = requestedHop;
}

void AnalysisEngine::run()
{
    std::array<float, 4096> tempRef {};
    std::array<float, 4096> tempMeas {};

    while (!threadShouldExit())
    {
        reconfigureIfNeeded();

        if (resetRequested.exchange(false, std::memory_order_acq_rel))
        {
            std::fill(avgAutoRef.begin(), avgAutoRef.end(), 0.0f);
            std::fill(avgAutoMeas.begin(), avgAutoMeas.end(), 0.0f);
            std::fill(avgCross.begin(), avgCross.end(), juce::dsp::Complex<float> {});
            std::fill(binCoherence.begin(), binCoherence.end(), 0.0f);
            std::fill(binEnergyWeight.begin(), binEnergyWeight.end(), 0.0f);
            std::fill(binQualityWeight.begin(), binQualityWeight.end(), 0.0f);
            averagesInitialised = false;
            averageFrameCount = 0;
            resetDelaySmoothing();
            resetContinuityTracking();
            resetStabilityTracking();

            std::lock_guard<std::mutex> lock(snapshotMutex);
            snapshot = {};
            snapshot.sampleRate = sampleRate.load(std::memory_order_relaxed);
            snapshot.fftSize = fftSize;
        }

        const int ready = fifo.getNumReady();
        if (ready <= 0)
        {
            wait(5);
            continue;
        }

        const int wanted = juce::jmin(ready, static_cast<int>(tempRef.size()));
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(wanted, start1, size1, start2, size2);

        if (size1 > 0)
        {
            std::memcpy(tempRef.data(), fifoRef.data() + start1, static_cast<size_t>(size1) * sizeof(float));
            std::memcpy(tempMeas.data(), fifoMeas.data() + start1, static_cast<size_t>(size1) * sizeof(float));
        }
        if (size2 > 0)
        {
            std::memcpy(tempRef.data() + size1, fifoRef.data() + start2, static_cast<size_t>(size2) * sizeof(float));
            std::memcpy(tempMeas.data() + size1, fifoMeas.data() + start2, static_cast<size_t>(size2) * sizeof(float));
        }
        fifo.finishedRead(size1 + size2);

        const auto count = size1 + size2;
        pendingRef.insert(pendingRef.end(), tempRef.begin(), tempRef.begin() + count);
        pendingMeas.insert(pendingMeas.end(), tempMeas.begin(), tempMeas.begin() + count);

        while (static_cast<int>(pendingRef.size()) >= fftSize)
        {
            if (!frozen.load(std::memory_order_relaxed))
                analyseFrame(pendingRef.data(), pendingMeas.data());

            const auto advance = static_cast<size_t>(juce::jmin(hopSize, static_cast<int>(pendingRef.size())));
            pendingRef.erase(pendingRef.begin(), pendingRef.begin() + static_cast<std::ptrdiff_t>(advance));
            pendingMeas.erase(pendingMeas.begin(), pendingMeas.begin() + static_cast<std::ptrdiff_t>(advance));
        }
    }
}

double AnalysisEngine::parabolicPeakOffset(double left, double centre, double right) noexcept
{
    const auto denominator = left - 2.0 * centre + right;
    if (std::abs(denominator) < 1.0e-20)
        return 0.0;

    return juce::jlimit(-1.0, 1.0, 0.5 * (left - right) / denominator);
}

double AnalysisEngine::medianOf(std::deque<double> values)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const auto n = values.size();
    if ((n & 1U) != 0U)
        return values[n / 2U];

    return 0.5 * (values[n / 2U - 1U] + values[n / 2U]);
}

double AnalysisEngine::robustSigmaFromMad(const std::deque<double>& values)
{
    if (values.size() < 3)
        return 0.0;

    const auto centre = medianOf(values);
    std::deque<double> deviations;
    for (const auto value : values)
        deviations.push_back(std::abs(value - centre));

    return 1.4826 * medianOf(std::move(deviations));
}

void AnalysisEngine::resetDelaySmoothing() noexcept
{
    delayHistory.clear();
    phaseHistory.clear();
    delaySmoothingInitialised = false;
    phaseSmoothingInitialised = false;
    smoothedDelaySamples = 0.0;
    smoothedPhaseSamples = 0.0;
    lastSmoothingSeconds = -1.0;
    hasDelayEstimate = false;
}

void AnalysisEngine::resetContinuityTracking() noexcept
{
    continuityLocked = false;
    trackedDelaySamples = 0.0;
    challengerDelaySamples = 0.0;
    challengerCount = 0;
}

void AnalysisEngine::resetStabilityTracking() noexcept
{
    stabilityHistory.clear();
}

double AnalysisEngine::smoothDelayEstimate(double rawDelaySamples, double confidence, double smoothingSeconds, double fs)
{
    if (smoothingSeconds <= 0.0 || fs <= 1000.0)
    {
        delayHistory.clear();
        smoothedDelaySamples = rawDelaySamples;
        delaySmoothingInitialised = true;
        return rawDelaySamples;
    }

    const auto frameSeconds = juce::jmax(1.0 / fs, static_cast<double>(hopSize) / fs);
    auto historyLength = static_cast<int>(std::lround(0.25 * smoothingSeconds / frameSeconds));
    historyLength = juce::jlimit(3, 31, historyLength);
    if ((historyLength & 1) == 0)
        ++historyLength;

    if (confidence >= 0.08 || delayHistory.empty())
    {
        delayHistory.push_back(rawDelaySamples);
        while (static_cast<int>(delayHistory.size()) > historyLength)
            delayHistory.pop_front();
    }

    const auto robustTarget = medianOf(delayHistory);
    if (!delaySmoothingInitialised)
    {
        smoothedDelaySamples = robustTarget;
        delaySmoothingInitialised = true;
        return smoothedDelaySamples;
    }

    const auto baseAlpha = 1.0 - std::exp(-frameSeconds / smoothingSeconds);
    const auto confidenceScale = juce::jmap(juce::jlimit(0.0, 1.0, confidence), 0.25, 1.0);
    const auto alpha = juce::jlimit(0.0001, 1.0, baseAlpha * confidenceScale);
    smoothedDelaySamples += alpha * (robustTarget - smoothedDelaySamples);
    return smoothedDelaySamples;
}

double AnalysisEngine::smoothPhaseEstimate(double rawPhaseSamples, double confidence, double smoothingSeconds, double fs)
{
    if (smoothingSeconds <= 0.0 || fs <= 1000.0)
    {
        phaseHistory.clear();
        smoothedPhaseSamples = rawPhaseSamples;
        phaseSmoothingInitialised = true;
        return rawPhaseSamples;
    }

    const auto frameSeconds = juce::jmax(1.0 / fs, static_cast<double>(hopSize) / fs);
    auto historyLength = static_cast<int>(std::lround(0.20 * smoothingSeconds / frameSeconds));
    historyLength = juce::jlimit(3, 21, historyLength);
    if ((historyLength & 1) == 0)
        ++historyLength;

    if (confidence >= 0.08 || phaseHistory.empty())
    {
        phaseHistory.push_back(rawPhaseSamples);
        while (static_cast<int>(phaseHistory.size()) > historyLength)
            phaseHistory.pop_front();
    }

    const auto robustTarget = medianOf(phaseHistory);
    if (!phaseSmoothingInitialised)
    {
        smoothedPhaseSamples = robustTarget;
        phaseSmoothingInitialised = true;
        return smoothedPhaseSamples;
    }

    const auto baseAlpha = 1.0 - std::exp(-frameSeconds / smoothingSeconds);
    const auto confidenceScale = juce::jmap(juce::jlimit(0.0, 1.0, confidence), 0.25, 1.0);
    const auto alpha = juce::jlimit(0.0001, 1.0, baseAlpha * confidenceScale);
    smoothedPhaseSamples += alpha * (robustTarget - smoothedPhaseSamples);
    return smoothedPhaseSamples;
}

AnalysisEngine::PeakCandidate AnalysisEngine::refineCandidateAtLag(int lag, int maxLag) const
{
    auto correlationAtLag = [this](int requestedLag)
    {
        const auto wrappedLag = requestedLag >= 0 ? requestedLag : fftSize + requestedLag;
        if (wrappedLag < 0 || wrappedLag >= fftSize)
            return 0.0;
        return static_cast<double>(correlation[static_cast<size_t>(wrappedLag)].real());
    };

    PeakCandidate candidate;
    candidate.lag = juce::jlimit(-maxLag, maxLag, lag);
    candidate.signedValue = correlationAtLag(candidate.lag);
    candidate.value = std::abs(candidate.signedValue);

    const auto left = std::abs(correlationAtLag(candidate.lag - 1));
    const auto centre = candidate.value;
    const auto right = std::abs(correlationAtLag(candidate.lag + 1));
    candidate.fractionalLag = static_cast<double>(candidate.lag) + parabolicPeakOffset(left, centre, right);
    return candidate;
}

std::vector<AnalysisEngine::PeakCandidate> AnalysisEngine::collectPeakCandidates(int maxLag, int guardRadius) const
{
    auto absAtLag = [this](int lag)
    {
        const auto index = lag >= 0 ? lag : fftSize + lag;
        return std::abs(static_cast<double>(correlation[static_cast<size_t>(index)].real()));
    };

    std::vector<PeakCandidate> localPeaks;
    localPeaks.reserve(64);

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        const auto centre = absAtLag(lag);
        const auto left = lag > -maxLag ? absAtLag(lag - 1) : -1.0;
        const auto right = lag < maxLag ? absAtLag(lag + 1) : -1.0;

        if (centre >= left && centre >= right)
            localPeaks.push_back(refineCandidateAtLag(lag, maxLag));
    }

    std::sort(localPeaks.begin(), localPeaks.end(), [](const auto& a, const auto& b)
    {
        return a.value > b.value;
    });

    std::vector<PeakCandidate> independentPeaks;
    independentPeaks.reserve(12);

    for (const auto& candidate : localPeaks)
    {
        bool tooClose = false;
        for (const auto& accepted : independentPeaks)
        {
            if (std::abs(candidate.fractionalLag - accepted.fractionalLag) <= guardRadius)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
        {
            independentPeaks.push_back(candidate);
            if (independentPeaks.size() >= 12)
                break;
        }
    }

    if (independentPeaks.empty())
        independentPeaks.push_back(refineCandidateAtLag(0, maxLag));

    return independentPeaks;
}

AnalysisEngine::PeakCandidate AnalysisEngine::selectContinuityCandidate(const std::vector<PeakCandidate>& candidates,
                                                                         const PeakCandidate& globalBest,
                                                                         double meanCoherence,
                                                                         double minPeakRatio,
                                                                         int continuityWindowSamples,
                                                                         bool& continuityQualified,
                                                                         bool& switchPending)
{
    continuityQualified = true;
    switchPending = false;

    if (!continuityLocked || candidates.empty())
        return globalBest;

    const auto globalSecond = candidates.size() > 1 ? candidates[1].value : 0.0;
    const auto globalPeakRatio = globalBest.value / juce::jmax(1.0e-20, globalSecond);

    const PeakCandidate* bestNear = nullptr;
    for (const auto& candidate : candidates)
    {
        if (std::abs(candidate.fractionalLag - trackedDelaySamples) <= continuityWindowSamples)
        {
            if (bestNear == nullptr || candidate.value > bestNear->value)
                bestNear = &candidate;
        }
    }

    // If a plausible peak remains near the last trustworthy value, prefer it over
    // a newly appearing remote peak. Peak-ratio gating can still hide the frame if
    // the two peaks are genuinely ambiguous.
    if (bestNear != nullptr && bestNear->value >= globalBest.value * 0.65)
    {
        challengerCount = 0;
        return *bestNear;
    }

    if (std::abs(globalBest.fractionalLag - trackedDelaySamples) <= continuityWindowSamples)
    {
        challengerCount = 0;
        return globalBest;
    }

    const bool challengerStrong = meanCoherence >= 0.35 && globalPeakRatio >= minPeakRatio;
    if (!challengerStrong)
    {
        challengerCount = 0;
        continuityQualified = false;
        return globalBest;
    }

    if (challengerCount == 0
        || std::abs(globalBest.fractionalLag - challengerDelaySamples) > continuityWindowSamples)
    {
        challengerDelaySamples = globalBest.fractionalLag;
        challengerCount = 1;
    }
    else
    {
        challengerDelaySamples = 0.65 * challengerDelaySamples + 0.35 * globalBest.fractionalLag;
        ++challengerCount;
    }

    switchPending = challengerCount < 3;
    continuityQualified = challengerCount >= 3;
    return globalBest;
}

AnalysisEngine::PhaseFitResult AnalysisEngine::calculateResidualPhaseFit(double,
                                                                          int firstBin,
                                                                          int lastBin,
                                                                          double) const
{
    // Group delay from adjacent-bin cross-phase differences. Because adjacent FFT
    // bins are separated by exactly fs/N, the phase step directly maps to delay in
    // samples and does not require long-range phase unwrapping.
    juce::dsp::Complex<double> vectorSum {};
    double sumWeights = 0.0;
    int pairCount = 0;

    for (int k = firstBin + 1; k <= lastBin; ++k)
    {
        const auto i0 = static_cast<size_t>(k - 1);
        const auto i1 = static_cast<size_t>(k);
        const auto w0 = static_cast<double>(binQualityWeight[i0]);
        const auto w1 = static_cast<double>(binQualityWeight[i1]);
        const auto weight = std::sqrt(w0 * w1);

        if (weight < 0.015)
            continue;

        const auto c0 = avgCross[i0];
        const auto c1 = avgCross[i1];
        if (std::abs(c0) < 1.0e-20f || std::abs(c1) < 1.0e-20f)
            continue;

        const auto delta = static_cast<double>(std::arg(c1 * std::conj(c0)));
        vectorSum += std::polar(weight, delta);
        sumWeights += weight;
        ++pairCount;
    }

    PhaseFitResult result;
    if (pairCount < 6 || sumWeights <= 1.0e-9)
        return result;

    const auto meanDelta = std::arg(vectorSum);
    result.samples = -meanDelta * static_cast<double>(fftSize) / twoPi;
    result.quality = juce::jlimit(0.0, 1.0, std::abs(vectorSum) / sumWeights);
    result.valid = std::isfinite(result.samples) && result.quality > 0.05;
    return result;
}

void AnalysisEngine::calculateBandPlausibility(double candidateSamples,
                                                int firstBin,
                                                int lastBin,
                                                double fs,
                                                int& agreeing,
                                                int& usable,
                                                double& agreement) const
{
    agreeing = 0;
    usable = 0;
    agreement = 0.0;

    static constexpr double bandEdges[] { 80.0, 250.0, 500.0, 1000.0, 2000.0,
                                           4000.0, 8000.0, 16000.0, 24000.0 };

    const auto binForHz = [this, fs](double hz)
    {
        return static_cast<int>(std::lround(hz * fftSize / fs));
    };

    double supportSum = 0.0;

    for (size_t band = 0; band + 1 < std::size(bandEdges); ++band)
    {
        const int start = juce::jmax(firstBin, binForHz(bandEdges[band]));
        const int end = juce::jmin(lastBin, binForHz(bandEdges[band + 1]));
        if (end - start < 4)
            continue;

        juce::dsp::Complex<double> vectorSum {};
        double sumWeights = 0.0;
        int usedBins = 0;

        for (int k = start; k <= end; ++k)
        {
            const auto idx = static_cast<size_t>(k);
            const auto weight = static_cast<double>(binQualityWeight[idx]);
            const auto crossMagnitude = std::abs(avgCross[idx]);
            if (weight < 0.015 || crossMagnitude < 1.0e-20f)
                continue;

            const auto unitCross = avgCross[idx] / crossMagnitude;
            const auto correction = twoPi * static_cast<double>(k) * candidateSamples / static_cast<double>(fftSize);
            const auto corrected = juce::dsp::Complex<double>(unitCross.real(), unitCross.imag())
                                 * std::polar(1.0, correction);
            vectorSum += weight * corrected;
            sumWeights += weight;
            ++usedBins;
        }

        if (usedBins < 4 || sumWeights < 0.5)
            continue;

        const auto support = juce::jlimit(0.0, 1.0, std::abs(vectorSum) / sumWeights);
        supportSum += support;
        ++usable;

        if (support >= 0.55)
            ++agreeing;
    }

    if (usable > 0)
        agreement = supportSum / static_cast<double>(usable);
}

double AnalysisEngine::calculateSpectralDiversity(int firstBin, int lastBin) const
{
    double sum = 0.0;
    double sumSquares = 0.0;
    int activeBins = 0;

    for (int k = firstBin; k <= lastBin; ++k)
    {
        const auto w = static_cast<double>(binQualityWeight[static_cast<size_t>(k)]);
        if (w <= 1.0e-6)
            continue;

        sum += w;
        sumSquares += w * w;
        ++activeBins;
    }

    if (sumSquares <= 1.0e-20 || activeBins <= 0)
        return 0.0;

    const auto effectiveBins = (sum * sum) / sumSquares;
    const auto totalBins = juce::jmax(1, lastBin - firstBin + 1);
    const auto targetBins = juce::jmax(18.0, 0.035 * static_cast<double>(totalBins));
    return juce::jlimit(0.0, 1.0, (effectiveBins - 3.0) / juce::jmax(1.0, targetBins - 3.0));
}

void AnalysisEngine::analyseFrame(const float* ref, const float* meas)
{
    const auto fs = sampleRate.load(std::memory_order_relaxed);
    if (fs <= 1000.0 || fft == nullptr || window == nullptr)
        return;

    double energyRef = 0.0;
    double energyMeas = 0.0;

    for (int i = 0; i < fftSize; ++i)
    {
        const auto r = ref[i];
        const auto m = meas[i];
        energyRef += static_cast<double>(r) * r;
        energyMeas += static_cast<double>(m) * m;
        fftInRef[static_cast<size_t>(i)] = { r, 0.0f };
        fftInMeas[static_cast<size_t>(i)] = { m, 0.0f };
    }

    const auto rmsRef = std::sqrt(energyRef / fftSize);
    const auto rmsMeas = std::sqrt(energyMeas / fftSize);
    if (rmsRef < 1.0e-7 || rmsMeas < 1.0e-7)
        return;

    std::vector<float> windowedRef(static_cast<size_t>(fftSize));
    std::vector<float> windowedMeas(static_cast<size_t>(fftSize));
    for (int i = 0; i < fftSize; ++i)
    {
        windowedRef[static_cast<size_t>(i)] = fftInRef[static_cast<size_t>(i)].real();
        windowedMeas[static_cast<size_t>(i)] = fftInMeas[static_cast<size_t>(i)].real();
    }

    window->multiplyWithWindowingTable(windowedRef.data(), static_cast<size_t>(fftSize));
    window->multiplyWithWindowingTable(windowedMeas.data(), static_cast<size_t>(fftSize));

    for (int i = 0; i < fftSize; ++i)
    {
        fftInRef[static_cast<size_t>(i)] = { windowedRef[static_cast<size_t>(i)], 0.0f };
        fftInMeas[static_cast<size_t>(i)] = { windowedMeas[static_cast<size_t>(i)], 0.0f };
    }

    fft->perform(fftInRef.data(), fftOutRef.data(), false);
    fft->perform(fftInMeas.data(), fftOutMeas.data(), false);

    const int numBins = fftSize / 2 + 1;
    const float alpha = getAveragingAlpha();

    for (int k = 0; k < numBins; ++k)
    {
        const auto idx = static_cast<size_t>(k);
        const auto autoR = std::norm(fftOutRef[idx]);
        const auto autoM = std::norm(fftOutMeas[idx]);
        const auto cross = fftOutMeas[idx] * std::conj(fftOutRef[idx]);

        if (!averagesInitialised)
        {
            avgAutoRef[idx] = autoR;
            avgAutoMeas[idx] = autoM;
            avgCross[idx] = cross;
        }
        else
        {
            avgAutoRef[idx] += alpha * (autoR - avgAutoRef[idx]);
            avgAutoMeas[idx] += alpha * (autoM - avgAutoMeas[idx]);
            avgCross[idx] += alpha * (cross - avgCross[idx]);
        }
    }

    averagesInitialised = true;
    ++averageFrameCount;

    const double lowHz = juce::jlimit(10.0, fs * 0.45, getLowFrequency());
    const double highHz = juce::jlimit(lowHz + 10.0, fs * 0.5, getHighFrequency());
    const int firstBin = juce::jlimit(1, numBins - 1, static_cast<int>(std::ceil(lowHz * fftSize / fs)));
    const int lastBin = juce::jlimit(firstBin, numBins - 1, static_cast<int>(std::floor(highHz * fftSize / fs)));

    // ---------------------------------------------------------------------
    // Robust weighted GCC-PHAT
    // ---------------------------------------------------------------------
    // PHAT remains the master delay estimator. Coherence and energy are used
    // only to suppress unreliable frequency bins before the one broadband IFFT.
    std::fill(phatSpectrum.begin(), phatSpectrum.end(), juce::dsp::Complex<float> {});
    std::fill(binCoherence.begin(), binCoherence.end(), 0.0f);
    std::fill(binEnergyWeight.begin(), binEnergyWeight.end(), 0.0f);
    std::fill(binQualityWeight.begin(), binQualityWeight.end(), 0.0f);

    double maxCombinedPower = 0.0;
    for (int k = firstBin; k <= lastBin; ++k)
    {
        const auto idx = static_cast<size_t>(k);
        const auto combined = std::sqrt(static_cast<double>(avgAutoRef[idx]) * static_cast<double>(avgAutoMeas[idx]));
        maxCombinedPower = std::max(maxCombinedPower, combined);
    }

    double coherenceSum = 0.0;
    double coherenceWeight = 0.0;

    for (int k = firstBin; k <= lastBin; ++k)
    {
        const auto idx = static_cast<size_t>(k);
        const auto denom = static_cast<double>(avgAutoRef[idx]) * static_cast<double>(avgAutoMeas[idx]);
        const auto coherence = denom > 1.0e-24
            ? juce::jlimit(0.0, 1.0, static_cast<double>(std::norm(avgCross[idx])) / denom)
            : 0.0;

        const auto combined = std::sqrt(juce::jmax(0.0, denom));
        const auto relative = maxCombinedPower > 1.0e-30 ? combined / maxCombinedPower : 0.0;
        const auto relativeDb = relative > 1.0e-30 ? 10.0 * std::log10(relative) : -200.0;

        // A soft gate: bins below roughly -55 dB relative are ignored, bins above
        // -25 dB are fully trusted. Strong bins are capped rather than allowed to
        // dominate, preserving the PHAT principle.
        const auto energyWeight = smoothStep((relativeDb + 55.0) / 30.0);
        const auto coherenceShape = std::pow(coherence, 1.35);
        const auto qualityWeight = energyWeight * coherenceShape;

        binCoherence[idx] = static_cast<float>(coherence);
        binEnergyWeight[idx] = static_cast<float>(energyWeight);
        binQualityWeight[idx] = static_cast<float>(qualityWeight);

        if (energyWeight > 0.01)
        {
            coherenceSum += coherence * energyWeight;
            coherenceWeight += energyWeight;
        }

        const auto magnitude = std::abs(avgCross[idx]);
        if (magnitude > 1.0e-20f && qualityWeight > 0.005)
            phatSpectrum[idx] = static_cast<float>(qualityWeight) * avgCross[idx] / magnitude;
    }

    for (int k = 1; k < fftSize / 2; ++k)
        phatSpectrum[static_cast<size_t>(fftSize - k)] = std::conj(phatSpectrum[static_cast<size_t>(k)]);

    fft->perform(phatSpectrum.data(), correlation.data(), true);

    const auto requestedLag = static_cast<int>(std::lround(getSearchMilliseconds() * fs / 1000.0));
    const int maxLag = juce::jlimit(1, fftSize / 2 - 2, requestedLag);
    const auto bandwidthHz = juce::jmax(100.0, highHz - lowHz);
    const int peakGuard = juce::jlimit(3, juce::jmax(3, maxLag / 3),
                                      static_cast<int>(std::ceil(1.5 * fs / bandwidthHz)));

    const auto candidates = collectPeakCandidates(maxLag, peakGuard);
    const auto globalBest = candidates.front();
    const auto secondBestValue = candidates.size() > 1 ? candidates[1].value : 0.0;
    const auto peakRatio = globalBest.value / juce::jmax(1.0e-20, secondBestValue);
    const auto peakProminence = globalBest.value > 1.0e-20
        ? juce::jlimit(0.0, 1.0, (globalBest.value - secondBestValue) / globalBest.value)
        : 0.0;

    const auto meanCoherence = coherenceWeight > 0.0 ? coherenceSum / coherenceWeight : 0.0;
    const auto minPeakRatio = getMinPeakRatio();
    const int continuityWindowSamples = juce::jmax(4, static_cast<int>(std::lround(0.75 * fs / 1000.0)));

    bool continuityQualified = true;
    bool continuitySwitchPending = false;
    const auto selectedCandidate = selectContinuityCandidate(candidates,
                                                              globalBest,
                                                              meanCoherence,
                                                              minPeakRatio,
                                                              continuityWindowSamples,
                                                              continuityQualified,
                                                              continuitySwitchPending);
    const auto rawDelaySamples = selectedCandidate.fractionalLag;

    // ---------------------------------------------------------------------
    // Independent / supporting checks
    // ---------------------------------------------------------------------
    const auto phaseFit = calculateResidualPhaseFit(rawDelaySamples, firstBin, lastBin, fs);
    const auto phaseDifferenceMs = phaseFit.valid
        ? std::abs(phaseFit.samples - rawDelaySamples) * 1000.0 / fs
        : 0.0;

    const auto maxPhaseDiffMs = getMaxPhaseDiffMs();
    const bool phaseCheckApplied = maxPhaseDiffMs > 0.0
                                && phaseFit.valid
                                && phaseFit.quality >= 0.72
                                && meanCoherence >= 0.50;
    const bool phaseQualified = !phaseCheckApplied || phaseDifferenceMs <= maxPhaseDiffMs;

    int bandAgreeing = 0;
    int bandUsable = 0;
    double bandAgreement = 0.0;
    calculateBandPlausibility(rawDelaySamples,
                              firstBin,
                              lastBin,
                              fs,
                              bandAgreeing,
                              bandUsable,
                              bandAgreement);

    const auto spectralDiversity = calculateSpectralDiversity(firstBin, lastBin);
    const bool tonalAmbiguous = spectralDiversity < 0.18 && peakRatio < juce::jmax(1.25, minPeakRatio + 0.05);

    // Stability is calculated from otherwise plausible raw broadband estimates,
    // before the display smoothing. This makes it a real measurement-quality
    // metric rather than a measure of how much UI smoothing is applied.
    const bool preliminaryForStability = meanCoherence >= 0.25
                                      && peakRatio >= juce::jmax(1.02, minPeakRatio * 0.85)
                                      && phaseQualified
                                      && continuityQualified
                                      && !tonalAmbiguous;

    if (preliminaryForStability)
    {
        const auto frameSeconds = juce::jmax(1.0 / fs, static_cast<double>(hopSize) / fs);
        const auto stabilityLength = juce::jlimit(5, 60, static_cast<int>(std::lround(1.0 / frameSeconds)));
        stabilityHistory.push_back(rawDelaySamples);
        while (static_cast<int>(stabilityHistory.size()) > stabilityLength)
            stabilityHistory.pop_front();
    }

    const auto stabilitySamples = robustSigmaFromMad(stabilityHistory);
    const auto stabilityMs = stabilitySamples * 1000.0 / fs;
    const bool stabilityReady = stabilityHistory.size() >= 5;
    const auto maxStabilityMs = getMaxStabilityMs();
    const bool stabilityQualified = maxStabilityMs <= 0.0 || !stabilityReady || stabilityMs <= maxStabilityMs;

    // Soft scores used for the familiar Confidence percentage. Hard validity
    // still uses the explicit gates below, so the user remains in control.
    const auto peakScore = juce::jlimit(0.0, 1.0, (peakRatio - 1.0) / 0.40);
    const auto bandScore = bandUsable >= 2 ? juce::jlimit(0.0, 1.0, bandAgreement) : 0.70;
    const auto phaseScore = phaseCheckApplied ? (phaseQualified ? phaseFit.quality : 0.15 * phaseFit.quality) : 0.72;
    const auto stabilityScore = (stabilityReady && maxStabilityMs > 0.0)
        ? juce::jlimit(0.0, 1.0, 1.0 - stabilityMs / juce::jmax(0.05, 2.0 * maxStabilityMs))
        : 0.75;
    const auto warmup = juce::jlimit(0.0, 1.0, static_cast<double>(averageFrameCount - 1) / 4.0);

    const auto confidence = warmup * juce::jlimit(0.0, 1.0,
          0.55 * meanCoherence
        + 0.18 * peakScore
        + 0.08 * spectralDiversity
        + 0.07 * bandScore
        + 0.07 * phaseScore
        + 0.05 * stabilityScore);

    const auto minConfidence = getMinConfidence();
    const auto minCoherence = getMinCoherence();
    const bool confidenceQualified = confidence >= minConfidence;
    const bool coherenceQualified = meanCoherence >= minCoherence;
    const bool peakRatioQualified = peakRatio >= minPeakRatio;

    const bool qualityQualified = confidenceQualified
                               && coherenceQualified
                               && peakRatioQualified
                               && phaseQualified
                               && stabilityQualified
                               && continuityQualified
                               && !tonalAmbiguous;

    // Commit continuity lock only from a frame that has passed every robustness
    // check. A remote new delay therefore needs repeated evidence before it can
    // replace the previously trusted value.
    if (qualityQualified)
    {
        const bool switched = continuityLocked
                           && std::abs(rawDelaySamples - trackedDelaySamples) > continuityWindowSamples;

        if (switched)
        {
            resetStabilityTracking();

            // A confirmed continuity switch starts a fresh robust-target history,
            // but MUST NOT reset the displayed output smoother. Otherwise a
            // newly accepted remote GCC peak would jump straight to the screen
            // even when Delay Smoothing is set to e.g. 5 seconds.
            //
            // Keeping the one-pole smoother state makes the selected smoothing
            // time a true display/output time constant. Smooth = Off still
            // follows the new raw result immediately in smoothDelayEstimate().
            delayHistory.clear();
            phaseHistory.clear();
        }

        continuityLocked = true;
        trackedDelaySamples = rawDelaySamples;
        challengerCount = 0;
    }

    const auto smoothingSeconds = getSmoothingSeconds();
    if (std::abs(smoothingSeconds - lastSmoothingSeconds) > 1.0e-9)
    {
        delayHistory.clear();
        phaseHistory.clear();
        delaySmoothingInitialised = false;
        phaseSmoothingInitialised = false;
        lastSmoothingSeconds = smoothingSeconds;
    }

    double displayedDelaySamples = hasDelayEstimate ? smoothedDelaySamples : 0.0;
    double displayedPhaseSamples = hasDelayEstimate ? smoothedPhaseSamples : 0.0;

    if (qualityQualified)
    {
        displayedDelaySamples = smoothDelayEstimate(rawDelaySamples, confidence, smoothingSeconds, fs);
        const auto phaseValue = phaseFit.valid ? phaseFit.samples : rawDelaySamples;
        displayedPhaseSamples = smoothPhaseEstimate(phaseValue, confidence, smoothingSeconds, fs);
        hasDelayEstimate = true;
    }
    else if (hasDelayEstimate)
    {
        displayedDelaySamples = smoothedDelaySamples;
        displayedPhaseSamples = smoothedPhaseSamples;
    }

    AnalysisSnapshot newSnapshot;
    newSnapshot.valid = averageFrameCount >= 2 && (confidence > 0.01 || meanCoherence > 0.05);
    newSnapshot.delayQualified = qualityQualified && hasDelayEstimate;
    newSnapshot.hasDelayEstimate = hasDelayEstimate;
    newSnapshot.polarityInverted = selectedCandidate.signedValue < 0.0;

    newSnapshot.confidenceQualified = confidenceQualified;
    newSnapshot.coherenceQualified = coherenceQualified;
    newSnapshot.peakRatioQualified = peakRatioQualified;
    newSnapshot.phaseQualified = phaseQualified;
    newSnapshot.phaseCheckApplied = phaseCheckApplied;
    newSnapshot.stabilityQualified = stabilityQualified;
    newSnapshot.stabilityReady = stabilityReady;
    newSnapshot.continuityQualified = continuityQualified;
    newSnapshot.tonalAmbiguous = tonalAmbiguous;

    newSnapshot.sampleRate = fs;
    newSnapshot.fftSize = fftSize;
    newSnapshot.delaySamples = displayedDelaySamples;
    newSnapshot.delayMs = displayedDelaySamples * 1000.0 / fs;
    newSnapshot.rawDelaySamples = rawDelaySamples;
    newSnapshot.rawDelayMs = rawDelaySamples * 1000.0 / fs;
    newSnapshot.phaseFitSamples = displayedPhaseSamples;
    newSnapshot.phaseFitMs = displayedPhaseSamples * 1000.0 / fs;
    newSnapshot.phaseFitQuality = phaseFit.quality;
    newSnapshot.phaseDifferenceMs = phaseDifferenceMs;
    newSnapshot.confidence = confidence;
    newSnapshot.meanCoherence = meanCoherence;
    newSnapshot.peakRatio = peakRatio;
    newSnapshot.peakProminence = peakProminence;
    newSnapshot.stabilitySamples = stabilitySamples;
    newSnapshot.stabilityMs = stabilityMs;
    newSnapshot.spectralDiversity = spectralDiversity;
    newSnapshot.bandAgreeing = bandAgreeing;
    newSnapshot.bandUsable = bandUsable;
    newSnapshot.bandAgreement = bandAgreement;
    newSnapshot.continuityLocked = continuityLocked;
    newSnapshot.continuitySwitchPending = continuitySwitchPending;

    const int plotStride = juce::jmax(1, (lastBin - firstBin + 1) / 900);
    for (int k = firstBin; k <= lastBin; k += plotStride)
    {
        const auto idx = static_cast<size_t>(k);
        newSnapshot.frequencies.push_back(static_cast<float>(static_cast<double>(k) * fs / fftSize));
        newSnapshot.coherence.push_back(binCoherence[idx]);
        newSnapshot.phaseDegrees.push_back(juce::radiansToDegrees(std::arg(avgCross[idx])));
    }

    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        snapshot = std::move(newSnapshot);
    }
}
} // namespace signalalign
