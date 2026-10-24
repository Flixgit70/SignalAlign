#include "PluginProcessor.h"
#include "PluginEditor.h"

SignalAlignAudioProcessor::SignalAlignAudioProcessor()
    : AudioProcessor(BusesProperties()
#if SIGNALALIGN_16_INPUT
                         .withInput("Input", juce::AudioChannelSet::discreteChannels(16), true)
                         .withOutput("Output", juce::AudioChannelSet::discreteChannels(16), true)),
#else
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      analyser(apvts)
{
}

void SignalAlignAudioProcessor::prepareToPlay(double sampleRate, int)
{
    analyser.setSampleRate(sampleRate);
}

void SignalAlignAudioProcessor::releaseResources()
{
}

bool SignalAlignAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if SIGNALALIGN_16_INPUT
    return layouts.getMainInputChannelSet().size() == 16
        && layouts.getMainOutputChannelSet().size() == 16;
#else
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
#endif
}

void SignalAlignAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const auto* ref = buffer.getReadPointer(0);
    int compareChannelIndex = 1;
#if SIGNALALIGN_16_INPUT
    if (const auto* compareParam = apvts.getRawParameterValue(signalalign::ParamIDs::compareInput))
        compareChannelIndex = juce::jlimit(1, juce::jmin(15, buffer.getNumChannels() - 1),
                                           static_cast<int>(compareParam->load()) + 1);

    if (compareChannelIndex != lastCompareChannelIndex)
    {
        lastCompareChannelIndex = compareChannelIndex;
        analyser.requestReset();
    }
#endif
    const auto* meas = buffer.getReadPointer(compareChannelIndex);
    const auto n = buffer.getNumSamples();

    analyser.pushAudio(ref, meas, n);

    auto calcRmsDb = [n](const float* data)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += static_cast<double>(data[i]) * data[i];
        const auto rms = n > 0 ? std::sqrt(sum / n) : 0.0;
        return juce::Decibels::gainToDecibels(static_cast<float>(rms), -100.0f);
    };

    const auto refNow = calcRmsDb(ref);
    const auto measNow = calcRmsDb(meas);
    const auto smooth = 0.20f;
    refLevelDb.store(refLevelDb.load(std::memory_order_relaxed) + smooth * (refNow - refLevelDb.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    measLevelDb.store(measLevelDb.load(std::memory_order_relaxed) + smooth * (measNow - measLevelDb.load(std::memory_order_relaxed)), std::memory_order_relaxed);

    // Measurement-only plug-in: all audio channels are passed through unchanged.
}

juce::AudioProcessorEditor* SignalAlignAudioProcessor::createEditor()
{
    return new SignalAlignAudioProcessorEditor(*this);
}

void SignalAlignAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SignalAlignAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout SignalAlignAudioProcessor::createParameterLayout()
{
    using namespace signalalign;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::fftSize, 1 }, "FFT Size",
        juce::StringArray { "2048", "4096", "8192", "16384", "32768", "65536", "131072" }, 4));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::overlap, 1 }, "Overlap",
        juce::StringArray { "0 %", "50 %", "75 %", "87.5 %" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::averaging, 1 }, "Averaging",
        juce::StringArray { "Fast", "Medium", "Slow", "Very Slow" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::lowFreq, 1 }, "Low Frequency",
        juce::NormalisableRange<float>(20.0f, 5000.0f, 1.0f, 0.35f), 50.0f, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::highFreq, 1 }, "High Frequency",
        juce::NormalisableRange<float>(1000.0f, 22000.0f, 1.0f, 0.45f), 15000.0f, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::searchMs, 1 }, "Delay Search",
        juce::StringArray { "+/-5 ms", "+/-10 ms", "+/-25 ms", "+/-50 ms", "+/-100 ms", "+/-250 ms", "+/-500 ms", "+/-750 ms", "+/-1000 ms" }, 6));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::smoothing, 1 }, "Delay Smoothing",
        juce::StringArray { "Off", "0.25 s", "0.5 s", "1 s", "2 s", "5 s" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::minConfidence, 1 }, "Min Confidence",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 10.0f, "%"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::minCoherence, 1 }, "Min Coherence",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 15.0f, "%"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::minPeakRatio, 1 }, "Min Peak Ratio",
        juce::NormalisableRange<float>(1.0f, 3.0f, 0.05f), 1.00f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::maxPhaseDiffMs, 1 }, "Max GCC Phase Difference",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 0.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::maxStabilityMs, 1 }, "Max Delay Stability",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.05f), 0.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::delayGreenThresholdMs, 1 }, "Delay Green Threshold",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 1.9f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::delayRedThresholdMs, 1 }, "Delay Red Threshold",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 7.9f, "ms"));

#if SIGNALALIGN_16_INPUT
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::compareInput, 1 }, "Compare Input",
        juce::StringArray { "Input 2", "Input 3", "Input 4", "Input 5", "Input 6",
                            "Input 7", "Input 8", "Input 9", "Input 10", "Input 11",
                            "Input 12", "Input 13", "Input 14", "Input 15", "Input 16" }, 0));
#endif

    // Display-only precision settings. These are stored with the plug-in state so
    // each instance remembers the user's preferred number of decimal places.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::sampleDecimals, 1 }, "Sample Decimals",
        juce::StringArray { "0", "1", "2", "3", "4" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamIDs::msDecimals, 1 }, "Millisecond Decimals",
        juce::StringArray { "0", "1", "2", "3", "4" }, 2));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SignalAlignAudioProcessor();
}
