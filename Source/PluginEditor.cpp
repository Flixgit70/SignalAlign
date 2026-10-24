#include "PluginEditor.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
// Shared visual language with COMISRatio: neutral #202020 chrome, black
// modules, grey typography and the same broadcast blue/green/yellow/red
// accents. This is UI-only; analysis/routing are unchanged.
const juce::Colour bg = juce::Colour::fromRGB(32, 32, 32);
const juce::Colour panel = juce::Colour::fromRGB(16, 16, 16);
const juce::Colour panelRaised = juce::Colour::fromRGB(24, 24, 24);
const juce::Colour grid = juce::Colour::fromRGB(60, 60, 60);
const juce::Colour uiText = juce::Colour::fromRGB(200, 200, 200);
const juce::Colour dimText = juce::Colour::fromRGB(200, 200, 200);
const juce::Colour primaryBlue = juce::Colour::fromRGB(0, 190, 255);
const juce::Colour accent = juce::Colour::fromRGB(253, 245, 81);
const juce::Colour coherenceColour = juce::Colour::fromRGB(0, 255, 120);
const juce::Colour phaseColour = primaryBlue;
const juce::Colour delayOrange = juce::Colour::fromRGB(255, 170, 64);
const juce::Colour delayRed = juce::Colour::fromRGB(255, 40, 40);
const juce::Colour buttonBackground = juce::Colour::fromRGB(70, 70, 70);
const juce::Colour moduleOutline = uiText.withAlpha(0.34f);

#if SIGNALALIGN_16_INPUT
class CompareRoundButton final : public juce::Button
{
public:
    explicit CompareRoundButton(int inputNumber)
        : juce::Button("CompareInput" + juce::String(inputNumber))
    {
        setButtonText(juce::String(inputNumber));
        setWantsKeyboardFocus(false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto fill = getToggleState()
            ? primaryBlue
            : buttonBackground;

        if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter(0.12f);
        if (shouldDrawButtonAsDown)
            fill = fill.darker(0.12f);

        g.setColour(fill);
        g.fillEllipse(bounds);

        g.setColour(getToggleState() ? juce::Colours::black : uiText);
        g.setFont(juce::Font(juce::FontOptions(juce::jlimit(11.0f, 15.0f, bounds.getHeight() * 0.48f))).boldened());
        g.drawFittedText(getButtonText(), getLocalBounds(), juce::Justification::centred, 1);
    }
};
#endif

void styleCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, panelRaised);
    box.setColour(juce::ComboBox::textColourId, uiText);
    box.setColour(juce::ComboBox::outlineColourId, grid);
    box.setColour(juce::ComboBox::arrowColourId, uiText);
}

void populateChoiceBox(juce::ComboBox& box,
                       juce::AudioProcessorValueTreeState& state,
                       const juce::String& parameterID)
{
    box.clear(juce::dontSendNotification);

    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterID)))
    {
        for (int i = 0; i < parameter->choices.size(); ++i)
            box.addItem(parameter->choices[i], i + 1);
    }
}

void styleLabel(juce::Label& label, const juce::String& value)
{
    label.setText(value, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, dimText);
    label.setFont(juce::Font(juce::FontOptions(12.0f)));
}

struct ResponsiveLayout
{
    bool compact = false;
    float uiScale = 1.0f;
    float margin = 20.0f;
    float headerHeight = 58.0f;
    float topPanelY = 64.0f;
    float topPanelHeight = 240.0f;
    float settingsButtonY = 314.0f;
    float settingsButtonHeight = 28.0f;
    float controlsTop = 350.0f;
    float controlsHeight = 252.0f;
    float plotTop = 618.0f;
    float plotGap = 14.0f;
    float plotHeight = 120.0f;
};

ResponsiveLayout makeResponsiveLayout(int width, int height, bool settingsVisible, bool advancedVisible)
{
    ResponsiveLayout l;
    const auto W = static_cast<float>(width);
    const auto H = static_cast<float>(height);

    l.compact = width < 940;
    l.uiScale = juce::jlimit(0.78f, 1.35f,
                             juce::jmin(W / 1120.0f, H / 900.0f));
    l.margin = juce::jlimit(12.0f, 22.0f, W * 0.018f);
    l.headerHeight = juce::jlimit(52.0f, 70.0f, H * 0.07f);
    l.topPanelY = l.headerHeight + 6.0f;
    // Height becomes much more aggressive in compact windows. The previous
    // fixed minima could make the second plot extend below the component when
    // the host zoomed the editor down. Keep the complete Coherence + Phase
    // Y ranges visible instead of clipping the lower plot.
    const auto heightScale = juce::jlimit(0.58f, 1.0f, H / 900.0f);

    l.topPanelHeight = juce::jlimit(l.compact ? 154.0f : 190.0f,
                                    l.compact ? 248.0f : 268.0f,
                                    H * (l.compact ? 0.285f : 0.275f));
    l.settingsButtonHeight = juce::jlimit(22.0f, 34.0f, 28.0f * heightScale);
    l.settingsButtonY = l.topPanelY + l.topPanelHeight + juce::jlimit(5.0f, 10.0f, 10.0f * heightScale);
    l.controlsTop = l.settingsButtonY + l.settingsButtonHeight + juce::jlimit(4.0f, 8.0f, 8.0f * heightScale);
    // Normal SETTINGS only contains the controls used in day-to-day work.
    // ADVANCED expands the panel with the quality/robustness gates.
    l.controlsHeight = settingsVisible
        ? (advancedVisible
            ? (l.compact
                ? juce::jlimit(186.0f, 232.0f, 218.0f * heightScale)
                : juce::jlimit(184.0f, 248.0f, 226.0f * heightScale))
            : (l.compact
                ? juce::jlimit(106.0f, 142.0f, 126.0f * heightScale)
                : juce::jlimit(104.0f, 142.0f, 124.0f * heightScale)))
        : 0.0f;

    l.plotTop = settingsVisible
        ? l.controlsTop + l.controlsHeight + juce::jlimit(6.0f, 14.0f, 14.0f * heightScale)
        : l.settingsButtonY + l.settingsButtonHeight + juce::jlimit(5.0f, 12.0f, 12.0f * heightScale);

    l.plotGap = juce::jlimit(3.0f, l.compact ? 10.0f : 14.0f, 10.0f * heightScale);

    // IMPORTANT: use the *real* remaining height. Never invent a minimum
    // available height, because that is exactly what caused bottom clipping.
    const auto availablePlotHeight = juce::jmax(58.0f, H - l.plotTop - l.margin);
    l.plotHeight = juce::jmax(27.0f, (availablePlotHeight - l.plotGap) * 0.5f);
    return l;
}

class DelayThresholdEditor final : public juce::Component
{
public:
    explicit DelayThresholdEditor(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
        setSize(390, 150);

        title.setText("DELAY BAR THRESHOLDS", juce::dontSendNotification);
        title.setColour(juce::Label::textColourId, uiText);
        title.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        addAndMakeVisible(title);

        info.setText("Green <= first limit | Orange until second limit | Red above", juce::dontSendNotification);
        info.setColour(juce::Label::textColourId, dimText);
        info.setFont(juce::Font(juce::FontOptions(11.0f)));
        addAndMakeVisible(info);

        configureLabel(greenLabel, "GREEN MAX", coherenceColour);
        configureLabel(redLabel, "RED FROM", delayRed);
        configureSlider(greenSlider, coherenceColour);
        configureSlider(redSlider, delayRed);

        greenSlider.onValueChange = [this]
        {
            if (greenSlider.getValue() > redSlider.getValue())
                redSlider.setValue(greenSlider.getValue(), juce::sendNotificationSync);
        };

        redSlider.onValueChange = [this]
        {
            if (redSlider.getValue() < greenSlider.getValue())
                greenSlider.setValue(redSlider.getValue(), juce::sendNotificationSync);
        };

        greenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, signalalign::ParamIDs::delayGreenThresholdMs, greenSlider);
        redAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, signalalign::ParamIDs::delayRedThresholdMs, redSlider);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel);
    }

    void resized() override
    {
        title.setBounds(14, 8, getWidth() - 28, 22);
        info.setBounds(14, 29, getWidth() - 28, 18);
        greenLabel.setBounds(14, 60, 90, 22);
        greenSlider.setBounds(102, 54, getWidth() - 116, 36);
        redLabel.setBounds(14, 105, 90, 22);
        redSlider.setBounds(102, 99, getWidth() - 116, 36);
    }

private:
    void configureLabel(juce::Label& label, const juce::String& labelText, juce::Colour labelColour)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, labelColour);
        label.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        addAndMakeVisible(label);
    }

    void configureSlider(juce::Slider& slider, juce::Colour sliderColour)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 24);
        slider.setRange(0.0, 20.0, 0.1);
        slider.setNumDecimalPlacesToDisplay(1);
        slider.setTextValueSuffix(" ms");
        slider.setColour(juce::Slider::trackColourId, sliderColour);
        slider.setColour(juce::Slider::backgroundColourId, grid);
        slider.setColour(juce::Slider::textBoxTextColourId, uiText);
        slider.setColour(juce::Slider::textBoxOutlineColourId, grid);
        addAndMakeVisible(slider);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::Label title;
    juce::Label info;
    juce::Label greenLabel;
    juce::Label redLabel;
    juce::Slider greenSlider;
    juce::Slider redSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> greenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> redAttachment;
};
}

