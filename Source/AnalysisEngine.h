#pragma once

#include <JuceHeader.h>
#include <deque>
#include "AnalysisTypes.h"

namespace signalalign
{
class AnalysisEngine final : private juce::Thread
{
public:
    explicit AnalysisEngine(juce::AudioProcessorValueTreeState& state);
    ~AnalysisEngine() override;

    void setSampleRate(double newSampleRate) noexcept;
    void pushAudio(const float* ref, const float* meas, int numSamples) noexcept;

    AnalysisSnapshot getSnapshot() const;

    void setFrozen(bool shouldFreeze) noexcept;
    bool isFrozen() const noexcept { return frozen.load(); }
    void requestReset() noexcept;

private:
    struct PeakCandidate
    {
        int lag = 0;
        double fractionalLag = 0.0;
        double value = 0.0;
        double signedValue = 0.0;
    };

    struct PhaseFitResult
    {
        double samples = 0.0;
        double quality = 0.0;
        bool valid = false;
    };

    void run() override;
    void reconfigureIfNeeded();
    void configureFFT(int newFftSize);
    void analyseFrame(const float* ref, const float* meas);

    int getRequestedFFTSize() const;
    double getOverlapFraction() const;
    float getAveragingAlpha() const;
    double getLowFrequency() const;
    double getHighFrequency() const;
    double getSearchMilliseconds() const;
    double getSmoothingSeconds() const;
    double getMinConfidence() const;
    double getMinCoherence() const;
    double getMinPeakRatio() const;
    double getMaxPhaseDiffMs() const;
    double getMaxStabilityMs() const;

    static double parabolicPeakOffset(double left, double centre, double right) noexcept;
    static double medianOf(std::deque<double> values);
    static double robustSigmaFromMad(const std::deque<double>& values);

    PeakCandidate refineCandidateAtLag(int lag, int maxLag) const;
    std::vector<PeakCandidate> collectPeakCandidates(int maxLag, int guardRadius) const;
    PeakCandidate selectContinuityCandidate(const std::vector<PeakCandidate>& candidates,
                                            const PeakCandidate& globalBest,
                                            double meanCoherence,
                                            double minPeakRatio,
                                            int continuityWindowSamples,
                                            bool& continuityQualified,
                                            bool& switchPending);
    PhaseFitResult calculateResidualPhaseFit(double candidateSamples,
                                             int firstBin,
                                             int lastBin,
                                             double fs) const;
    void calculateBandPlausibility(double candidateSamples,
                                   int firstBin,
                                   int lastBin,
                                   double fs,
                                   int& agreeing,
                                   int& usable,
                                   double& agreement) const;
    double calculateSpectralDiversity(int firstBin, int lastBin) const;

    double smoothDelayEstimate(double rawDelaySamples, double confidence, double smoothingSeconds, double fs);
    double smoothPhaseEstimate(double rawPhaseSamples, double confidence, double smoothingSeconds, double fs);
    void resetDelaySmoothing() noexcept;
    void resetContinuityTracking() noexcept;
    void resetStabilityTracking() noexcept;

    juce::AudioProcessorValueTreeState& parameters;
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<bool> frozen { false };
    std::atomic<bool> resetRequested { false };

    static constexpr int fifoCapacity = 1 << 19;
    juce::AbstractFifo fifo { fifoCapacity };
    std::array<float, fifoCapacity> fifoRef {};
    std::array<float, fifoCapacity> fifoMeas {};

    std::vector<float> pendingRef;
    std::vector<float> pendingMeas;

    int fftSize = 65536;
    int fftOrder = 16;
    int hopSize = 16384;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    std::vector<juce::dsp::Complex<float>> fftInRef;
    std::vector<juce::dsp::Complex<float>> fftInMeas;
    std::vector<juce::dsp::Complex<float>> fftOutRef;
    std::vector<juce::dsp::Complex<float>> fftOutMeas;
    std::vector<juce::dsp::Complex<float>> phatSpectrum;
    std::vector<juce::dsp::Complex<float>> correlation;

    std::vector<float> avgAutoRef;
    std::vector<float> avgAutoMeas;
    std::vector<juce::dsp::Complex<float>> avgCross;
    std::vector<float> binCoherence;
    std::vector<float> binEnergyWeight;
    std::vector<float> binQualityWeight;
    bool averagesInitialised = false;
    int averageFrameCount = 0;

    std::deque<double> delayHistory;
    std::deque<double> phaseHistory;
    std::deque<double> stabilityHistory;
    bool delaySmoothingInitialised = false;
    bool phaseSmoothingInitialised = false;
    double smoothedDelaySamples = 0.0;
    double smoothedPhaseSamples = 0.0;
    double lastSmoothingSeconds = -1.0;
    bool hasDelayEstimate = false;

    bool continuityLocked = false;
    double trackedDelaySamples = 0.0;
    double challengerDelaySamples = 0.0;
    int challengerCount = 0;

    mutable std::mutex snapshotMutex;
    AnalysisSnapshot snapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisEngine)
};
} // namespace signalalign
