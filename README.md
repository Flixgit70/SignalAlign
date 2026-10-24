# SignalAlign 0.2.27 ROBUST

SignalAlign compares two audio signals in REAPER/JUCE:

- Left input = REFERENCE
- Right input = COMPARE
- Positive delay = COMPARE arrives later than REFERENCE

This version is based on the stable pre-multiband code path. The failed 0.2.0 multiband majority-vote architecture is **not** used.

## 0.2.27 - Settings / Advanced split + new defaults

- Normal SETTINGS now shows only FFT Size, Search, Smooth, Low/High analysis band and Freeze.
- ADVANCED exposes Overlap, Averaging, Min Confidence, Min Coherence, Min Peak Ratio, Max Phase Diff and Max Stability.
- ADVANCED is hidden by default; closing SETTINGS also closes ADVANCED.
- New-instance defaults now match the current working setup: FFT 32768, Overlap 75%, Averaging Medium, Search +/-500 ms, Smooth Off, Low 50 Hz, High 15000 Hz, Min Confidence 10%, Min Coherence 15%, Min Peak Ratio 1.00, Max Phase Diff 0.0 ms and Max Stability 0.00 ms.
- DSP algorithms and SignalAlign 16 routing are unchanged.

## 0.2.26 - COMISRatio design compile fix

- Fixes a JUCE `juce::Button::text` name collision in the SignalAlign 16 round-button renderer introduced by the 0.2.25 visual update.
- The shared UI text colour identifier is now `uiText`, so it cannot shadow JUCE Button internals.
- No DSP, GCC/PHAT, coherence, phase, smoothing, delay logic or routing behaviour changed.

## 0.2.25 - COMISRatio visual family

- UI-only design update; delay/coherence/phase DSP and 16-input routing are unchanged.
- Default plugin background now matches COMISRatio: RGB 32/32/32.
- Main module panels use the COMISRatio black module surface with subtle light frames.
- Typography uses the COMISRatio grey (RGB 200/200/200).
- Main blue accent is RGB 0/190/255; coherence green is RGB 0/255/120; warning yellow and error red follow the COMISRatio palette.
- Header wordmark is now **SignalAlign** in the COMISRatio-style bold italic blue treatment with shadow/highlight.
- The K2 logo remains at the left; SignalAlign 16 gets a small **16 INPUT** family badge.
- Buttons, sliders, compare selectors, plot panels and the robust-quality box use the same darker COMISRatio control/module language.


## 0.2.22 - SignalAlign 16 variant

The project now builds a second VST3 alongside the original stereo plug-in:

- **SignalAlign**: original stereo version, Left = REFERENCE, Right = COMPARE.
- **SignalAlign 16**: 16 discrete inputs. Input 1 is always REFERENCE. Inputs 2-16 are selectable COMPARE sources.
- Only one COMPARE source can be active at a time.
- In 0.2.22, a menu button next to SHOW/HIDE SETTINGS selected the COMPARE input; 0.2.24 replaces it with 15 direct round buttons.
- Changing COMPARE resets analysis history/continuity so the newly selected source is reacquired cleanly.
- The selected compare input is stored as an automatable plug-in parameter.
- SignalAlign 16 is VST3-only; the original SignalAlign continues to provide VST3 + Standalone.

## 0.2.24 - round compare button compile fix

- Fixes the JUCE compile error in the SignalAlign 16 round COMPARE button renderer.
- The button renderer no longer collides with juce::Button's private `text` member.
- No DSP or routing behaviour changed.


## FFT size hover information

Hovering over **FFT SIZE** now shows the maximum unambiguous GCC delay for every available FFT size at the host's current sample rate. The limit is calculated as:

`max delay ~= (FFT size / 2 - 2) / sample rate`

The actually usable delay range is always the smaller of this FFT limit and the selected **SEARCH** range.

## Robust delay architecture

### 1. Broadband GCC-PHAT remains master

GCC = Generalized Cross-Correlation. PHAT = Phase Transform.
There is still one broadband GCC-PHAT delay estimate. Narrow bands never vote the final delay.

### 2. Coherence-weighted GCC
Frequency bins with high coherence contribute more strongly. Low-coherence bins are suppressed before the broadband GCC IFFT.

### 3. Energy weighting
Very weak bins are softly rejected. Loud tonal bins are capped rather than allowed to dominate, preserving the PHAT concept.

### 4. Peak Ratio / ambiguity detection
The strongest independent GCC peak is compared with the second strongest peak. Similar peaks indicate an ambiguous measurement.

New parameter:
- **MIN PEAK RATIO**: 1.00 ... 3.00, default **1.15**

Example:
- 1.80 : 1 = clear peak
- 1.05 : 1 = highly ambiguous

### 5. Continuity Tracking
Once a trustworthy delay is established, a remote new peak cannot immediately replace it. A clearly different delay must be confirmed over multiple consecutive frames. This prevents occasional wrong GCC peaks from causing large jumps.