SignalAlignAudioProcessorEditor::SignalAlignAudioProcessorEditor(SignalAlignAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), tooltipWindow(this, 650)
{
    setSize(1120, 900);
    setResizable(true, true);
    setResizeLimits(700, 560, 2200, 1500);

    companyLogo = juce::ImageFileFormat::loadFrom(
        BinaryData::K2_logo_png,
        BinaryData::K2_logo_pngSize);

    for (auto* box : { &fftSizeBox, &overlapBox, &averagingBox, &searchBox, &smoothingBox })
    {
        addAndMakeVisible(*box);
        styleCombo(*box);
    }

    populateChoiceBox(fftSizeBox, audioProcessor.apvts, signalalign::ParamIDs::fftSize);
    populateChoiceBox(overlapBox, audioProcessor.apvts, signalalign::ParamIDs::overlap);
    populateChoiceBox(averagingBox, audioProcessor.apvts, signalalign::ParamIDs::averaging);
    populateChoiceBox(searchBox, audioProcessor.apvts, signalalign::ParamIDs::searchMs);
    populateChoiceBox(smoothingBox, audioProcessor.apvts, signalalign::ParamIDs::smoothing);

    updateFftSizeTooltip();

    for (auto* slider : { &lowFreqSlider, &highFreqSlider, &minConfidenceSlider, &minCoherenceSlider,
                          &minPeakRatioSlider, &maxPhaseDiffSlider, &maxStabilitySlider })
    {
        addAndMakeVisible(*slider);
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 20);
        slider->setColour(juce::Slider::trackColourId, primaryBlue);
        slider->setColour(juce::Slider::backgroundColourId, grid);
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, panelRaised);
        slider->setColour(juce::Slider::textBoxTextColourId, uiText);
        slider->setColour(juce::Slider::textBoxOutlineColourId, grid);
        slider->setNumDecimalPlacesToDisplay(0);
    }
    lowFreqSlider.setTextValueSuffix(" Hz");
    highFreqSlider.setTextValueSuffix(" Hz");
    minConfidenceSlider.setTextValueSuffix(" %");
    minCoherenceSlider.setTextValueSuffix(" %");
    minPeakRatioSlider.setNumDecimalPlacesToDisplay(2);
    maxPhaseDiffSlider.setNumDecimalPlacesToDisplay(1);
    maxStabilitySlider.setNumDecimalPlacesToDisplay(2);
    maxPhaseDiffSlider.setTextValueSuffix(" ms");
    maxStabilitySlider.setTextValueSuffix(" ms");

    styleLabel(fftLabel, "FFT SIZE");
    styleLabel(overlapLabel, "OVERLAP");
    styleLabel(averagingLabel, "AVERAGING");
    styleLabel(searchLabel, "SEARCH");
    styleLabel(smoothingLabel, "SMOOTH");
    styleLabel(lowLabel, "LOW");
    styleLabel(highLabel, "HIGH");
    styleLabel(minConfidenceLabel, "MIN CONFIDENCE");
    styleLabel(minCoherenceLabel, "MIN COHERENCE");
    styleLabel(minPeakRatioLabel, "MIN PEAK RATIO");
    styleLabel(maxPhaseDiffLabel, "MAX PHASE DIFF");
    styleLabel(maxStabilityLabel, "MAX STABILITY");

    for (auto* label : { &fftLabel, &overlapLabel, &averagingLabel, &searchLabel, &smoothingLabel, &lowLabel, &highLabel,
                         &minConfidenceLabel, &minCoherenceLabel, &minPeakRatioLabel, &maxPhaseDiffLabel, &maxStabilityLabel })
        addAndMakeVisible(*label);

    addAndMakeVisible(logoButton);
    logoButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    logoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    logoButton.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    logoButton.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    logoButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    logoButton.setTooltip("Click the K2 logo / SignalAlign wordmark for parameter help");
    logoButton.onClick = [this] { showHelpMenu(); };

    addAndMakeVisible(settingsToggleButton);
    settingsToggleButton.setClickingTogglesState(true);
    settingsToggleButton.setToggleState(true, juce::dontSendNotification);
    settingsToggleButton.setColour(juce::TextButton::buttonColourId, buttonBackground);
    settingsToggleButton.setColour(juce::TextButton::buttonOnColourId, primaryBlue);
    settingsToggleButton.setColour(juce::TextButton::textColourOffId, uiText);
    settingsToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    settingsToggleButton.setTooltip("Show or hide the day-to-day SignalAlign settings");
    settingsToggleButton.onClick = [this]
    {
        settingsVisible = settingsToggleButton.getToggleState();

        // Closing SETTINGS also closes ADVANCED so the next opening starts
        // with the clean day-to-day view.
        if (! settingsVisible)
        {
            advancedVisible = false;
            advancedToggleButton.setToggleState(false, juce::dontSendNotification);
        }

        updateSettingsVisibility();
        resized();
        repaint();
    };

    addAndMakeVisible(advancedToggleButton);
    advancedToggleButton.setClickingTogglesState(true);
    advancedToggleButton.setToggleState(false, juce::dontSendNotification);
    advancedToggleButton.setColour(juce::TextButton::buttonColourId, buttonBackground);
    advancedToggleButton.setColour(juce::TextButton::buttonOnColourId, primaryBlue);
    advancedToggleButton.setColour(juce::TextButton::textColourOffId, uiText);
    advancedToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    advancedToggleButton.setTooltip("Show the less frequently used quality and robustness controls");
    advancedToggleButton.onClick = [this]
    {
        advancedVisible = advancedToggleButton.getToggleState();

        // ADVANCED is a sub-view of SETTINGS. Opening it while SETTINGS is
        // collapsed automatically opens the normal settings panel as well.
        if (advancedVisible && ! settingsVisible)
        {
            settingsVisible = true;
            settingsToggleButton.setToggleState(true, juce::dontSendNotification);
        }

        updateSettingsVisibility();
        resized();
        repaint();
    };

#if SIGNALALIGN_16_INPUT
    for (int i = 0; i < static_cast<int>(compareInputButtons.size()); ++i)
    {
        const int inputNumber = i + 2;
        auto button = std::make_unique<CompareRoundButton>(inputNumber);
        button->setTooltip("Select Input " + juce::String(inputNumber)
                           + " as COMPARE. Input 1 is always REFERENCE.");
        button->onClick = [this, inputNumber] { selectCompareInput(inputNumber); };
        addAndMakeVisible(*button);
        compareInputButtons[static_cast<size_t>(i)] = std::move(button);
    }
    updateCompareInputButtons();
#endif

    addAndMakeVisible(freezeButton);
    freezeButton.setColour(juce::ToggleButton::textColourId, uiText);
    freezeButton.setColour(juce::ToggleButton::tickColourId, primaryBlue);
    freezeButton.setColour(juce::ToggleButton::tickDisabledColourId, dimText.withAlpha(0.45f));

    freezeButton.onClick = [this]
    {
        audioProcessor.analyser.setFrozen(freezeButton.getToggleState());
    };

    fftAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, signalalign::ParamIDs::fftSize, fftSizeBox);
    overlapAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, signalalign::ParamIDs::overlap, overlapBox);
    averagingAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, signalalign::ParamIDs::averaging, averagingBox);
    searchAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, signalalign::ParamIDs::searchMs, searchBox);
    smoothingAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, signalalign::ParamIDs::smoothing, smoothingBox);
    lowAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::lowFreq, lowFreqSlider);
    highAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::highFreq, highFreqSlider);
    minConfidenceAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::minConfidence, minConfidenceSlider);
    minCoherenceAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::minCoherence, minCoherenceSlider);
    minPeakRatioAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::minPeakRatio, minPeakRatioSlider);
    maxPhaseDiffAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::maxPhaseDiffMs, maxPhaseDiffSlider);
    maxStabilityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, signalalign::ParamIDs::maxStabilityMs, maxStabilitySlider);

    updateSettingsVisibility();
    startTimerHz(30);
}

void SignalAlignAudioProcessorEditor::updateSettingsVisibility()
{
    settingsToggleButton.setButtonText(settingsVisible ? "HIDE SETTINGS" : "SHOW SETTINGS");
    settingsToggleButton.setToggleState(settingsVisible, juce::dontSendNotification);

    advancedToggleButton.setButtonText(advancedVisible ? "HIDE ADVANCED" : "ADVANCED");
    advancedToggleButton.setToggleState(advancedVisible, juce::dontSendNotification);

    const bool basicVisible = settingsVisible;
    const bool advancedControlsVisible = settingsVisible && advancedVisible;

    // Normal day-to-day controls.
    for (auto* box : { &fftSizeBox, &searchBox, &smoothingBox })
        box->setVisible(basicVisible);

    for (auto* slider : { &lowFreqSlider, &highFreqSlider })
        slider->setVisible(basicVisible);

    for (auto* label : { &fftLabel, &searchLabel, &smoothingLabel, &lowLabel, &highLabel })
        label->setVisible(basicVisible);

    freezeButton.setVisible(basicVisible);

    // Engineering / troubleshooting controls.
    for (auto* box : { &overlapBox, &averagingBox })
        box->setVisible(advancedControlsVisible);

    for (auto* slider : { &minConfidenceSlider, &minCoherenceSlider,
                          &minPeakRatioSlider, &maxPhaseDiffSlider, &maxStabilitySlider })
        slider->setVisible(advancedControlsVisible);

    for (auto* label : { &overlapLabel, &averagingLabel,
                         &minConfidenceLabel, &minCoherenceLabel,
                         &minPeakRatioLabel, &maxPhaseDiffLabel, &maxStabilityLabel })
        label->setVisible(advancedControlsVisible);
}

void SignalAlignAudioProcessorEditor::timerCallback()
{
    latest = audioProcessor.analyser.getSnapshot();

    const auto currentSampleRate = audioProcessor.getSampleRate();
    if (std::abs(currentSampleRate - lastTooltipSampleRate) > 0.5)
        updateFftSizeTooltip();

#if SIGNALALIGN_16_INPUT
    updateCompareInputButtons();
#endif
    repaint();
}

