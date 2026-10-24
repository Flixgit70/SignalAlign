#pragma once

#include <JuceHeader.h>
#include "AnalysisEngine.h"

class SignalAlignAudioProcessor final : public juce::AudioProcessor
{
public:
    SignalAlignAudioProcessor();
    ~SignalAlignAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    signalalign::AnalysisEngine analyser;

    float getRefLevelDb() const noexcept { return refLevelDb.load(std::memory_order_relaxed); }
    float getMeasLevelDb() const noexcept { return measLevelDb.load(std::memory_order_relaxed); }

    int getSelectedCompareInputNumber() const noexcept
    {
#if SIGNALALIGN_16_INPUT
        if (const auto* value = apvts.getRawParameterValue(signalalign::ParamIDs::compareInput))
            return juce::jlimit(2, 16, static_cast<int>(value->load()) + 2);
#endif
        return 2;
    }

private:
    std::atomic<float> refLevelDb { -100.0f };
    std::atomic<float> measLevelDb { -100.0f };
#if SIGNALALIGN_16_INPUT
    int lastCompareChannelIndex = -1;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalAlignAudioProcessor)
};
