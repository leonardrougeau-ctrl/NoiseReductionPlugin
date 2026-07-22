# Noise Reduction VST3 Plugin

**Version:** v1.0.1

A real-time noise reduction plugin using spectral subtraction. Designed for cleaning up dialogue, vocals, and instrument recordings with minimal artifacts and low latency.

---

## Features

- ✅ **Spectral subtraction algorithm** — Effective broadband noise reduction
- ✅ **Real-time noise profile learning** — Click "Learn Noise" to capture the noise floor
- ✅ **Adjustable reduction amount** — Control the strength of the noise reduction
- ✅ **Difference Mode** — Audition only the removed noise for fine-tuning
- ✅ **Built-in spectrum analyzer** — Visual feedback of the frequency response
- ✅ **Low latency** — ~46ms at 44.1kHz
- ✅ **Preset saving and loading** — Save and recall your favorite settings
- ✅ **Code-signed with SSL.com certificate** — Trusted and verified
- ✅ **Hard clipper at output** — Prevents clipping from overlap-add artifacts
- ✅ **Output gain in decibels** — Display shows dB (0.0 dB at unity gain)
- ✅ **Noise profile persists across bypass** — Profile stays intact when bypassing and re-enabling the plugin

---

## System Requirements

- Windows 10 or 11 (64-bit)
- VST3-compatible DAW (FL Studio, Reaper, Ableton, Cubase, etc.)
- 4GB RAM minimum
- 64-bit processor

---

## Installation

1. **Download** the latest release from [clearwatercodes.com](https://clearwatercodes.com)
2. **Extract** the ZIP file
3. **Copy** the `Noise Reduction.vst3` folder to:
   - `C:\Program Files\Common Files\VST3\`
4. **Restart** your DAW
5. **Scan** for new plugins in your DAW's plugin manager

---

## How to Use

1. **Load** the plugin on a track with unwanted noise
2. **Play** the audio and click **"Learn Noise"** during a quiet section (noise only)
3. **Adjust** the **Reduction** knob to taste
4. **Toggle Difference Mode** to hear what's being removed
5. **Adjust Output Gain** to set the final level (displayed in dB)

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Plugin not showing up | Restart your DAW and rescan plugins |
| Audio dropouts | Increase your DAW's buffer size |
| Noise profile lost on bypass | Fixed in v1.0.1 — profile now persists |
| Clipping at output | Reduced in v1.0.1 — hard clipper added |

---

## Recent Updates (v1.0.1)

- **Output Clipping Fix**: Added a hard clipper at the output stage to prevent peaks above 0 dBFS caused by overlap-add artifacts.
- **Output Gain Display**: Now shows values in decibels (`0.0 dB` at unity gain) for better usability.
- **Noise Profile Persistence**: The noise profile now persists across bypass/re-enable — no need to re-learn after bypassing.

---

## Credits

**Developer:** Leonard Rougeau — ClearWaterCodes  
**Website:** [https://clearwatercodes.com](https://clearwatercodes.com)  
**Contact:** [contact@clearwatercodes.com](mailto:contact@clearwatercodes.com)

---

## License

Free for personal and commercial use. See [LICENSE](LICENSE) for details.

---

## Changelog

### v1.0.1 (2026-07-21)
- Added hard clipper at output to prevent clipping
- Output gain display now in decibels
- Noise profile persists across bypass/re-enable
- Improved stability and performance

### v1.0.0 (2026-07-12)
- Initial release
- Spectral subtraction algorithm
- Learn Noise / Difference Mode / Spectrum Analyzer
- Preset saving and loading
- Code-signed with SSL.com certificate