void SignalAlignAudioProcessorEditor::updateFftSizeTooltip()
{
    const auto sampleRate = audioProcessor.getSampleRate();

    juce::String info;
    if (sampleRate > 1.0)
    {
        lastTooltipSampleRate = sampleRate;
        info << "Maximum unambiguous GCC delay at "
             << juce::String(sampleRate / 1000.0, 1) << " kHz:\n";

        static constexpr int sizes[] { 2048, 4096, 8192, 16384, 32768, 65536, 131072 };
        for (const auto size : sizes)
        {
            const auto maxDelayMs = 1000.0 * (static_cast<double>(size) * 0.5 - 2.0) / sampleRate;
            info << juce::String(size).paddedLeft(' ', 6)
                 << " : +/-" << juce::String(maxDelayMs, 1) << " ms\n";
        }

        info << "Actual usable range = smaller of FFT limit and SEARCH.";
    }
    else
    {
        lastTooltipSampleRate = sampleRate;
        info = "Maximum GCC delay ~= (FFT size / 2 - 2) / sample rate.\n"
               "Start audio to show the values for the current host sample rate.";
    }

    fftSizeBox.setTooltip(info);
}

#if SIGNALALIGN_16_INPUT
void SignalAlignAudioProcessorEditor::updateCompareInputButtons()
{
    const auto currentInput = audioProcessor.getSelectedCompareInputNumber();

    for (int i = 0; i < static_cast<int>(compareInputButtons.size()); ++i)
    {
        if (auto* button = compareInputButtons[static_cast<size_t>(i)].get())
            button->setToggleState((i + 2) == currentInput, juce::dontSendNotification);
    }
}

void SignalAlignAudioProcessorEditor::selectCompareInput(int inputNumber)
{
    inputNumber = juce::jlimit(2, 16, inputNumber);

    if (auto* parameter = audioProcessor.apvts.getParameter(signalalign::ParamIDs::compareInput))
    {
        const auto choiceIndex = static_cast<float>(inputNumber - 2);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(choiceIndex));
        parameter->endChangeGesture();
        audioProcessor.analyser.requestReset();
        updateCompareInputButtons();
        repaint();
    }
}
#endif

void SignalAlignAudioProcessorEditor::showHelpMenu()
{
    juce::PopupMenu analysisMenu;
    analysisMenu.addSectionHeader("Spectral analysis");
    analysisMenu.addItem(101, "FFT Size: frequency resolution and analysis window length");
    analysisMenu.addItem(102, "Overlap: how much consecutive FFT windows overlap");
    analysisMenu.addItem(103, "Averaging: smoothing of coherence and phase spectra");
    analysisMenu.addItem(104, "Low / High: frequency band used for delay evaluation");
    analysisMenu.addItem(105, "Search: maximum +/- delay range searched by Generalized Cross-Correlation with Phase Transform (GCC-PHAT)");
    analysisMenu.addItem(106, "Smooth: time smoothing after a delay frame has passed all quality checks");
    analysisMenu.addItem(107, "FFT Size hover: shows the maximum unambiguous GCC delay for every FFT size at the current host sample rate");

    juce::PopupMenu robustMenu;
    robustMenu.addSectionHeader("Robust delay logic");
    robustMenu.addItem(150, "GCC = Generalized Cross-Correlation: searches for the time offset where REFERENCE and COMPARE match best");
    robustMenu.addItem(160, "PHAT = Phase Transform: normalizes cross-spectrum magnitude so phase information dominates the delay search");
    robustMenu.addItem(151, "Weighted GCC-PHAT: broadband Generalized Cross-Correlation with Phase Transform remains the master delay estimator");
    robustMenu.addItem(152, "Coherence weighting: unreliable frequency bins contribute less to GCC");
    robustMenu.addItem(153, "Energy weighting: very weak bins are rejected without letting loud tones dominate");
    robustMenu.addItem(154, "Peak Ratio: compares the strongest GCC peak with the next independent peak");
    robustMenu.addItem(155, "Continuity Tracking: a remote new peak needs repeated evidence before replacing the stable delay");
    robustMenu.addItem(156, "Phase Check: adjacent-bin phase slope independently checks the broadband GCC delay");
    robustMenu.addItem(157, "Stability: robust variation of accepted raw delay estimates over roughly one second");
    robustMenu.addItem(158, "Band Check: frequency bands only test plausibility; they never choose the final delay");
    robustMenu.addItem(159, "Tonality rejection: sparse / strongly periodic spectra are marked ambiguous");

    juce::PopupMenu qualityMenu;
    qualityMenu.addSectionHeader("Measurement quality");
    qualityMenu.addItem(201, "Confidence: combined robust reliability score");
    qualityMenu.addItem(202, "Coherence: similarity / linear relation of REFERENCE and COMPARE");
    qualityMenu.addItem(203, "Min Confidence: delay is hidden below this value");
    qualityMenu.addItem(204, "Min Coherence: delay is hidden below this value");
    qualityMenu.addItem(205, "Min Peak Ratio: delay is hidden when competing GCC peaks are too similar");
    qualityMenu.addItem(206, "Max Phase Diff: maximum allowed GCC-versus-phase delay difference; 0 disables the gate");
    qualityMenu.addItem(207, "Max Stability: maximum allowed raw delay jitter; 0 disables the gate");
    qualityMenu.addItem(208, "Rejected frames never enter delay smoothing");

    juce::PopupMenu displayMenu;
    displayMenu.addSectionHeader("Display and controls");
    displayMenu.addItem(301, "Delay: positive = COMPARE arrives later than REFERENCE");
    displayMenu.addItem(302, "Phase Fit: independent delay estimate from adjacent-bin phase slope");
    displayMenu.addItem(303, "Peak Ratio: strongest independent GCC peak divided by second strongest");
    displayMenu.addItem(304, "Stability: MAD-based robust sigma in milliseconds");
    displayMenu.addItem(305, "Band Check: agreeing / usable frequency bands, soft information only");
    displayMenu.addItem(306, "Continuous measurement: delay analysis always runs whenever FREEZE is off");
    displayMenu.addItem(307, "FREEZE: holds the current analysis display");
#if SIGNALALIGN_16_INPUT
    displayMenu.addItem(308, "REFERENCE = Input 1; COMPARE = exactly one selected input from Inputs 2-16");
    displayMenu.addItem(315, "COMPARE selector: 15 round buttons select exactly one of Inputs 2-16; changing input resets analysis history and reacquires the delay");
#else
    displayMenu.addItem(308, "REFERENCE = left input channel, COMPARE = right input channel");
#endif
    displayMenu.addItem(309, "Alignment Bar: signed delay around 0 ms; right-click sets green/red limits");
    displayMenu.addItem(310, "SHOW/HIDE SETTINGS: shows the day-to-day controls (FFT, Search, Smooth, Low/High and Freeze)");
    displayMenu.addItem(316, "ADVANCED: shows Overlap, Averaging and the quality / robustness gates");
    displayMenu.addItem(311, "Responsive GUI: controls reflow in narrow windows and plots expand with available space");
    displayMenu.addItem(312, "Last Valid: if no current delay qualifies, the last valid delay remains visible in grey");
    displayMenu.addItem(313, "Correction arrows: large green right arrow = increase delay, large red left arrow = reduce delay; REFERENCE and COMPARE always show opposite corrections");
    displayMenu.addItem(314, "Display precision: right-click the Samples or ms readout to choose 0-4 decimal places");

    juce::PopupMenu menu;
    const auto versionString = juce::String(JucePlugin_VersionString);
    menu.addSectionHeader(juce::String(JucePlugin_Name) + " " + versionString + " ROBUST - Help");
    menu.addSubMenu("Analysis parameters", analysisMenu);
    menu.addSubMenu("Robust delay logic", robustMenu);
    menu.addSubMenu("Quality / validity", qualityMenu);
    menu.addSubMenu("Display / controls", displayMenu);
    menu.addSeparator();
    menu.addItem(403, "About " + juce::String(JucePlugin_Name) + "...");
    menu.addItem(402, "Open Manual...");
    menu.addItem(401, "Measurement only: audio passes through unchanged");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&logoButton),
                       [versionString](int result)
                       {
                           if (result == 402)
                           {
                               juce::URL("https://ogy.de/l6xz").launchInDefaultBrowser();
                           }
                           else if (result == 403)
                           {
                               juce::AlertWindow::showMessageBoxAsync(
                                   juce::MessageBoxIconType::InfoIcon,
                                   "About " + juce::String(JucePlugin_Name),
                                   juce::String(JucePlugin_Name) + "\nVersion " + versionString
#if SIGNALALIGN_16_INPUT
                                       + "\n\n16-input analyser: Input 1 is REFERENCE, one of Inputs 2-16 is the selected COMPARE.");
#else
                                       + "\n\nRobust dual-channel coherence, phase and time-alignment analyser.");
#endif
                           }
                       });
}

void SignalAlignAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);

    const auto W = static_cast<float>(getWidth());
    const auto layout = makeResponsiveLayout(getWidth(), getHeight(), settingsVisible, advancedVisible);
    const auto s = layout.uiScale;
    const auto margin = layout.margin;

    const auto logoSize = juce::jlimit(36.0f, 56.0f, 44.0f * s);
    const auto logoY = (layout.headerHeight - logoSize) * 0.5f;
    if (companyLogo.isValid())
        g.drawImageWithin(companyLogo,
                          static_cast<int>(margin), static_cast<int>(logoY),
                          static_cast<int>(logoSize), static_cast<int>(logoSize),
                          juce::RectanglePlacement::centred, false);

    const auto titleX = margin + logoSize + 8.0f * s;
    const auto titleW = juce::jlimit(190.0f, 350.0f, 270.0f * s);
    const auto wordmarkSize = juce::jlimit(24.0f, 42.0f, 31.0f * s);
    const auto wordmarkBounds = juce::Rectangle<int>(
        static_cast<int>(titleX), 0,
        static_cast<int>(titleW), static_cast<int>(layout.headerHeight));

    // Same wordmark treatment as COMISRatio: bold italic, soft black shadow,
    // broadcast-blue face and a subtle white top highlight.
    g.setFont(juce::Font(juce::FontOptions(wordmarkSize, juce::Font::bold | juce::Font::italic)));
    const juce::String wordmarkText("SignalAlign");
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawFittedText(wordmarkText, wordmarkBounds.translated(2, 2),
                     juce::Justification::centredLeft, 1);
    g.setColour(primaryBlue.brighter(0.30f));
    g.drawFittedText(wordmarkText, wordmarkBounds,
                     juce::Justification::centredLeft, 1);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawFittedText(wordmarkText, wordmarkBounds.translated(0, -1),
                     juce::Justification::centredLeft, 1);

