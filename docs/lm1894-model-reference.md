# LM1894 Model Reference — V1

This document consolidates the datasheet-derived targets, tolerances and mapping
curves used as the single source of truth for the DSP implementation.

Sources: TI LM1894 datasheet (Rev C) and National Semiconductor AN-0390.

---

## 1. Cutoff Range (task 1.1)

| Parameter | Min | Typ | Max | Unit |
|---|---:|---:|---:|---|
| Minimum bandwidth (AC gnd pin 9) | 675 | 965 | 1400 | Hz |
| Maximum bandwidth (DC gnd pin 9) | 27 000 | 34 000 | 46 000 | Hz |

**Plugin defaults:** `minBandwidthHz = 1000 Hz`, `maxBandwidthHz = 35000 Hz`.

## 2. Stereo Link (task 1.1)

Both audio channels share a single control path derived from the sum of L+R.
The summing amplifier has unity gain (typ 1 V/V, ±0.1).

## 3. Envelope Timing (task 1.1)

Measured to 90 % of final value with a 10 kHz tone burst:

| Parameter | Min | Typ | Max | Unit |
|---|---:|---:|---:|---|
| Attack time | 300 | 500 | 700 | µs |
| Decay time | 45 | 60 | 75 | ms |

**Plugin defaults:** `attackMs = 0.5`, `releaseMs = 60`.

## 4. Parameter Translation (task 1.2)

| User parameter | Internal meaning | Range | Default |
|---|---|---:|---:|
| `sensitivityDb` | Offset applied to sidechain level before detector | -18 … +18 dB | 0 dB |
| `minBandwidthHz` | Lower clamp of adaptive cutoff | 800 … 4000 Hz | 1000 Hz |
| `maxBandwidthHz` | Upper clamp of adaptive cutoff | 8000 … 40000 Hz | 35000 Hz |
| `attackMs` | Envelope follower attack (to 90 %) | 0.05 … 5.0 ms | 0.5 ms |
| `releaseMs` | Envelope follower decay (to 90 %) | 10 … 250 ms | 60 ms |
| `stageCount` | Number of cascaded 1st-order LPF stages | 1 … 4 | 1 |
| `sourceProfile` | Sidechain weighting / notch preset | generic, tape, fm, tv | tape |
| `bypass` | Hard bypass | off / on | off |
| `outputTrimDb` | Post-process gain trim | -12 … +12 dB | 0 dB |

Conversion flow: Normalised host value → `ParameterLayout` → `Lm1894Parameters` struct → `Lm1894Model::setParameters()`.

## 5. Tolerances (task 1.3)

| Metric | Target | Acceptable tolerance |
|---|---|---|
| Attack time (90 %) | 0.5 ms | ±0.3 ms |
| Decay time (90 %) | 60 ms | ±20 ms |
| Min bandwidth default | 1000 Hz | ±200 Hz |
| Max bandwidth default | 35 kHz | ±5 kHz |
| Roll-off slope per stage | −6 dB/oct | ±1 dB/oct at 2× fc |

## 6. Cascade Compensation Law (task 1.4)

For N identical 1st-order stages, the combined −3 dB point drops.
To preserve the user-facing global cutoff meaning:

$$f_{c,stage} = \frac{f_{c,global}}{\sqrt{10^{0.3/N} - 1}}$$

| N | Compensation factor (fc_stage / fc_global) |
|---:|---:|
| 1 | 1.000 |
| 2 | 1.555 |
| 3 | 1.962 |
| 4 | 2.298 |

## 7. Detector-to-Cutoff Mapping Curve (task 1.5)

Internal detector equivalent variable `v_eq` is normalised to the LM1894
electrical range (1.1 V – 3.8 V). The mapping to global cutoff uses
interpolation in log₁₀(frequency):

| v_eq (V) | Cutoff target (Hz) |
|---:|---:|
| 1.10 | 1 000 |
| 1.25 | 1 300 |
| 1.35 | 1 800 |
| 1.50 | 2 500 |
| 1.75 | 4 000 |
| 2.05 | 7 000 |
| 2.40 | 12 000 |
| 2.80 | 20 000 |
| 3.20 | 28 000 |
| 3.80 | 35 000 |

Normalised form used by the implementation:

$$u = \frac{v_{eq} - 1.1}{3.8 - 1.1}$$

$$\log_{10}(f_c) = \log_{10}(f_{min}) + s(u)\,\bigl[\log_{10}(f_{max}) - \log_{10}(f_{min})\bigr]$$

where `s(u)` is the shape function derived from the table above, preserving
relative log-frequency distribution when min/max are changed.

## 8. Sidechain Weighting (task 1.6)

The digital sidechain filter chain approximates the LM1894 control-path
frequency sensitivity:

| Stage | Type | Purpose |
|---|---|---|
| High-pass | 2nd-order HPF, fc ≈ 200 Hz | Reject sub-bass / DC, prevent low-freq from opening the filter |
| Emphasis | 1st-order shelf or peak, boost onset ≈ 1.5 kHz, +12 dB by 6 kHz | Weight the 2–10 kHz region where hiss is most audible |
| Notch (FM only) | 2nd-order notch, fc = 19 kHz, Q ≈ 5 | Suppress FM stereo pilot |
| Notch (TV only) | 2nd-order notch, fc = 15.734 kHz, Q ≈ 5 | Suppress TV line-scan frequency |
| Anti-alias LPF | 1st-order, fc = 20 kHz | Prevent ultrasonic content from biasing the detector |

**Profile mapping:**

| Profile | HPF | Emphasis | 19 kHz notch | 15.7 kHz notch | LPF |
|---|:---:|:---:|:---:|:---:|:---:|
| generic | ✓ | ✓ | — | — | ✓ |
| tape | ✓ | ✓ | — | — | ✓ |
| fm | ✓ | ✓ | ✓ | — | ✓ |
| tv | ✓ | ✓ | — | ✓ | ✓ |

The `tape` and `generic` profiles differ only in default sensitivity offset (tape is calibrated so hiss-like noise settles near 1.8–2.2 kHz nominal cutoff).
