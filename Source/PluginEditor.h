#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SignalAlignAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit SignalAlignAudioProcessorEditor(SignalAlignAudioProcessor&);
    ~SignalAlignAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showHelpMenu();
    void updateSettingsVisibility();
    void updateFftSizeTooltip();
    void showPrecisionMenu(const juce::String& parameterID, const juce::String& title, juce::Component* target);
#if SIGNALALIGN_16_INPUT
    void updateCompareInputButtons();
    void selectCompareInput(int inputNumber);
#endif

    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float levelDb, const juce::String& name) const;
    void drawCoherencePlot(juce::Graphics&, juce::Rectangle<float>) const;
    void drawPhasePlot(juce::Graphics&, juce::Rectangle<float>) const;
    void drawDelayBar(juce::Graphics&, juce::Rectangle<float>);
    void drawLogFrequencyGrid(juce::Graphics&, juce::Rectangle<float>, bool phasePlot) const;
    float frequencyToX(float frequency, juce::Rectangle<float> bounds) const;

    SignalAlignAudioProcessor& audioProcessor;
    juce::TooltipWindow tooltipWindow;
    double lastTooltipSampleRate = -1.0;
    signalalign::AnalysisSnapshot latest;
    juce::Rectangle<int> delayBarBounds;
    juce::Rectangle<int> sampleValueBounds;
    juce::Rectangle<int> msValueBounds;
    juce::Image companyLogo;
    bool settingsVisible = true;
    bool advancedVisible = false;

    juce::ComboBox fftSizeBox;
    juce::ComboBox overlapBox;
    juce::ComboBox averagingBox;
    juce::ComboBox searchBox;
    juce::ComboBox smoothingBox;
    juce::Slider lowFreqSlider;
    juce::Slider highFreqSlider;
    juce::Slider minConfidenceSlider;
    juce::Slider minCoherenceSlider;
    juce::Slider minPeakRatioSlider;
    juce::Slider maxPhaseDiffSlider;
    juce::Slider maxStabilitySlider;

    juce::Label fftLabel;
    juce::Label overlapLabel;
    juce::Label averagingLabel;
    juce::Label searchLabel;
    juce::Label smoothingLabel;
    juce::Label lowLabel;
    juce::Label highLabel;
    juce::Label minConfidenceLabel;
    juce::Label minCoherenceLabel;
    juce::Label minPeakRatioLabel;
    juce::Label maxPhaseDiffLabel;
    juce::Label maxStabilityLabel;

    juce::TextButton logoButton;
    juce::TextButton settingsToggleButton { "HIDE SETTINGS" };
    juce::TextButton advancedToggleButton { "ADVANCED" };
#if SIGNALALIGN_16_INPUT
    std::array<std::unique_ptr<juce::Button>, 15> compareInputButtons;
#endif
    juce::ToggleButton freezeButton { "FREEZE" };

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAttachment> fftAttachment;
    std::unique_ptr<ComboAttachment> overlapAttachment;
    std::unique_ptr<ComboAttachment> averagingAttachment;
    std::unique_ptr<ComboAttachment> searchAttachment;
    std::unique_ptr<ComboAttachment> smoothingAttachment;
    std::unique_ptr<SliderAttachment> lowAttachment;
    std::unique_ptr<SliderAttachment> highAttachment;
    std::unique_ptr<SliderAttachment> minConfidenceAttachment;
    std::unique_ptr<SliderAttachment> minCoherenceAttachment;
    std::unique_ptr<SliderAttachment> minPeakRatioAttachment;
    std::unique_ptr<SliderAttachment> maxPhaseDiffAttachment;
    std::unique_ptr<SliderAttachment> maxStabilityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalAlignAudioProcessorEditor)
};