#if SIGNALALIGN_16_INPUT
    // Small family badge; the core wordmark stays identical between variants.
    const auto badgeW = juce::jlimit(48.0f, 72.0f, 58.0f * s);
    const auto badgeH = juce::jlimit(16.0f, 23.0f, 19.0f * s);
    const auto badgeX = titleX + juce::jlimit(145.0f, 230.0f, 182.0f * s);
    const auto badgeY = (layout.headerHeight - badgeH) * 0.5f;
    const auto badge = juce::Rectangle<float>(badgeX, badgeY, badgeW, badgeH);
    g.setColour(primaryBlue.withAlpha(0.18f));
    g.fillRoundedRectangle(badge, badgeH * 0.48f);
    g.setColour(primaryBlue.withAlpha(0.75f));
    g.drawRoundedRectangle(badge, badgeH * 0.48f, 1.0f);
    g.setColour(uiText);
    g.setFont(juce::Font(juce::FontOptions(juce::jlimit(8.0f, 11.0f, 9.2f * s))).boldened());
    g.drawFittedText("16 INPUT", badge.toNearestInt(), juce::Justification::centred, 1);
#endif

    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions(juce::jlimit(10.0f, 15.0f, 12.5f * s))));
    const auto taglineX = titleX + titleW + 8.0f;
    const auto taglineW = W - margin - taglineX;
#if SIGNALALIGN_16_INPUT
    const juce::String tagline = layout.compact
        ? "IN 1 = REFERENCE   |   SELECTED IN = COMPARE"
        : "REFERENCE = Input 1   |   COMPARE = selected Input 2-16   |   positive delay = COMPARE arrives later";
#else
    const juce::String tagline = layout.compact
        ? "L = REFERENCE   |   R = COMPARE"
        : "REFERENCE = left channel   |   COMPARE = right channel   |   positive delay = COMPARE arrives later";
#endif
    if (taglineW > 120.0f)
        g.drawFittedText(tagline,
                         static_cast<int>(taglineX), 0,
                         static_cast<int>(taglineW), static_cast<int>(layout.headerHeight),
                         juce::Justification::centredRight, 1);

    auto topPanel = juce::Rectangle<float>(margin, layout.topPanelY, W - margin * 2.0f, layout.topPanelHeight);
    g.setColour(panel);
    g.fillRoundedRectangle(topPanel, 8.0f * s);
    g.setColour(moduleOutline);
    g.drawRoundedRectangle(topPanel.reduced(0.5f), 8.0f * s, 1.0f);

    const auto innerW = topPanel.getWidth();
    const auto meterW = juce::jlimit(145.0f, 270.0f, innerW * (layout.compact ? 0.22f : 0.235f));
    const auto infoW = juce::jlimit(185.0f, 320.0f, innerW * (layout.compact ? 0.26f : 0.27f));
    const auto sidePad = juce::jlimit(12.0f, 20.0f, 16.0f * s);
    const auto infoX = topPanel.getRight() - infoW - sidePad;
    const auto delayLeft = topPanel.getX() + meterW + sidePad * 2.0f;
    const auto delayRight = infoX - sidePad;
    const auto meterH = juce::jlimit(44.0f, 60.0f, 52.0f * s);
    const auto meterTop = topPanel.getY() + 24.0f * s;
    const auto meterGap = juce::jlimit(18.0f, 30.0f, 24.0f * s);

    drawMeter(g,
              { topPanel.getX() + sidePad, meterTop, meterW, meterH },
              audioProcessor.getRefLevelDb(),
#if SIGNALALIGN_16_INPUT
              "REFERENCE / INPUT 1");
#else
              "REFERENCE / LEFT");
#endif
    drawMeter(g,
              { topPanel.getX() + sidePad, meterTop + meterH + meterGap, meterW, meterH },
              audioProcessor.getMeasLevelDb(),
#if SIGNALALIGN_16_INPUT
              "COMPARE / INPUT " + juce::String(audioProcessor.getSelectedCompareInputNumber()));
#else
              "COMPARE / RIGHT");