When a real new source is selected, SignalAlign normally reacquires the new delay after a few consistent frames.

### 6. Phase Check
A second delay estimate is calculated from adjacent-bin cross-phase slope. This avoids fragile long-range phase unwrapping.

The phase check is only used as a hard gate when its own phase-quality value is sufficiently high.

New parameter:
- **MAX PHASE DIFF**: 0.0 ... 20.0 ms, default **20.0 ms**
- 0.0 ms disables the hard phase-difference gate.

### 7. Stability
SignalAlign measures the robust variation of accepted raw delay candidates over about one second using median absolute deviation (MAD). This is measured before display smoothing.

New parameter:
- **MAX STABILITY**: 0.00 ... 2.00 ms, default **0.00 ms**
- 0.00 ms disables the stability gate.

### 8. Tonality / sparse-spectrum rejection
A spectral-diversity metric detects cases where only very few frequency bins carry trustworthy information. Combined with an ambiguous peak ratio, strongly tonal/periodic material is rejected rather than displaying a misleading exact delay.

### 9. Band Check is plausibility only
Several frequency ranges check whether the broadband GCC delay is compatible with their cross-phase progression.

The display shows e.g.:

`BAND CHECK 5/6`

This is a **soft confidence factor only**. It never chooses or overrides the broadband GCC delay.

## Quality gates

A delay is displayed only when all applicable hard checks pass:

- Min Confidence
- Min Coherence
- Min Peak Ratio
- Phase difference, when the phase check is reliable and enabled
- Max Stability, once the stability window has enough samples
- Continuity confirmation
- Tonal/ambiguity rejection

Rejected frames do not enter delay smoothing.

## Recommended starting settings

- FFT Size: 65536
- Overlap: 75 %
- Averaging: Medium
- Low: 100 Hz
- High: 10 kHz
- Smooth: 1 s
- Min Confidence: 70 %
- Min Coherence: 70 %
- Min Peak Ratio: 1.15
- Max Phase Diff: 1.0 ms
- Max Stability: 0.50 ms

For difficult programme audio, first observe **Peak Ratio**, **Stability**, **Phase Check** and **Band Check** before increasing smoothing.

## Build

The project expects JUCE at:

`~/JUCE`

Build on macOS:

```bash
./build_macos.sh
```

Do not keep renamed backup plug-ins with a `.vst3` suffix inside REAPER's VST3 scan folders. REAPER will still scan them.


## 0.2.5

- Added **Open Manual...** to the SIGNAL ALIGN help menu.
- Opens `https://ogy.de/l6xz` in the system default browser.
- GCC and PHAT remain expanded as Generalized Cross-Correlation and Phase Transform in the help menu.
- DSP / measurement logic unchanged from 0.2.2 ROBUST.


## 0.2.5 UI
- Original K2 PNG logo embedded as JUCE BinaryData and drawn in the header.
- Central SHOW/HIDE SETTINGS button collapses the complete FFT SIZE through MAX STABILITY block.
- When settings are hidden, Coherence and Phase plots move upward and use the freed space.
- Clicking the K2 logo / SIGNAL ALIGN brand area still opens Help.


## Responsive GUI (0.2.5)

- Resize range expanded to 760 x 820 through 2200 x 1500.
- Header, logo, delay readout and quality panel scale with the window.
- Below 940 px width the settings reflow into a compact multi-row grid instead of shrinking.
- Coherence and phase plots always consume the remaining vertical space.
- Hiding Settings immediately gives the recovered space to the plots.



## 0.2.8 - Extended Phase Difference range

- **MAX PHASE DIFF** now ranges from **0.0 to 20.0 ms**.
- Resolution remains **0.1 ms**.
- Default remains **1.0 ms**.
- DSP behaviour is otherwise unchanged.

## 0.2.7 - Deep vertical fit for small windows

- Coherence and Phase keep their complete Y ranges visible even at much smaller editor heights.
- The layout now uses the real remaining plot height; it no longer invents a minimum height that can push Phase below the window.
- Plot top/bottom insets and y-axis typography can scale down much further.
- At extremely small plot heights, frequency labels disappear before the graph itself is sacrificed.
- Compact settings rows also tighten vertically so they do not overlap the plots.
- Minimum editor size is now 700 x 560 px.
- DSP / measurement logic unchanged.


## 0.2.9 - Last Valid Hold

- The main delay display now retains the last qualified Samples/ms value when the current frame has no trustworthy result.
- Held values are rendered grey with `LAST VALID | NO CURRENT RESULT`.
- The held value is display-only: rejected frames still do not enter smoothing and the live alignment bar is not driven by stale data.
- Before the first qualified result, the display shows `WAITING FOR FIRST VALID RESULT`.


## 0.2.11 - Continuous measurement + 65536 FFT

Factory defaults now match the validated practical setup:

