# Third-party notices and licensing exclusions

The root BSD 3-Clause LICENSE covers only original ZED code authored by James
Murphy. It does not cover third-party code, assets, generated copies of assets,
or portions whose independent authorship is unresolved. No third-party notices
have been removed. These exclusions also apply to historical copies.

## JUCE and SDK material

CMake FetchContent pins **JUCE 8.0.6**, commit
`51a8a6d7aeae7326956d747737ccf1575e61e209`. JUCE is separately licensed under
its commercial/open-source dual-licensing system (JUCE 8 offers AGPLv3 as its
open-source basis). It is not BSD-licensed by this repository. Consult the
[licence supplied with that exact revision](https://github.com/juce-framework/JUCE/blob/51a8a6d7aeae7326956d747737ccf1575e61e209/LICENSE.md).
James Murphy's JUCE Starter licence does not transfer to downstream users.
Anyone building or distributing JUCE-based derivatives must independently arrange
and comply with an appropriate JUCE licensing basis and dependency terms.

JUCE's bundled SDKs and other dependencies retain their own notices and terms;
consult the pinned checkout rather than treating this file as a replacement for
those licences. Legacy Projucer-generated `JuceLibraryCode/` and `Builds/` are
retained for history, not a new BSD grant over JUCE or SDK material. No additional
JUCE or SDK source was vendored for the migration.

## Font and embedded copy

`Source/Galvji.ttc` is a third-party font, also embedded in the retained generated
`JuceLibraryCode/BinaryData.cpp`. Both the font and its embedded bytes are
**excluded** from the BSD grant. The repository does not establish its applicable
redistribution licence or permission to embed/distribute it. Its licensing status
remains unresolved; this migration grants no font rights. Obtain the applicable
font terms and confirm distribution permission before public distribution.
Other third-party assets or artwork, if present, are likewise excluded.

## DSP provenance requiring clarification

The following files are retained unchanged. Original James Murphy-authored
portions are within the BSD grant; uncertain or third-party portions are explicitly
excluded. No claim is made that these mixed-provenance files are wholly BSD.
Paths below are relative to `Source/jdsplib/`.

| Files | Existing attribution / unresolved boundary |
| --- | --- |
| `ZDSVF.h`, `ZDSK.h`, `ZDSKHPF.h`, `ZDML.h`, `ZDDL.h`, `ZDOnePole.h`, `ZDOnePoleEx.h` | Pirkle/Zavalishin app-note and implementation references; independent implementation versus adapted code unresolved |
| `DSPMath.h` | Martijn Zwartjes, Aleksey Vaneev, an external gist and Arduino routines; applicable terms unresolved |
| `OnePoleLP.h`, `DCBlocker.h` | MusicDSP / Stanford implementation references |
| `DelayLin.h`, `DiffusionDelayLin.h`, `RevMZ.h` | Explicit derivation from Martijn Zwartjes code |
| `Adsr.h`, `lfo.h`, `oscillatormm.h` | Martijn/Pirkle implementation references |
| `CombFilterFB.h`, `CombFilterFBLP.h`, `CombFilterFF.h` | Stanford/Freeverb references |
| `RevMini.h`, `voice.h` | Provenance not sufficiently established by the existing audit |

Algorithm citations do not by themselves prove copied code, but an author header
alone does not resolve the rights in attributed implementations. No licence is
invented for these portions. The earlier audit in `RC1_RELEASE_CONFIG.md` records
the evidence; clarification or applicable permissions remain necessary for any
claim that the complete bundled code can be redistributed under BSD alone.