#endif

    const auto delayWidth = juce::jmax(120.0f, delayRight - delayLeft);
    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions(juce::jlimit(10.0f, 15.0f, 12.0f * s))).boldened());
    g.drawText("ROBUST DELAY",
               static_cast<int>(delayLeft), static_cast<int>(topPanel.getY() + 12.0f * s),
               static_cast<int>(delayWidth), static_cast<int>(20.0f * s),
               juce::Justification::centred);

    const auto delayMainFont = juce::jlimit(25.0f, 48.0f, juce::jmin(38.0f * s, delayWidth * 0.105f));
    const auto delayMsFont = juce::jlimit(16.0f, 28.0f, 21.0f * s);
    const auto directionFont = juce::jlimit(10.0f, 15.5f, 13.0f * s);

    const auto sampleDecimals = juce::jlimit(0, 4, static_cast<int>(std::lround(
        audioProcessor.apvts.getRawParameterValue(signalalign::ParamIDs::sampleDecimals)->load())));
    const auto msDecimals = juce::jlimit(0, 4, static_cast<int>(std::lround(
        audioProcessor.apvts.getRawParameterValue(signalalign::ParamIDs::msDecimals)->load())));

    const auto formatWithDecimals = [] (double value, int decimals) -> juce::String
    {
        // 0 decimals means a true integer display: no decimal separator and
        // no trailing fractional digit.
        if (decimals <= 0)
            return juce::String(static_cast<juce::int64>(std::llround(value)));

        return juce::String(value, decimals);
    };

    sampleValueBounds = juce::Rectangle<int>(
        static_cast<int>(delayLeft), static_cast<int>(topPanel.getY() + 34.0f * s),
        static_cast<int>(delayWidth), static_cast<int>(52.0f * s));
    msValueBounds = juce::Rectangle<int>(
        static_cast<int>(delayLeft), static_cast<int>(topPanel.getY() + 84.0f * s),
        static_cast<int>(delayWidth), static_cast<int>(30.0f * s));

    const bool currentInputsActive = audioProcessor.getRefLevelDb() > -95.0f
                                  && audioProcessor.getMeasLevelDb() > -95.0f;
    const bool hasCurrentResult = latest.valid && latest.delayQualified && currentInputsActive;

    if (hasCurrentResult)
    {
        const auto sign = latest.delaySamples >= 0.0 ? "+" : "";
        g.setColour(primaryBlue);
        g.setFont(juce::Font(juce::FontOptions(delayMainFont)).boldened());
        g.drawFittedText(sign + formatWithDecimals(latest.delaySamples, sampleDecimals) + " samples",
                         sampleValueBounds,
                         juce::Justification::centred, 1);

        g.setColour(primaryBlue.brighter(0.10f));
        g.setFont(juce::Font(juce::FontOptions(delayMsFont)));
        g.drawText(sign + formatWithDecimals(latest.delayMs, msDecimals) + " ms",
                   msValueBounds, juce::Justification::centred);

        juce::String direction;
        if (std::abs(latest.delaySamples) < 0.25)
            direction = "ALIGNED";
        else if (latest.delaySamples > 0.0)
            direction = "COMPARE ARRIVES LATER";
        else
            direction = "REFERENCE ARRIVES LATER";

        g.setColour(coherenceColour);
        g.setFont(juce::Font(juce::FontOptions(directionFont)).boldened());
        g.drawFittedText(direction,
                         static_cast<int>(delayLeft + 2.0f), static_cast<int>(topPanel.getY() + 122.0f * s),
                         static_cast<int>(delayWidth - 4.0f), static_cast<int>(28.0f * s),
                         juce::Justification::centred, 1);
    }
    else if (latest.hasDelayEstimate)
    {
        // Keep the last trusted estimate visible, but clearly mark it as stale.
        // This is display-only; rejected frames never enter smoothing and the
        // live alignment bar below is not driven by this held value.
        const auto sign = latest.delaySamples >= 0.0 ? "+" : "";
        const auto heldColour = dimText.withAlpha(0.72f);

        g.setColour(heldColour);
        g.setFont(juce::Font(juce::FontOptions(delayMainFont)).boldened());
        g.drawFittedText(sign + formatWithDecimals(latest.delaySamples, sampleDecimals) + " samples",
                         sampleValueBounds,
                         juce::Justification::centred, 1);

        g.setFont(juce::Font(juce::FontOptions(delayMsFont)));
        g.drawText(sign + formatWithDecimals(latest.delayMs, msDecimals) + " ms",
                   msValueBounds, juce::Justification::centred);

        g.setColour(dimText.withAlpha(0.92f));
        g.setFont(juce::Font(juce::FontOptions(directionFont)).boldened());
        g.drawFittedText("LAST VALID | NO CURRENT RESULT",
                         static_cast<int>(delayLeft + 2.0f), static_cast<int>(topPanel.getY() + 122.0f * s),
                         static_cast<int>(delayWidth - 4.0f), static_cast<int>(28.0f * s),
                         juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(dimText);
        g.setFont(juce::Font(juce::FontOptions(juce::jlimit(18.0f, 32.0f, 27.0f * s))).boldened());
        g.drawFittedText("WAITING FOR FIRST VALID RESULT",
                         static_cast<int>(delayLeft), static_cast<int>(topPanel.getY() + 60.0f * s),
                         static_cast<int>(delayWidth), static_cast<int>(50.0f * s),
                         juce::Justification::centred, 2);
    }

    // Detailed robust quality panel scales with the available right-hand space.
    const auto infoY = topPanel.getY() + 16.0f * s;
    const auto infoH = juce::jmin(topPanel.getHeight() - 32.0f * s, 205.0f * s);
    g.setColour(panelRaised);
    g.fillRoundedRectangle({ infoX, infoY, infoW, infoH }, 6.0f * s);
    g.setColour(moduleOutline);
    g.drawRoundedRectangle({ infoX, infoY, infoW, infoH }, 6.0f * s, 1.0f);

    const auto infoPad = juce::jlimit(8.0f, 14.0f, 11.0f * s);
    const auto labelX = infoX + infoPad;
    const auto labelW = juce::jmax(78.0f, infoW * 0.43f);
    const auto valueX = labelX + labelW;
    const auto valueW = juce::jmax(60.0f, infoX + infoW - infoPad - valueX);
    const auto rowStep = infoH / 8.65f;
    const auto rowY = [infoY, rowStep](int row) { return infoY + 5.0f + static_cast<float>(row) * rowStep; };

    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions(juce::jlimit(9.0f, 12.0f, 10.8f * s))).boldened());
    const juce::String labels[] { "CONFIDENCE", "COHERENCE", "PEAK RATIO", "STABILITY",
                                  "PHASE FIT", "PHASE CHECK", "BAND CHECK", "POLARITY" };
    for (int row = 0; row < 8; ++row)
        g.drawFittedText(labels[row], static_cast<int>(labelX), static_cast<int>(rowY(row)),
                         static_cast<int>(labelW - 4.0f), static_cast<int>(rowStep),
                         juce::Justification::centredLeft, 1);

    const auto confidenceText = latest.valid ? juce::String(latest.confidence * 100.0, 0) + " %" : "--";
    const auto coherenceText = latest.valid ? juce::String(latest.meanCoherence * 100.0, 0) + " %" : "--";
    const auto peakText = latest.valid ? juce::String(latest.peakRatio, 2) + " : 1" : "--";
    const auto stabilityText = latest.valid
        ? (latest.stabilityReady ? juce::String(latest.stabilityMs, 3) + " ms" : "warming")
        : "--";
    const auto phaseText = latest.valid ? juce::String(latest.phaseFitSamples, 2) + " smp" : "--";
    const auto phaseCheckText = !latest.valid ? "--"
        : (!latest.phaseCheckApplied ? "N/A"
           : juce::String(latest.phaseQualified ? "OK " : "FAIL ") + juce::String(latest.phaseDifferenceMs, 2) + " ms");
    const auto bandText = !latest.valid ? "--"
        : (latest.bandUsable > 0 ? juce::String(latest.bandAgreeing) + "/" + juce::String(latest.bandUsable)
                                 : "N/A");
    const auto polarityText = latest.valid ? (latest.polarityInverted ? "INVERTED" : "NORMAL") : "--";
    const juce::String values[] { confidenceText, coherenceText, peakText, stabilityText,
                                  phaseText, phaseCheckText, bandText, polarityText };

    g.setFont(juce::Font(juce::FontOptions(juce::jlimit(9.5f, 13.0f, 11.8f * s))));
    for (int row = 0; row < 8; ++row)
    {
        juce::Colour valueColour = uiText;
        if ((row == 0 && latest.valid && !latest.confidenceQualified)
            || (row == 1 && latest.valid && !latest.coherenceQualified)
            || (row == 2 && latest.valid && !latest.peakRatioQualified)
            || (row == 3 && latest.stabilityReady && !latest.stabilityQualified)
            || (row == 5 && latest.phaseCheckApplied && !latest.phaseQualified))
            valueColour = delayRed;
        if (row == 7 && latest.polarityInverted)
            valueColour = accent;

        g.setColour(valueColour);
        g.drawFittedText(values[row], static_cast<int>(valueX), static_cast<int>(rowY(row)),
                         static_cast<int>(valueW), static_cast<int>(rowStep),
                         juce::Justification::centredRight, 1);
    }

    const auto delayBarHeight = juce::jlimit(52.0f, 64.0f, 58.0f * s);
    const auto delayBarY = topPanel.getBottom() - delayBarHeight - 4.0f;
    if (delayRight > delayLeft + 120.0f)
        drawDelayBar(g, { delayLeft, delayBarY, delayRight - delayLeft, delayBarHeight });

    if (settingsVisible)
    {
        const auto controlsPanel = juce::Rectangle<float>(margin, layout.controlsTop, W - margin * 2.0f, layout.controlsHeight);
        g.setColour(panel);
        g.fillRoundedRectangle(controlsPanel, 8.0f * s);
        g.setColour(moduleOutline);
        g.drawRoundedRectangle(controlsPanel.reduced(0.5f), 8.0f * s, 1.0f);
    }

    auto coherenceBounds = juce::Rectangle<float>(margin, layout.plotTop, W - margin * 2.0f, layout.plotHeight);
    auto phaseBounds = juce::Rectangle<float>(margin, layout.plotTop + layout.plotHeight + layout.plotGap,
                                              W - margin * 2.0f, layout.plotHeight);

    drawCoherencePlot(g, coherenceBounds);
    drawPhasePlot(g, phaseBounds);
}

