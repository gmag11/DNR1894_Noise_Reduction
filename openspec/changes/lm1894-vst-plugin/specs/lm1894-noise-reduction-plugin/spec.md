## ADDED Requirements

### Requirement: Stereo Linked Dynamic Noise Reduction

The plugin MUST process a stereo signal with two matched audio paths controlled by one shared control path derived from the combined channel content.

#### Scenario: Bright event on one side opens both filters
- **WHEN** a high-frequency transient appears predominantly in only one input channel
- **THEN** the control path MUST increase the bandwidth of both audio channels together

#### Scenario: Stereo image remains stable under adaptive filtering
- **WHEN** the input contains asymmetric left/right content with changing high-frequency energy
- **THEN** the plugin MUST avoid independent per-channel cutoff movement that would audibly pull the stereo image

### Requirement: LM1894-Like Adaptive Bandwidth Range

The plugin MUST implement an adaptive low-pass response whose default operating range approximates the LM1894 reference behavior, with minimum bandwidth near 1 kHz and maximum bandwidth near 35 kHz, and with a first-order style roll-off per channel.

#### Scenario: Default range matches reference intent
- **WHEN** the plugin is loaded with default settings
- **THEN** its adaptive cutoff range MUST target the documented LM1894 operating region rather than a generic de-noiser range

#### Scenario: User can narrow or widen the operating window
- **WHEN** the user changes minimum or maximum bandwidth controls
- **THEN** the adaptive cutoff mapping MUST update without discontinuities or clicks

### Requirement: Cascaded Multi-Stage Processing

The plugin MUST support chaining multiple stages of the same adaptive filtering process in series to achieve steeper effective roll-off and greater hiss reduction than a single stage.

#### Scenario: Stage count is user-selectable
- **WHEN** the user configures the processing topology
- **THEN** the plugin MUST provide a stage-count control that supports at least single-stage and multi-stage operation

#### Scenario: Additional stages increase attenuation capability
- **WHEN** the user increases the number of cascaded stages while keeping the same source material and overall calibration
- **THEN** the plugin MUST produce stronger high-frequency attenuation potential than the single-stage configuration

#### Scenario: All cascaded stages stay linked to one control path
- **WHEN** multiple stages are active
- **THEN** all stages in both channels MUST be driven by the same shared stereo-linked control state rather than independent per-stage detectors

#### Scenario: Global cutoff meaning is preserved across stage counts
- **WHEN** the user switches between different stage counts
- **THEN** the plugin MUST compensate per-stage cutoff so that the user-selected global bandwidth range remains behaviorally consistent within documented tolerances

### Requirement: Control Path Sensitivity And Envelope Behavior

The plugin MUST expose and implement control-path sensitivity plus envelope timing that defaults to an LM1894-like fast attack and slower decay.

#### Scenario: Fast opening on short high-frequency burst
- **WHEN** the input contains a short burst of sufficient high-frequency energy
- **THEN** the control path MUST approach its widened bandwidth state on a sub-millisecond timescale consistent with the LM1894 target behavior

#### Scenario: Slower closing preserves ambience
- **WHEN** the burst ends and the remaining signal energy falls below the opening threshold
- **THEN** the bandwidth MUST decay back toward its narrower state over tens of milliseconds rather than collapsing immediately

### Requirement: Detector-To-Cutoff Mapping Fidelity

The plugin MUST use a non-linear detector-to-cutoff mapping that reflects the LM1894 control behavior documented in the datasheet, rather than a simple linear interpolation of cutoff versus signal level.

#### Scenario: Low-frequency content does not open the filter aggressively
- **WHEN** the sidechain is driven mainly by low-frequency content below approximately 500 Hz
- **THEN** the plugin MUST keep bandwidth opening materially lower than it would for an equal effective bright-sidechain stimulus in the upper-mid and high-frequency region

#### Scenario: Bright-sidechain energy opens the filter strongly
- **WHEN** the sidechain is driven by energy concentrated around the LM1894-sensitive region near 2 kHz to 10 kHz
- **THEN** the detector-to-cutoff mapping MUST open the global cutoff significantly faster than for equally strong low-frequency content

#### Scenario: Nominal tape calibration does not pin the filter to the absolute minimum
- **WHEN** the plugin uses default tape-oriented calibration with reference hiss-like source material
- **THEN** the steady-state cutoff SHOULD settle near a nominal low-bandwidth operating region around 2 kHz rather than staying fixed at the absolute minimum bandwidth limit

### Requirement: Source Calibration Profiles

The plugin MUST support calibration for different source types by providing at least Generic, Tape, FM, and TV/Video operating profiles or equivalent recallable parameter sets.

#### Scenario: FM profile suppresses pilot-driven false opening
- **WHEN** the FM profile is selected
- **THEN** the control path MUST include behavior equivalent to reducing unwanted 19 kHz pilot influence on bandwidth opening

#### Scenario: TV profile suppresses line-frequency artifacts in sidechain
- **WHEN** the TV/Video profile is selected
- **THEN** the control path MUST include behavior equivalent to reducing unwanted influence around 15.734 kHz in the detector path

### Requirement: User Controls, Bypass, And Metering

The plugin MUST provide user-facing controls and feedback sufficient to calibrate the model and compare processed versus unprocessed behavior.

#### Scenario: Core controls are available for calibration
- **WHEN** the user opens the plugin UI
- **THEN** the plugin MUST expose controls for sensitivity, minimum bandwidth, maximum bandwidth, attack, release, source profile, stage count, and bypass

#### Scenario: User can inspect adaptive behavior
- **WHEN** audio is playing through the plugin
- **THEN** the plugin MUST display meter feedback showing current bandwidth opening, estimated reduction activity, or an equivalent representation of control-path state

### Requirement: Real-Time Safe And Sample-Rate Consistent Operation

The plugin MUST behave consistently in real-time use across supported sample rates and host buffer sizes.

#### Scenario: Sample-rate changes preserve intended behavior
- **WHEN** the host changes sample rate between common production rates such as 44.1 kHz, 48 kHz, 96 kHz, and 192 kHz
- **THEN** the plugin MUST preserve equivalent cutoff range and envelope timing within documented tolerances

#### Scenario: Host automation does not introduce audible artifacts
- **WHEN** the host automates plugin parameters during playback
- **THEN** the plugin MUST smooth internal state changes enough to avoid clicks, zipper noise, or unstable filter behavior

### Requirement: Behavioral Validation Against Reference Material

The project MUST include a repeatable validation procedure that compares the plugin behavior against targets derived from the LM1894 datasheet and AN-0390.

#### Scenario: Validation covers dynamics and spectral behavior
- **WHEN** the validation suite is run
- **THEN** it MUST measure at least cutoff range, attack behavior, decay behavior, stereo-link behavior, and multi-stage behavior using synthetic and program-like signals

#### Scenario: Validation produces documented acceptance criteria
- **WHEN** a release candidate is prepared
- **THEN** the project MUST provide documented pass/fail criteria for the LM1894 model rather than relying only on subjective listening notes