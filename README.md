# ZED

## Overview

ZED, by South Coast Synthesis, is a free filter plug-in featuring four analogue-modelled filter designs. Analogue-inspired input distortion and feedback paths provide distinctive timbral colouring, while modern digital filter-processing techniques support stable self-oscillation and rapid modulation.

Inspired by Ableton Live's filter effect, ZED offers a classic state-variable filter, a Korg MS-10/MS-20-inspired Sallen–Key filter, and two different ladder filters: a Moog-inspired transistor ladder and an ARP-inspired diode ladder. All original signal-processing and interface code was written specifically for ZED using purpose-built DSP functions.

## Download

Download the latest installers from the [ZED releases page](https://github.com/jimurphy/ZED/releases).

For release candidate 1, the installer filenames are:

- macOS: `ZED-1.0.0-rc.1-macOS-arm64.pkg`
- Windows: `ZED-1.0.0-rc.1-Windows-x64-Setup.exe`

## Requirements

### macOS

- An Apple silicon Mac
- A plug-in host supporting AU or VST3 plug-ins

### Windows

- 64-bit Windows 10 or Windows 11
- A 64-bit plug-in host supporting VST3 plug-ins

ZED explicitly supports sample rates of 44.1, 48, 88.2, 96 and 192 kHz. Other sample rates are not restricted and may work, but remain outside the tested specification.

ZED is designed to process mono or stereo audio.

## Installation

Download the appropriate installer package, open it, and follow the on-screen instructions.

On macOS, the plug-ins are installed to:

- AU: `/Library/Audio/Plug-Ins/Components/ZED.component`
- VST3: `/Library/Audio/Plug-Ins/VST3/ZED.vst3`

On Windows, the VST3 is installed to:

- `C:\Program Files\Common Files\VST3\ZED.vst3`

After installation, follow your DAW's instructions for rescanning or accessing third-party plug-ins. Insert ZED as an audio effect on an audio track, or after an instrument on a MIDI track.

The Windows installer is currently unsigned, so Microsoft Defender SmartScreen may display a warning. If you downloaded the installer from the official ZED releases page, select **More info**, followed by **Run anyway**, to continue.

To uninstall ZED:

- On macOS, remove `ZED.component` and `ZED.vst3` from the folders listed above.
- On Windows, uninstall ZED through **Installed apps**, **Apps & features**, or **Add or Remove Programs**, depending on your Windows version.

## Controls and usage

### Input Drive

This parameter applies analogue-inspired saturation at the input stage. Leave it at the default value of `1.0` for no additional input distortion or saturation. Values above `1.0` progressively apply a tanh-based saturation curve. The control defaults and snaps to `1.0`.

### Cutoff

The horizontal slider controls the cutoff frequency of the selected filter. Its current value is displayed in the upper-right corner of the response panel.

### Resonance

The vertical slider controls filter resonance. Its current value is displayed as `RES: __` in the upper-right corner of the response panel.

The filter models support self-oscillation in a manner similar to analogue filters. Their output has been tested to remain finite at the supported sample rates. Self-oscillation may nevertheless be loud, so reduce your monitoring level before experimenting with high resonance settings.

### Filter model and type

The model selector changes the sonic character of ZED's output:

- **SVF — State-Variable Filter:** Inspired by LM13700 OTA analogue filters. This is the cleanest and most predictable filter model.
- **SK — Sallen–Key:** Inspired by Korg MS-10 and MS-20 analogue filters. It provides abundant distortion and strong emphasis around the cutoff frequency.
- **TL — Transistor Ladder:** Modelled on the classic Moog ladder low-pass filter.
- **DL — Diode Ladder:** Inspired by ARP diode-ladder low-pass filters.

The available response types are:

- **LP:** low-pass
- **HP:** high-pass
- **BP:** band-pass
- **BR:** band-reject or notch

The eight available model and response configurations are:

| Filter model | Available responses |
| --- | --- |
| State-Variable Filter | LP, HP, BP, BR |
| Sallen–Key | LP, HP |
| Transistor Ladder | LP |
| Diode Ladder | LP |

Click and drag within the response panel to adjust cutoff and resonance simultaneously. Changing the filter configuration while audio is playing may produce a small click.

## Acknowledgements

Thanks to Martijn Zwartjes for his mentorship, and to Will Pirkle for *Designing Audio Effect Plugins in C++* and the accompanying Sallen–Key filter application note. Pirkle's work helped me understand and implement the topology-preserving transform techniques described by Vadim Zavalishin in *The Art of VA Filter Design*.

Thanks also to Michael Norris, who motivated me to finish ZED by turning to some new-fangled tools.

ZED was originally developed by hand in 2020–2021. Final testing, release-candidate validation and release engineering were assisted by OpenAI Codex.

## Development

Developer documentation, including build and testing instructions, is available in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

Additional release-engineering documentation:

- [macOS packaging and notarisation](scripts/macos/README.md)
- [Windows installer](installer/windows/README.md)

## Licence

Original ZED code is copyright © 2020–2026 James Murphy and is available under the BSD 3-Clause License. See [`LICENSE`](LICENSE) for details.

JUCE, the included font, and other third-party components or assets remain subject to their respective licences. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