void SignalAlignAudioProcessorEditor::drawDelayBar(juce::Graphics& g, juce::Rectangle<float> outer)
{
    delayBarBounds = outer.toNearestInt();

    const auto* greenParam = audioProcessor.apvts.getRawParameterValue(signalalign::ParamIDs::delayGreenThresholdMs);
    const auto* redParam = audioProcessor.apvts.getRawParameterValue(signalalign::ParamIDs::delayRedThresholdMs);
    const auto greenThreshold = greenParam != nullptr ? greenParam->load() : 0.5f;
    const auto redThreshold = juce::jmax(greenThreshold, redParam != nullptr ? redParam->load() : 2.0f);

    // drawDelayBar() is a separate member function, so it cannot see the local
    // msDecimals variable used by paint(). Read the same APVTS display setting
    // here so the REFERENCE / COMPARE correction amounts use identical precision.
    const auto* msDecimalsParam = audioProcessor.apvts.getRawParameterValue(signalalign::ParamIDs::msDecimals);
    const auto msDecimals = juce::jlimit(0, 4, static_cast<int>(std::lround(
        msDecimalsParam != nullptr ? msDecimalsParam->load() : 2.0f)));

    const auto formatMs = [msDecimals] (double value) -> juce::String
    {
        if (msDecimals <= 0)
            return juce::String(static_cast<juce::int64>(std::llround(value)));

        return juce::String(value, msDecimals);
    };

    // Show both possible correction directions above the bar. The displayed
    // delay is defined as positive when COMPARE arrives later than REFERENCE.
    // Therefore the required delay changes are always exact opposites:
    //   REFERENCE correction = + measured delay
    //   COMPARE correction   = - measured delay
    // This is display-only and deliberately follows the smoothed delay value.
    // Arrows are drawn as vector graphics so they stay large and crisp on both
    // macOS and Windows. Green/right = increase delay, red/left = reduce delay.
    auto correctionHeader = outer.removeFromTop(34.0f);
    const bool inputsActive = audioProcessor.getRefLevelDb() > -95.0f
                           && audioProcessor.getMeasLevelDb() > -95.0f;
    const bool currentCorrection = latest.valid && latest.delayQualified && inputsActive;
    const bool heldCorrection = !currentCorrection && latest.hasDelayEstimate;

    auto leftHeader = correctionHeader.removeFromLeft(correctionHeader.getWidth() * 0.5f).reduced(2.0f, 0.0f);
    auto rightHeader = correctionHeader.reduced(2.0f, 0.0f);
    const bool compactCorrection = outer.getWidth() < 430.0f;

    const auto increaseColour = coherenceColour;
    const auto reduceColour = delayRed;

    const auto drawVectorArrow = [&g](juce::Rectangle<float> bounds, bool increase, juce::Colour colour)
    {
        bounds = bounds.reduced(2.0f, 3.0f);
        const auto cy = bounds.getCentreY();
        const auto headLength = juce::jlimit(8.0f, 13.0f, bounds.getHeight() * 0.48f);
        const auto halfHeadHeight = juce::jlimit(6.0f, 10.0f, bounds.getHeight() * 0.36f);
        const auto shaftThickness = juce::jlimit(3.0f, 5.0f, bounds.getHeight() * 0.15f);
        const auto left = bounds.getX() + 1.0f;
        const auto right = bounds.getRight() - 1.0f;

        g.setColour(colour);

        juce::Path head;
        if (increase)
        {
            const auto headBase = right - headLength;
            g.fillRect(juce::Rectangle<float>(left, cy - shaftThickness * 0.5f,
                                              juce::jmax(1.0f, headBase - left + 1.0f), shaftThickness));
            head.startNewSubPath(headBase, cy - halfHeadHeight);
            head.lineTo(right, cy);
            head.lineTo(headBase, cy + halfHeadHeight);
        }
        else
        {
            const auto headBase = left + headLength;
            g.fillRect(juce::Rectangle<float>(headBase - 1.0f, cy - shaftThickness * 0.5f,
                                              juce::jmax(1.0f, right - headBase + 1.0f), shaftThickness));
            head.startNewSubPath(headBase, cy - halfHeadHeight);
            head.lineTo(left, cy);
            head.lineTo(headBase, cy + halfHeadHeight);
        }
        head.closeSubPath();
        g.fillPath(head);
    };

    const auto drawCorrection = [&](juce::Rectangle<float> area,
                                    const juce::String& fullName,
                                    const juce::String& shortName,
                                    double deltaMs,
                                    bool isCurrent)
    {
        const auto name = compactCorrection ? shortName : fullName;
        const auto absMs = std::abs(deltaMs);
        const auto alpha = isCurrent ? 1.0f : 0.48f;
        const auto textColour = isCurrent ? uiText : dimText.withAlpha(0.72f);

        if (absMs < 0.00001)
        {
            g.setColour(textColour);
            g.setFont(juce::Font(juce::FontOptions(compactCorrection ? 12.0f : 15.0f)).boldened());
            g.drawFittedText(name + "   =   " + formatMs(0.0) + " ms", area.toNearestInt(), juce::Justification::centred, 1);
            return;
        }

        const bool increase = deltaMs > 0.0;
        const auto arrowColour = (increase ? increaseColour : reduceColour).withAlpha(alpha);

        auto nameArea = area.removeFromLeft(area.getWidth() * (compactCorrection ? 0.30f : 0.34f));
        auto valueArea = area.removeFromRight(area.getWidth() * (compactCorrection ? 0.44f : 0.42f));
        auto arrowArea = area.reduced(compactCorrection ? 0.0f : 2.0f, 0.0f);

        g.setColour(textColour);
        g.setFont(juce::Font(juce::FontOptions(compactCorrection ? 11.8f : 14.5f)).boldened());
        g.drawFittedText(name, nameArea.toNearestInt(), juce::Justification::centredLeft, 1);
        g.drawFittedText(formatMs(absMs) + " ms", valueArea.toNearestInt(),
                         juce::Justification::centredRight, 1);

        drawVectorArrow(arrowArea, increase, arrowColour);
    };

    if (currentCorrection || heldCorrection)
    {
        drawCorrection(leftHeader, "REFERENCE", "REF", latest.delayMs, currentCorrection);
        drawCorrection(rightHeader, "COMPARE", "COM", -latest.delayMs, currentCorrection);
    }
    else
    {
        g.setColour(dimText.withAlpha(0.65f));
        g.setFont(juce::Font(juce::FontOptions(compactCorrection ? 11.8f : 14.5f)).boldened());
        g.drawFittedText(compactCorrection ? "REF  --" : "REFERENCE  --",
                         leftHeader.toNearestInt(), juce::Justification::centredLeft, 1);
        g.drawFittedText(compactCorrection ? "COM  --" : "COMPARE  --",
                         rightHeader.toNearestInt(), juce::Justification::centredRight, 1);
    }

    auto bar = outer.removeFromTop(18.0f).reduced(0.0f, 2.0f);
    const auto centreX = bar.getCentreX();
    const auto halfWidth = bar.getWidth() * 0.5f;

    // Keep the two useful threshold regions clearly visible. A delay beyond the
    // displayed range simply saturates the meter at the edge.
    const auto displayMaxMs = juce::jmax(5.0f, redThreshold * 2.5f);
    const auto msToPixels = halfWidth / displayMaxMs;
    const auto greenPx = juce::jmin(halfWidth, greenThreshold * msToPixels);
    const auto redPx = juce::jmin(halfWidth, redThreshold * msToPixels);

    g.setColour(panelRaised);
    g.fillRoundedRectangle(bar, 3.0f);
    g.setColour(moduleOutline.withAlpha(0.70f));
    g.drawRoundedRectangle(bar.reduced(0.5f), 3.0f, 1.0f);

    // Static threshold zones around zero, PPM-style.
    g.setColour(coherenceColour.withAlpha(0.18f));
    g.fillRect(juce::Rectangle<float>(centreX - greenPx, bar.getY(), greenPx * 2.0f, bar.getHeight()));

    if (redPx > greenPx)
    {
        g.setColour(delayOrange.withAlpha(0.16f));
        g.fillRect(juce::Rectangle<float>(centreX - redPx, bar.getY(), redPx - greenPx, bar.getHeight()));
        g.fillRect(juce::Rectangle<float>(centreX + greenPx, bar.getY(), redPx - greenPx, bar.getHeight()));
    }

    g.setColour(delayRed.withAlpha(0.10f));
    if (redPx < halfWidth)
    {
        g.fillRect(juce::Rectangle<float>(bar.getX(), bar.getY(), halfWidth - redPx, bar.getHeight()));
        g.fillRect(juce::Rectangle<float>(centreX + redPx, bar.getY(), halfWidth - redPx, bar.getHeight()));
    }

    // Threshold markers and centre zero line.
    g.setColour(coherenceColour.withAlpha(0.75f));
    g.drawVerticalLine(static_cast<int>(centreX - greenPx), bar.getY(), bar.getBottom());
    g.drawVerticalLine(static_cast<int>(centreX + greenPx), bar.getY(), bar.getBottom());
    g.setColour(delayRed.withAlpha(0.75f));
    g.drawVerticalLine(static_cast<int>(centreX - redPx), bar.getY(), bar.getBottom());
    g.drawVerticalLine(static_cast<int>(centreX + redPx), bar.getY(), bar.getBottom());
    g.setColour(uiText.withAlpha(0.85f));
    g.drawVerticalLine(static_cast<int>(centreX), bar.getY() - 1.0f, bar.getBottom() + 1.0f);

    const bool liveDelayForBar = latest.valid && latest.delayQualified
                              && audioProcessor.getRefLevelDb() > -95.0f
                              && audioProcessor.getMeasLevelDb() > -95.0f;
    if (liveDelayForBar)
    {
        const auto delayMs = static_cast<float>(latest.delayMs);
        const auto absDelayMs = std::abs(delayMs);
        const auto clamped = juce::jlimit(-displayMaxMs, displayMaxMs, delayMs);
        const auto valueX = centreX + clamped * msToPixels;

        juce::Colour valueColour = coherenceColour;
        if (absDelayMs > redThreshold)
            valueColour = delayRed;
        else if (absDelayMs > greenThreshold)
            valueColour = delayOrange;

        auto valueRect = juce::Rectangle<float>(juce::jmin(centreX, valueX),
                                                 bar.getY() + 2.0f,
                                                 std::abs(valueX - centreX),
                                                 bar.getHeight() - 4.0f);
        if (valueRect.getWidth() < 2.0f)
            valueRect.setWidth(2.0f);

        g.setColour(valueColour);
        g.fillRoundedRectangle(valueRect, 2.0f);

        g.setColour(valueColour);
        g.fillEllipse(valueX - 3.0f, bar.getCentreY() - 3.0f, 6.0f, 6.0f);
    }

    auto footer = outer;
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.setColour(dimText);
    g.drawText("-" + juce::String(displayMaxMs, 1) + " ms", footer.toNearestInt(), juce::Justification::centredLeft);
    const auto centreFooter = outer.getWidth() >= 430.0f
        ? "0   |   Green <= " + juce::String(greenThreshold, 1) + " ms   |   Red > "
            + juce::String(redThreshold, 1) + " ms   |   right-click limits"
        : "0";
    g.drawFittedText(centreFooter, footer.toNearestInt(), juce::Justification::centred, 1);
    g.drawText("+" + juce::String(displayMaxMs, 1) + " ms", footer.toNearestInt(), juce::Justification::centredRight);

}

void SignalAlignAudioProcessorEditor::showPrecisionMenu(const juce::String& parameterID,
                                                        const juce::String& title,
                                                        juce::Component* target)
{
    auto* parameter = audioProcessor.apvts.getParameter(parameterID);
    if (parameter == nullptr)
        return;

    const auto current = juce::jlimit(0, 4, static_cast<int>(std::lround(parameter->convertFrom0to1(parameter->getValue()))));

    juce::PopupMenu menu;
    menu.addSectionHeader(title);
    for (int decimals = 0; decimals <= 4; ++decimals)
    {
        const auto label = juce::String(decimals) + (decimals == 1 ? " decimal place" : " decimal places");
        menu.addItem(decimals + 1, label, true, decimals == current);
    }

    juce::Component::SafePointer<SignalAlignAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target != nullptr ? target : this),
                       [safeThis, parameterID](int result)
                       {
                           if (safeThis == nullptr || result < 1 || result > 5)
                               return;

                           if (auto* p = safeThis->audioProcessor.apvts.getParameter(parameterID))
                           {
                               const auto decimals = static_cast<float>(result - 1);
                               p->beginChangeGesture();
                               p->setValueNotifyingHost(p->convertTo0to1(decimals));
                               p->endChangeGesture();
                               safeThis->repaint();
                           }
                       });
}

void SignalAlignAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu())
        return;

    const auto position = event.getPosition();

    if (sampleValueBounds.contains(position))
    {
        showPrecisionMenu(signalalign::ParamIDs::sampleDecimals, "SAMPLE DECIMALS", this);
        return;
    }

    if (msValueBounds.contains(position))
    {
        showPrecisionMenu(signalalign::ParamIDs::msDecimals, "MILLISECOND DECIMALS", this);
        return;
    }

    if (delayBarBounds.contains(position))
    {
        auto content = std::make_unique<DelayThresholdEditor>(audioProcessor.apvts);
        auto& callout = juce::CallOutBox::launchAsynchronously(std::move(content), delayBarBounds, this);
        callout.setArrowSize(14.0f);
    }
}

void SignalAlignAudioProcessorEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float levelDb, const juce::String& name) const
{
    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions(11.5f)).boldened());
    g.drawText(name, bounds.removeFromTop(17.0f).toNearestInt(), juce::Justification::centredLeft);

    auto bar = bounds.reduced(0.0f, 8.0f);
    g.setColour(panelRaised);
    g.fillRoundedRectangle(bar, 3.0f);

    const auto norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    auto filled = bar.withWidth(bar.getWidth() * norm);
    g.setColour(coherenceColour);
    g.fillRoundedRectangle(filled, 3.0f);

    g.setColour(uiText);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(juce::String(levelDb, 1) + " dBFS", bar.toNearestInt(), juce::Justification::centredRight);
}

