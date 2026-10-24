#pragma once

#include <JuceHeader.h>

namespace signalalign
{
struct AnalysisSnapshot
{
    bool valid = false;
    bool polarityInverted = false;
    bool delayQualified = false;
    bool hasDelayEstimate = false;

    bool confidenceQualified = false;
    bool coherenceQualified = false;
    bool peakRatioQualified = false;
    bool phaseQualified = true;
    bool phaseCheckApplied = false;
    bool stabilityQualified = true;
    bool stabilityReady = false;
    bool continuityQualified = true;
    bool tonalAmbiguous = false;

    double sampleRate = 48000.0;
    int fftSize = 65536;

    double delaySamples = 0.0;       // positive: selected COMPARE arrives later than REFERENCE
    double delayMs = 0.0;
    double rawDelaySamples = 0.0;
    double rawDelayMs = 0.0;

    double phaseFitSamples = 0.0;
    double phaseFitMs = 0.0;
    double phaseFitQuality = 0.0;    // 0..1
    double phaseDifferenceMs = 0.0;

    double confidence = 0.0;         // robust overall confidence, 0..1
    double meanCoherence = 0.0;      // 0..1
    double peakRatio = 1.0;          // strongest GCC peak / second independent peak
    double peakProminence = 0.0;     // 0..1

    double stabilitySamples = 0.0;   // robust sigma (MAD based)
    double stabilityMs = 0.0;
    double spectralDiversity = 0.0;  // 0..1, low values indicate tonal / sparse spectra

    int bandAgreeing = 0;
    int bandUsable = 0;
    double bandAgreement = 0.0;      // soft plausibility only, never a hard gate

    bool continuityLocked = false;
    bool continuitySwitchPending = false;

    std::vector<float> frequencies;
    std::vector<float> coherence;
    std::vector<float> phaseDegrees;
};

namespace ParamIDs
{
    inline constexpr auto fftSize = "fftSize";
    inline constexpr auto overlap = "overlap";
    inline constexpr auto averaging = "averaging";
    inline constexpr auto lowFreq = "lowFreq";
    inline constexpr auto highFreq = "highFreq";
    inline constexpr auto searchMs = "searchMs";
    inline constexpr auto smoothing = "smoothing";
    inline constexpr auto minConfidence = "minConfidence";
    inline constexpr auto minCoherence = "minCoherence";
    inline constexpr auto minPeakRatio = "minPeakRatio";
    inline constexpr auto maxPhaseDiffMs = "maxPhaseDiffMs";
    inline constexpr auto maxStabilityMs = "maxStabilityMs";
    inline constexpr auto delayGreenThresholdMs = "delayGreenThresholdMs";
    inline constexpr auto delayRedThresholdMs = "delayRedThresholdMs";
    inline constexpr auto sampleDecimals = "sampleDecimals";
    inline constexpr auto msDecimals = "msDecimals";
    inline constexpr auto compareInput = "compareInput";
}
} // namespace signalalign
