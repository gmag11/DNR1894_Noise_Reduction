# DNR1894 - Dynamic Noise Reduction Plugin

A digital recreation of the **LM1894 Dynamic Noise Reduction (DNR)** integrated circuit for non-complementary noise reduction (no prior encoding required).

This plugin implements the core audible behavior of the DNR system, originally designed for stereo audio applications (tape, FM, TV) where high-frequency hiss reduction is desired without compromising signal integrity.

---

## Key Features

- **Precise Functional Modeling:** Dynamic simulation of spectral response and temporal envelopes of the original silicon.
- **Stereo Link:** A single control path for both channels (L+R), preserving stereo image stability.
- **Adaptive Bandwidth:** Variable low-pass filter with a nominal range of 1 kHz to 35 kHz.
- **Multi-Stage Architecture:** Support for up to 4 cascaded stages for steeper roll-off (V1 supports automatic frequency compensation).
- **Calibration Profiles:** Preset modes for Tape, FM, TV, and Generic (including standard-specific notch filters).
- **Full Control:** Adjustable sensitivity, dynamic attack/decay times, and bandwidth limits.

---

## Reference Specifications (V1)

The DSP engine is designed to meet the following performance targets (based on datasheet Rev C):

| Parameter | Typical Value | Notes |
| --- | --- | --- |
| Frequency Range | 1 kHz - 35 kHz | User-configurable |
| Attack Time | 0.5 ms | Fast opening for transients |
| Decay Time | 60 ms | Smooth closing to preserve ambience |
| Slope | -6 dB/oct | Per stage (configurable up to -24 dB/oct) |

### Cascade Compensation Law

To keep the global cutoff point independent of the number of stages ($N$), the plugin applies the compensation law:

$$f_{c,stage} = \frac{f_{c,global}}{\sqrt{10^{0.3/N} - 1}}$$

---

## Requirements and Building

### Prerequisites

- CMake 3.20+
- Audio SDKs (JUCE included as a submodule)
- C++17 compatible compiler (MSVC, Clang, GCC)

### Build Instructions

1. Clone the repository including submodules:

   ```bash
   git clone --recursive https://github.com/gmag11/DNR1894_Noise_Reduction.git
   ```

2. Run the build script or use CMake:

   ```powershell
   ./build.ps1
   ```

   Or manually:

   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build . --config Release
   ```

---

## Development and Validation

This project uses **OpenSpec** for requirements management and experimental design. All architectural decisions and functional changes are documented in [openspec/changes/lm1894-vst-plugin/design.md](openspec/changes/lm1894-vst-plugin/design.md).

Objective validation is performed by measuring:

1. Accuracy of the Detector -> Frequency mapping curve.
2. Stereo image stability under sudden dynamic changes.
3. Temporal envelope consistency across different sample rates.

---

## License

This project is a tool for technological preservation and digital recreation. See the license file for details (where applicable).

*Disclaimer: DNR is a trademark of National Semiconductor / Texas Instruments. This project is a reverse-engineered digital implementation for creative and restorative use.*