float SignalAlignAudioProcessorEditor::frequencyToX(float frequency, juce::Rectangle<float> bounds) const
{
    const auto maxFreq = static_cast<float>(latest.sampleRate > 0.0 ? juce::jmin(22000.0, latest.sampleRate * 0.5) : 20000.0);
    const float minFreq = 20.0f;
    const auto f = juce::jlimit(minFreq, maxFreq, frequency);
    const auto norm = std::log10(f / minFreq) / std::log10(maxFreq / minFreq);
    return bounds.getX() + static_cast<float>(norm) * bounds.getWidth();
}

void SignalAlignAudioProcessorEditor::drawLogFrequencyGrid(juce::Graphics& g, juce::Rectangle<float> bounds, bool phasePlot) const
{
    static constexpr float frequencies[] { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    // The plots deliberately compress more aggressively than the surrounding GUI.
    // At small sizes the graph gets priority over labels and margins.
    const auto compactPlot = bounds.getWidth() < 760.0f || bounds.getHeight() < 92.0f;
    const auto tinyPlot = bounds.getWidth() < 620.0f || bounds.getHeight() < 58.0f;
    const auto ultraTinyPlot = bounds.getHeight() < 40.0f;
    const auto axisFont = juce::jlimit(5.4f, 10.0f,
                                       juce::jmin(bounds.getWidth() / 82.0f,
                                                  bounds.getHeight() / 7.5f));
    const auto labelH = juce::jlimit(6.0f, 14.0f, 11.5f * (axisFont / 10.0f));
    const auto labelW = juce::jlimit(20.0f, 44.0f, 38.0f * (axisFont / 10.0f));

    g.setColour(grid.withAlpha(compactPlot ? 0.42f : 0.55f));
    g.setFont(juce::Font(juce::FontOptions(axisFont)));

    for (const auto f : frequencies)
    {
        if (f > latest.sampleRate * 0.5)
            continue;

        const auto x = frequencyToX(f, bounds);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

        bool drawLabel = ! ultraTinyPlot;
        if (tinyPlot)
            drawLabel = ! ultraTinyPlot && (f == 20.0f || f == 100.0f || f == 1000.0f || f == 10000.0f || f == 20000.0f);
        else if (compactPlot)
            drawLabel = (f == 20.0f || f == 100.0f || f == 500.0f || f == 1000.0f || f == 5000.0f || f == 20000.0f);

        if (drawLabel)
        {
            juce::String label = f >= 1000.0f
                ? juce::String(f / 1000.0f, f < 10000.0f ? 1 : 0) + "k"
                : juce::String(static_cast<int>(f));

            g.setColour(dimText);
            g.drawText(label,
                       static_cast<int>(x - labelW * 0.5f),
                       static_cast<int>(bounds.getBottom() - labelH),
                       static_cast<int>(labelW),
                       static_cast<int>(labelH),
                       juce::Justification::centred);
            g.setColour(grid.withAlpha(compactPlot ? 0.42f : 0.55f));
        }
    }

    if (phasePlot)
    {
        // Keep all phase reference lines; they are useful even when labels are reduced.
        for (const auto deg : { -180, -90, 0, 90, 180 })
        {
            const auto y = juce::jmap(static_cast<float>(deg), -180.0f, 180.0f, bounds.getBottom(), bounds.getY());
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
        }
    }
    else
    {
        for (const auto c : { 0.0f, 0.5f, 1.0f })
        {
            const auto y = juce::jmap(c, 0.0f, 1.0f, bounds.getBottom(), bounds.getY());
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
        }
    }
}

void SignalAlignAudioProcessorEditor::drawCoherencePlot(juce::Graphics& g, juce::Rectangle<float> outer) const
{
    g.setColour(panel);

    const auto plotScale = juce::jlimit(0.38f, 1.0f,
                                        juce::jmin(outer.getWidth() / 1070.0f,
                                                   outer.getHeight() / 120.0f));
    const auto compactPlot = plotScale < 0.82f;
    const auto corner = juce::jlimit(4.0f, 8.0f, 8.0f * plotScale);
    g.fillRoundedRectangle(outer, corner);
    g.setColour(moduleOutline);
    g.drawRoundedRectangle(outer.reduced(0.5f), corner, 1.0f);

    // At small sizes preserve the curve area first: margins shrink much faster
    // than the panel itself.
    const auto leftInset = juce::jlimit(18.0f, 48.0f, 48.0f * plotScale);
    const auto rightInset = juce::jlimit(5.0f, 30.0f, 22.0f * plotScale);
    const auto topInset = juce::jlimit(7.0f, 22.0f, 22.0f * plotScale);
    const auto bottomInset = juce::jlimit(5.0f, 22.0f, 20.0f * plotScale);
    auto bounds = juce::Rectangle<float>(outer.getX() + leftInset,
                                         outer.getY() + topInset,
                                         juce::jmax(30.0f, outer.getWidth() - leftInset - rightInset),
                                         juce::jmax(24.0f, outer.getHeight() - topInset - bottomInset));

    const auto titleFont = juce::jlimit(6.5f, 12.0f, 12.0f * plotScale);
    const auto axisFont = juce::jlimit(5.8f, 11.0f, 10.5f * plotScale);
    const auto yLabelW = juce::jlimit(17.0f, 40.0f, 40.0f * plotScale);
    const auto labelH = juce::jlimit(7.0f, 14.0f, 14.0f * plotScale);

    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions(titleFont)).boldened());
    g.drawText("COHERENCE",
               static_cast<int>(outer.getX() + juce::jlimit(7.0f, 14.0f, 14.0f * plotScale)),
               static_cast<int>(outer.getY() + juce::jlimit(2.0f, 7.0f, 7.0f * plotScale)),
               static_cast<int>(juce::jlimit(78.0f, 120.0f, 120.0f * plotScale)),
               static_cast<int>(juce::jlimit(12.0f, 18.0f, 18.0f * plotScale)),
               juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(axisFont)));
    g.drawText("100%", static_cast<int>(outer.getX() + 2.0f), static_cast<int>(bounds.getY() - labelH * 0.5f),
               static_cast<int>(yLabelW), static_cast<int>(labelH), juce::Justification::centredRight);
    g.drawText("0%", static_cast<int>(outer.getX() + 2.0f), static_cast<int>(bounds.getBottom() - labelH * 0.5f),
               static_cast<int>(yLabelW), static_cast<int>(labelH), juce::Justification::centredRight);

    drawLogFrequencyGrid(g, bounds, false);

    if (const auto* thresholdParam = audioProcessor.apvts.getRawParameterValue(signalalign::ParamIDs::minCoherence))
    {
        const auto threshold = juce::jlimit(0.0f, 1.0f, thresholdParam->load() / 100.0f);
        const auto y = juce::jmap(threshold, 0.0f, 1.0f, bounds.getBottom(), bounds.getY());
        g.setColour(accent.withAlpha(0.75f));
        const float dashLengths[] { 5.0f, 4.0f };
        g.drawDashedLine({ bounds.getX(), y, bounds.getRight(), y }, dashLengths, 2, 1.0f);
        const auto thresholdFont = juce::jlimit(7.0f, 9.5f, 9.5f * plotScale);
        g.setFont(juce::Font(juce::FontOptions(thresholdFont)));
        if (!compactPlot || bounds.getHeight() > 54.0f)
            g.drawText("MIN " + juce::String(threshold * 100.0f, 0) + "%",
                       static_cast<int>(bounds.getRight() - juce::jlimit(42.0f, 60.0f, 60.0f * plotScale)),
                       static_cast<int>(y - juce::jlimit(8.0f, 14.0f, 14.0f * plotScale)),
                       static_cast<int>(juce::jlimit(40.0f, 58.0f, 58.0f * plotScale)),
                       static_cast<int>(juce::jlimit(9.0f, 13.0f, 13.0f * plotScale)),
                       juce::Justification::centredRight);
    }

    if (latest.frequencies.empty())
        return;

    juce::Path path;
    bool first = true;
    for (size_t i = 0; i < latest.frequencies.size() && i < latest.coherence.size(); ++i)
    {
        const auto x = frequencyToX(latest.frequencies[i], bounds);
        const auto y = juce::jmap(juce::jlimit(0.0f, 1.0f, latest.coherence[i]), 0.0f, 1.0f, bounds.getBottom(), bounds.getY());
        if (first) { path.startNewSubPath(x, y); first = false; }
        else path.lineTo(x, y);
    }

    g.setColour(coherenceColour);
    g.strokePath(path, juce::PathStrokeType(juce::jlimit(1.2f, 2.0f, 2.0f * plotScale)));
}