- FFT Size: **65536**
- Overlap: **75 %**
- Averaging: **Medium**
- Search: **+/-250 ms**
- Smooth: **Off**
- Low: **43 Hz**
- High: **14046 Hz**
- Min Confidence: **0 %**
- Min Coherence: **0 %**
- Min Peak Ratio: **1.15**
- Max Phase Diff: **20.0 ms**
- Max Stability: **0.00 ms**
- Alignment green threshold: **1.9 ms**
- Alignment red threshold: **7.9 ms**


## 0.2.11 changes

- AUTO FIND has been removed. SignalAlign now analyses continuously whenever FREEZE is off.
- FFT Size now includes **65536**, which is the new factory default.
- Factory defaults follow the current production reference: Overlap 75 %, Averaging Medium, Search +/-250 ms, Smooth Off, Low 50 Hz, High 15000 Hz, Min Confidence 10 %, Min Coherence 15 %, Min Peak Ratio 1.15, Max Phase Diff 20.0 ms, Max Stability 0.00 ms.


## 0.2.12 - true output smoothing

- Delay Smoothing is now a true output/display time constant.
- A continuity-confirmed jump to a new GCC peak no longer resets the output smoother.
- With Smooth = 5 s, a new accepted delay now approaches gradually instead of jumping immediately.
- Smooth = Off remains immediate.
- Factory defaults are explicitly aligned with the production reference: Low 50 Hz, High 15000 Hz, Min Confidence 10 %, Min Coherence 15 %.


## 0.2.13 - dual correction display

The alignment bar now shows both equivalent ways to correct the measured offset at the same time:

- **REFERENCE** on the left: how much delay must be increased or reduced on the reference/left path.
- **COMPARE** on the right: the exact opposite delay change for the compare/right path.

With the established sign convention (positive delay = COMPARE arrives later), the correction math is:

- REFERENCE correction = measured delay
- COMPARE correction = - measured delay

The correction fields follow the smoothed displayed delay. A held Last Valid result remains visible in grey.


## 0.2.14 - About version

- Added an explicit **About SignalAlign...** entry to the logo/help menu.
- About always shows the current plugin version via `JucePlugin_VersionString`.
- The help-menu header also reads the version dynamically from the plugin build.


## 0.2.15 - larger correction arrows

- The REFERENCE / COMPARE correction display above the delay bar is significantly larger for live readability.
- Text actions INCREASE / REDUCE are replaced by direction arrows:
  - right arrow = increase delay
  - left arrow = reduce delay
- The display remains symmetric: REFERENCE and COMPARE always show opposite corrections.


## 0.2.17 - configurable delay readout precision

- Right-click the main Samples readout to choose 0-4 decimal places.
- Right-click the main ms readout to choose 0-4 decimal places.
- Factory defaults: Samples = 0 decimals, ms = 2 decimals.
- The ms correction readouts above the alignment bar follow the selected ms precision.
- Precision settings are stored with the plug-in state.
- About reports version 0.2.17 automatically from the JUCE plug-in version.

## 0.2.16 - large coloured correction arrows

- Correction arrows above the delay bar are now drawn as large vector graphics for much better live readability.
- Right arrow = increase delay and is shown in green.
- Left arrow = reduce delay and is shown in red.
- REFERENCE / COMPARE labels and the correction amount remain beside the arrows.
- Held Last Valid corrections keep the same direction colours but are dimmed.


## 0.2.20 - extended delay search

- Delay SEARCH choices extended to +/-500 ms, +/-750 ms and +/-1000 ms.
- Default remains +/-250 ms.
- FFT SIZE now also offers 131072 samples; the default remains 65536.
- The effective searchable lag is bounded by about FFT_SIZE/2 samples.
- At 48 kHz, FFT 131072 provides about +/-1365 ms of GCC lag range, so the new +/-1000 ms search is fully usable.
- At 96 kHz, FFT 131072 reaches about +/-682 ms; +/-1000 ms would require a still larger FFT.
- About reports version 0.2.20 automatically from the JUCE plug-in version.


## 0.2.19 - decimal precision compile fix

- Fixes the macOS/Clang compile error in the 0.2.17 correction display.
- The correction-bar renderer now reads the ms decimal setting in its own scope.
- Samples default remains 0 decimals; milliseconds default remains 2 decimals.
- Right-click precision selection and saved plug-in state remain unchanged.
- Removed one unused local variable warning in the editor paint routine.
- About reports version 0.2.19 automatically from the JUCE plug-in version.


## 0.2.19
- Decimal precision display fix: selecting 0 decimals now renders a true integer with no decimal separator or trailing fractional digit.
- Defaults remain 0 decimals for samples and 2 decimals for milliseconds.

## 0.2.24

- SignalAlign 16 now uses 15 round, exclusive COMPARE buttons (Inputs 2-16) in one row beside SHOW/HIDE SETTINGS.
- Input 1 remains fixed as REFERENCE.
- Selected COMPARE input is highlighted in blue; changing it resets analysis history and reacquires the delay.