void SignalAlignAudioProcessorEditor::drawPhasePlot(juce::Graphics& g, juce::Rectangle<float> outer) const
{
    g.setColour(panel);

    const auto plotScale = juce::jlimit(0.38f, 1.0f,
                                        juce::jmin(outer.getWidth() / 1070.0f,
                                                   outer.getHeight() / 120.0f));
    const auto compactPlot = plotScale < 0.82f;
    const auto corner = juce::jlimit(4.0f, 8.0f, 8.0f * plotScale);
    g.fillRoundedRectangle(outer, corner);
    g.setColour(moduleOutline);
    g.drawRoundedRectangle(outer.reduced(0.5f), corner, 1.0f);

    const auto leftInset = juce::jlimit(19.0f, 48.0f, 48.0f * plotScale);
    const auto rightInset = juce::jlimit(5.0f, 30.0f, 22.0f * plotScale);
    const auto topInset = juce::jlimit(7.0f, 22.0f, 22.0f * plotScale);
    const auto bottomInset = juce::jlimit(5.0f, 22.0f, 20.0f * plotScale);
    auto bounds = juce::Rectangle<float>(outer.getX() + leftInset,
                                         outer.getY() + topInset,
                                         juce::jmax(30.0f, outer.getWidth() - leftInset - rightInset),
                                         juce::jmax(24.0f, outer.getHeight() - topInset - bottomInset));

    const auto titleFont = juce::jlimit(6.3f, 12.0f, 12.0f * plotScale);
    const auto axisFont = juce::jlimit(5.6f, 10.5f, 10.2f * plotScale);
    const auto yLabelW = juce::jlimit(18.0f, 44.0f, 44.0f * plotScale);
    const auto labelH = juce::jlimit(7.0f, 14.0f, 14.0f * plotScale);

    g.setColour(dimText);
    g.setFont(juce::Font(juce::FontOptions(titleFont)).boldened());
    const auto phaseTitle = compactPlot ? "PHASE" : "PHASE  (COMPARE vs REFERENCE)";
    g.drawFittedText(phaseTitle,
                     static_cast<int>(outer.getX() + juce::jlimit(7.0f, 14.0f, 14.0f * plotScale)),
                     static_cast<int>(outer.getY() + juce::jlimit(2.0f, 7.0f, 7.0f * plotScale)),
                     static_cast<int>(juce::jmax(70.0f, outer.getWidth() * 0.42f)),
                     static_cast<int>(juce::jlimit(12.0f, 18.0f, 18.0f * plotScale)),
                     juce::Justification::centredLeft, 1);

    g.setFont(juce::Font(juce::FontOptions(axisFont)));
    g.drawText("+180", static_cast<int>(outer.getX() + 1.0f), static_cast<int>(bounds.getY() - labelH * 0.5f),
               static_cast<int>(yLabelW), static_cast<int>(labelH), juce::Justification::centredRight);
    if (!compactPlot || bounds.getHeight() > 48.0f)
        g.drawText("0", static_cast<int>(outer.getX() + 1.0f), static_cast<int>(bounds.getCentreY() - labelH * 0.5f),
                   static_cast<int>(yLabelW), static_cast<int>(labelH), juce::Justification::centredRight);
    g.drawText("-180", static_cast<int>(outer.getX() + 1.0f), static_cast<int>(bounds.getBottom() - labelH * 0.5f),
               static_cast<int>(yLabelW), static_cast<int>(labelH), juce::Justification::centredRight);

    drawLogFrequencyGrid(g, bounds, true);

    if (latest.frequencies.empty())
        return;

    juce::Path path;
    bool first = true;
    float previousPhase = 0.0f;

    for (size_t i = 0; i < latest.frequencies.size() && i < latest.phaseDegrees.size(); ++i)
    {
        const auto phase = juce::jlimit(-180.0f, 180.0f, latest.phaseDegrees[i]);
        const auto x = frequencyToX(latest.frequencies[i], bounds);
        const auto y = juce::jmap(phase, -180.0f, 180.0f, bounds.getBottom(), bounds.getY());

        if (first || std::abs(phase - previousPhase) > 250.0f)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);

        previousPhase = phase;
        first = false;
    }

    g.setColour(phaseColour);
    g.strokePath(path, juce::PathStrokeType(juce::jlimit(1.1f, 1.8f, 1.8f * plotScale)));
}

void SignalAlignAudioProcessorEditor::resized()
{
    const auto W = getWidth();
    const auto layout = makeResponsiveLayout(getWidth(), getHeight(), settingsVisible, advancedVisible);
    const auto s = layout.uiScale;
    const int margin = static_cast<int>(layout.margin);

    const int logoW = juce::jlimit(250, 390, static_cast<int>(305.0f * s));
    logoButton.setBounds(margin - 6, 2, logoW, static_cast<int>(layout.headerHeight - 2.0f));

    const int settingsW = juce::jlimit(132, 190, static_cast<int>(150.0f * s));
    const int advancedW = juce::jlimit(104, 154, static_cast<int>(122.0f * s));
    const int topButtonGap = juce::jlimit(5, 10, static_cast<int>(8.0f * s));
    const int buttonY = static_cast<int>(layout.settingsButtonY);
    const int buttonH = static_cast<int>(layout.settingsButtonHeight);

#if SIGNALALIGN_16_INPUT
    const int roundGap = juce::jlimit(2, 5, static_cast<int>(3.0f * s));
    const int preferredDiameter = juce::jlimit(22, 30, buttonH);
    const int fixedButtonsW = settingsW + advancedW + 2 * topButtonGap;
    const int availableForRounds = juce::jmax(15 * 16,
                                               W - 2 * margin - fixedButtonsW);
    const int maxDiameter = juce::jmax(16, (availableForRounds - 14 * roundGap) / 15);
    const int roundDiameter = juce::jlimit(16, preferredDiameter, maxDiameter);
    const int compareRowW = 15 * roundDiameter + 14 * roundGap;
    const int topButtonsW = settingsW + topButtonGap + advancedW + topButtonGap + compareRowW;
    const int topButtonsX = (W - topButtonsW) / 2;
    const int rowY = buttonY + (buttonH - roundDiameter) / 2;

    settingsToggleButton.setBounds(topButtonsX, buttonY, settingsW, buttonH);
    advancedToggleButton.setBounds(topButtonsX + settingsW + topButtonGap,
                                   buttonY, advancedW, buttonH);

    int compareX = topButtonsX + settingsW + topButtonGap + advancedW + topButtonGap;
    for (auto& button : compareInputButtons)
    {
        if (button != nullptr)
        {
            button->setBounds(compareX, rowY, roundDiameter, roundDiameter);
            compareX += roundDiameter + roundGap;
        }
    }
#else
    const int topButtonsW = settingsW + topButtonGap + advancedW;
    const int topButtonsX = (W - topButtonsW) / 2;
    settingsToggleButton.setBounds(topButtonsX, buttonY, settingsW, buttonH);
    advancedToggleButton.setBounds(topButtonsX + settingsW + topButtonGap,
                                   buttonY, advancedW, buttonH);
#endif

    const int gap = layout.compact ? 8 : juce::jlimit(8, 14, static_cast<int>(10.0f * s));
    const int left = margin + (layout.compact ? 14 : 18);
    const int right = left;
    const int usableW = W - left - right;
    const int labelH = juce::jlimit(11, 16, static_cast<int>(14.0f * s));
    const int boxH = juce::jlimit(25, 35, static_cast<int>(28.0f * s));
    const int labelOffset = labelH + 2;

    for (auto* label : { &fftLabel, &overlapLabel, &averagingLabel, &searchLabel, &smoothingLabel, &lowLabel, &highLabel,
                         &minConfidenceLabel, &minCoherenceLabel, &minPeakRatioLabel, &maxPhaseDiffLabel, &maxStabilityLabel })
        label->setFont(juce::Font(juce::FontOptions(juce::jlimit(10.0f, 14.0f, 12.0f * s))));

    const int textBoxW = juce::jlimit(60, 92, static_cast<int>(74.0f * s));
    const int textBoxH = juce::jlimit(19, 27, static_cast<int>(20.0f * s));
    for (auto* slider : { &lowFreqSlider, &highFreqSlider, &minConfidenceSlider, &minCoherenceSlider,
                          &minPeakRatioSlider, &maxPhaseDiffSlider, &maxStabilitySlider })
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, textBoxW, textBoxH);

    const int topY = static_cast<int>(layout.controlsTop)
                   + juce::jlimit(17, 24, static_cast<int>(20.0f * s));

    auto place = [labelOffset, labelH, boxH](juce::Label& label, juce::Component& control,
                                             int x, int y, int width)
    {
        label.setBounds(x, y - labelOffset, width, labelH);
        control.setBounds(x, y, width, boxH);
    };

    // SETTINGS: only controls that are useful in normal operation.
    const int col3 = juce::jmax(150, (usableW - 2 * gap) / 3);
    int x = left;
    place(fftLabel, fftSizeBox, x, topY, col3); x += col3 + gap;
    place(searchLabel, searchBox, x, topY, col3); x += col3 + gap;
    place(smoothingLabel, smoothingBox, x, topY, col3);

    const int rowStep = juce::jlimit(44, 58, static_cast<int>(52.0f * s));
    const int row2Y = topY + rowStep;
    const int actionW = juce::jlimit(90, 130, static_cast<int>(105.0f * s));
    const int basicSliderW = juce::jmax(170, (usableW - actionW - 2 * gap) / 2);
    x = left;
    place(lowLabel, lowFreqSlider, x, row2Y, basicSliderW); x += basicSliderW + gap;
    place(highLabel, highFreqSlider, x, row2Y, basicSliderW);
    freezeButton.setBounds(W - right - actionW, row2Y, actionW, boxH);

    if (advancedVisible)
    {
        // ADVANCED: rarely needed quality and robustness controls. These remain
        // active in the DSP at all times; this section only exposes their values.
        const int row3Y = topY + 2 * rowStep;
        const int col4 = juce::jmax(118, (usableW - 3 * gap) / 4);
        x = left;
        place(overlapLabel, overlapBox, x, row3Y, col4); x += col4 + gap;
        place(averagingLabel, averagingBox, x, row3Y, col4); x += col4 + gap;
        place(minConfidenceLabel, minConfidenceSlider, x, row3Y, col4); x += col4 + gap;
        place(minCoherenceLabel, minCoherenceSlider, x, row3Y, col4);

        const int row4Y = topY + 3 * rowStep;
        const int robustW = juce::jmax(150, (usableW - 2 * gap) / 3);
        x = left;
        place(minPeakRatioLabel, minPeakRatioSlider, x, row4Y, robustW); x += robustW + gap;
        place(maxPhaseDiffLabel, maxPhaseDiffSlider, x, row4Y, robustW); x += robustW + gap;
        place(maxStabilityLabel, maxStabilitySlider, x, row4Y, robustW);
    }
}

