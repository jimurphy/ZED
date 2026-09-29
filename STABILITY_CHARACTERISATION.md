# ZED stability characterisation

> Historical validation record. Commands, output paths and applicable commit IDs
> have been adapted to the standalone repository; the recorded results were not
> rerun during migration unless listed in MIGRATION.md.

ZED is tested and supported at **44.1, 48, 88.2, 96 and 192 kHz**.
Other sample rates are permitted but are outside the supported specification.
The internal 32 kHz findings are retained below; no production DSP correction
was required for this milestone and none was made.

## A. Confirmed behaviour

This is an investigation of existing DSP, not a correction. The branch
`zed-stability-characterisation` began at clean, fetched `origin/master`
`4324fecd5ef2dacadb94460c1ad8fc32324dde87`. Production files, existing tests,
parameter contracts, JUCE pin and normal plug-in configuration are unchanged.
Only the optional measurement executable, its summary script, CMake registration
inside `ZED_BUILD_TESTS`, and this report are new. No production API was added:
the separate executable reuses the existing private `ZedLifecycleTestAccess`
friendship and does not link the other definition in `LifecycleTests.cpp`.

### Parameter timing and smoothing

| Parameter | Read in `processBlock()` | DSP application |
| --- | --- | --- |
| `cutoff` | Raw atomic read inside the sample loop | One 4 Hz `OnePoleLP` update per sample, shared by left/right; smooths pitch before frequency conversion |
| `resonance` | Raw atomic read inside the sample loop | One independent 4 Hz `OnePoleLP` update per sample, shared by left/right; then model-specific scaling |
| `drive` | Eight separate raw atomic reads at block entry, one per model/channel setter | Unsmooth, block-level update; no single shared snapshot across those reads |
| `filterConfiguration` | One raw atomic snapshot at block entry | Existing destination-engine reset and response selection, once per block |

There is no consumption of host automation events with intra-block sample
offsets. This is **block-level configuration/drive selection plus per-sample
atomic polling and smoothing for cutoff/resonance**, not sample-accurate host
automation. A concurrent parameter write could be observed by cutoff/resonance
mid-block, but its timing is not tied to a host event offset. Drive's separate
reads could theoretically disagree during a concurrent write; that race in
observation timing was not deliberately induced in this deterministic harness.

`OnePoleLP` implements `x = float(exp(-2*pi*4/fs))`, then
`y = (1-x)*input + x*previous`, storing float history. It is not JUCE
`SmoothedValue`, has no target counter or finite linear-ramp duration, and does
not promise exact arrival. Ideal time constant is **39.7887 ms**; 90% is
**91.6169 ms**, 99% **183.234 ms**. Each new atomic value is the next target.
Both channels use the same once-per-sample result. Zero-sample blocks advance
neither smoother.

`prepareToPlay()` recalculates both smoothers at the supplied double sample
rate, then `reset()` seeds their current values directly from current parameters.
Processor reset and release also synchronise them to current parameters rather
than retaining a trajectory. Preparation, reset and re-preparation checks passed
without a zero/stale starting value. The existing filter history/reset policy is
unchanged: SVF responses share an engine; SK LP and HP are distinct; transistor
and diode are separate engines. Activation resets both destination channels,
including on an empty block; subsequent same-engine blocks do not repeat reset.

### Coefficient audit

The active production path uses double stored sample rates with float audio and
many float coefficient intermediates; no host-rate integer truncation was found.
`p2f` uses a float `exp2f` conversion, finite over the declared pitch domain.
No active square-root or logarithm with a restricted domain was found (the
unused `ftom` utility contains a logarithm).

- SVF uses prewarped `tan(pi*f/fs)` and denominator `1+2*r*g+g*g`.
  Its existing `fs/2.5` cutoff cap stays below Nyquist; `r >= -0.2`, so for
  nonnegative `g` that denominator is at least 0.96.
- SK LP/HP and their `ZDOnePole` children use `g/(1+g)` and feedback divisions.
  The processor's resonance scaling gives `k <= 1.8`; for `G` in [0,1],
  `1-k*G+k*G*G >= 0.55`. The guarded output division by `k` has a positive
  minimum under the declared resonance range. SK coefficient updates precede
  the new resonance setter in the sample loop: feedback coefficients use the
  previous sample's resonance while the current `k` is applied in DSP.
- Transistor ladder and its four `ZDOnePole` children use an existing `fs/3.5`
  cap. Its actual feedback denominator is `1+k*g*g*g`, positive for the
  supported domain. This report does not reinterpret or redesign that equation.
- Diode ladder and its four `ZDOnePoleEx` children use nested denominators
  `1+g-...` and `1+k*gamma`. These and SK's uncapped tangent become suspect if
  cutoff crosses Nyquist. At supported rates the sampled coefficients and
  history were finite throughout the tests.
- `tanh` saturation does not prove that every intermediate state is bounded.
  Existing `fasttanh` is a rational polynomial approximation; extreme finite
  arguments can overflow intermediate arithmetic. The reported tests use
  explicitly bounded deterministic input, not every possible float input.

The smallest sampled absolute feedback denominator was **0.550004**. This is a
block-end diagnostic over selected denominators, not a proof covering every
intermediate expression or every sample. Output is checked sample by sample
after each processing call; the first returned non-finite sample is recorded.

### Cutoff actually supplied to filters

Pitch limits remain 12–135, step 1; default 57. Resonance remains 0.01–1.1,
step 0.01, default 0.7; drive remains 0.5–5, step 0.01, default 1.
The following values are directly inspected after preparation. SVF values
converted back from `wd` have insignificant float rounding in the last digits.
After an automated change, smoother settling error can leave a slightly
different cutoff; the caps and conversion themselves are unchanged.

| Hz | Minimum, all engines (Hz) | SVF maximum | SK LP/HP maximum | Transistor maximum | Diode maximum |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 44100 | 16.351593 | 17640 | 19912.060547 | 12600 | 19912.060547 |
| 48000 | 16.351593 | 19200 | 19912.060547 | 13714.286133 | 19912.060547 |
| 88200 | 16.351593 | 19912.0602 | 19912.060547 | 19912.060547 | 19912.060547 |
| 96000 | 16.351593 | 19912.0602 | 19912.060547 | 19912.060547 | 19912.060547 |
| 192000 | 16.351593 | 19912.0602 | 19912.060547 | 19912.060547 | 19912.060547 |

## B. Supported-rate results

No NaN/Inf, invalid sampled coefficients/history, crash, assertion or uncontrolled
growth was observed at the five supported rates in the completed Release
measurements. A finite output above unity is not classified as a failure.
The largest observed peak was **2.17484092712**, transistor LP at 192 kHz,
maximum cutoff/drive and minimum resonance with DC input; RMS 2.17360019109,
mean 2.17336116354. The DC mean is expected for that input, not evidence of
self-oscillation. No output normalization, gain control or limiting was used.

Coverage:

- **7,040 static cases:** five rates × eight configurations × mono/stereo ×
  eleven control profiles × eight signals. Each renders at least 4,096 frames
  or 0.1 seconds, whichever is longer, with a variable block sequence.
- Signals: silence, impulse, DC, 40 Hz, 1 kHz and `0.45*fs` sines,
  deterministic broadband noise, and logarithmic 20 Hz–`0.45*fs` sweep.
  Sines/noise have amplitude 0.25, impulse 0.5 (right -0.4), DC 0.25
  (right -0.2); stereo noise and sine phases differ between channels.
- Profiles `(pitch, resonance, drive)`:
  `(57,.7,1)`, `(12,.01,.5)`, `(135,1.1,5)`, `(12,1.1,1)`,
  `(135,.01,5)`, `(76,.5,2.5)`, `(57,.7,5)`, `(135,.7,1)`,
  `(12,.7,1)`, `(69,1.05,5)`, `(69,1,1)`.
- **240 dynamic boundary cases:** every rate/configuration/layout, two seconds
  each of an abrupt minimum-to-maximum cutoff target, a smoothed up/down cutoff
  triangle, or repeated unsmoothed drive changes. Resonance is maximum; drive
  is maximum except during the drive schedule. All were finite, including
  maximum cutoff at 44.1 kHz. Maximum boundary peak **2.08451080322**.
- **336 mono oscillation trials:** 20 ms noise excitation, eight one-second
  silence windows at resonance 1.05/1.1 and pitches 12/69/135, then six seconds
  of silence after setting resonance to .01, without reset. At 48 kHz the
  same grid adds resonance .7/.8/.9/1.0 for descriptive onset estimates.
- **126 extended trials:** all rate/configuration/cutoff combinations in
  stereo at drive 5/resonance 1.1 with 12 seconds high-resonance silence and
  10 seconds recovery; plus six focused 60-second buildup trials below.
  CSVs retain consecutive one-second peak, RMS and DC-mean measurements.

### Per-configuration and rate summary

The 8-second RMS triplet is at low/mid/high pitch (12/69/135), resonance 1.1,
drive 1. An audible sustained alternating signal in these windows is intended
self-oscillation; a tiny residual or zero is not labelled audible oscillation.
Peak includes static, boundary, ordinary and extended trials. Recovery is the
worst final one-second window across ordinary and extended trials. All rows
share the boundary result shown; no additional supported-rate numerical anomaly
was found. Model-specific observations follow the table.

| Hz | Configuration | Finite | Peak | 8s RMS: low / mid / high | Final recovery peak / RMS | Boundary |
| ---: | --- | --- | ---: | --- | --- | --- |
| 44100 | SVF: LP | yes | 0.976396 | 0.0305303 / 0.157052 / 0.431496 | 2.13416e-15 / 2.13416e-15 | finite |
| 44100 | SVF: HP | yes | 2.04684 | 0.0305414 / 0.159209 / 0.481666 | 4.29239e-16 / 4.29239e-16 | finite |
| 44100 | SVF: BP | yes | 0.975667 | 0.0305395 / 0.157974 / 0.453 | 4.29284e-16 / 4.29284e-16 | finite |
| 44100 | SVF: BR | yes | 1.27242 | 0.0122184 / 0.0635408 / 0.0740277 | 1.70493e-15 / 1.70493e-15 | finite |
| 44100 | Sallen-Key: LP | yes | 0.633775 | 0.216656 / 0.210038 / 0.184206 | 4.23502e-23 / 4.23502e-23 | finite |
| 44100 | Sallen-Key: HP | yes | 0.54126 | 0.119511 / 0.116685 / 0.102338 | 2.70401e-21 / 2.70401e-21 | finite |
| 44100 | Transistor ladder: LP | yes | 2.00222 | 0.248195 / 0.308006 / 0 | 3.14361e-18 / 3.14325e-18 | finite |
| 44100 | Diode ladder: LP | yes | 2.10724 | 0.0123965 / 0.0608072 / 0.0970044 | 8.84656e-18 / 8.84656e-18 | finite |
| 48000 | SVF: LP | yes | 0.976396 | 0.0292894 / 0.150656 / 0.431492 | 2.32245e-15 / 2.32245e-15 | finite |
| 48000 | SVF: HP | yes | 2.06748 | 0.0293019 / 0.152544 / 0.481659 | 4.67187e-16 / 4.67187e-16 | finite |
| 48000 | SVF: BP | yes | 0.979232 | 0.0292399 / 0.151456 / 0.453005 | 4.67248e-16 / 4.67248e-16 | finite |
| 48000 | SVF: BR | yes | 1.29242 | 0.0116983 / 0.0608924 / 0.0740334 | 1.85526e-15 / 1.85526e-15 | finite |
| 48000 | Sallen-Key: LP | yes | 0.636154 | 0.215908 / 0.210535 / 0.156478 | 4.61069e-23 / 4.61069e-23 | finite |
| 48000 | Sallen-Key: HP | yes | 0.582006 | 0.11952 / 0.11696 / 0.0869321 | 2.95075e-21 / 2.95075e-21 | finite |
| 48000 | Transistor ladder: LP | yes | 2.00222 | 0.248662 / 0.303736 / 6.54618e-08 | 3.14367e-18 / 3.14355e-18 | finite |
| 48000 | Diode ladder: LP | yes | 1.85985 | 0.0118607 / 0.0585271 / 0.117458 | 8.68171e-18 / 8.68171e-18 | finite |
| 88200 | SVF: LP | yes | 0.9363 | 0.0216127 / 0.111547 / 0.544925 | 4.26272e-15 / 4.26272e-15 | finite |
| 88200 | SVF: HP | yes | 2.08451 | 0.0216246 / 0.112336 / 0.662691 | 8.58456e-16 / 8.58456e-16 | finite |
| 88200 | SVF: BP | yes | 0.977177 | 0.0215655 / 0.111856 / 0.592039 | 8.5841e-16 / 8.5841e-16 | finite |
| 88200 | SVF: BR | yes | 1.29514 | 0.00862711 / 0.0448693 / 0.198 | 3.40426e-15 / 3.40426e-15 | finite |
| 88200 | Sallen-Key: LP | yes | 0.637688 | 0.215715 / 0.213086 / 0.0399286 | 8.47023e-23 / 8.47023e-23 | finite |
| 88200 | Sallen-Key: HP | yes | 0.628539 | 0.119587 / 0.118413 / 0.0221826 | 5.4143e-21 / 5.4143e-21 | finite |
| 88200 | Transistor ladder: LP | yes | 2.08388 | 0.246786 / 0.279745 / 6.90915e-19 | 3.14434e-18 / 3.14406e-18 | finite |
| 88200 | Diode ladder: LP | yes | 1.52046 | 0.00876619 / 0.0442025 / 0.157358 | 8.52938e-18 / 8.52938e-18 | finite |
| 96000 | SVF: LP | yes | 0.934341 | 0.0207039 / 0.106942 / 0.540665 | 4.6393e-15 / 4.6393e-15 | finite |
| 96000 | SVF: HP | yes | 2.07586 | 0.0207337 / 0.107644 / 0.653747 | 9.34424e-16 / 9.34423e-16 | finite |
| 96000 | SVF: BP | yes | 0.979521 | 0.0206767 / 0.107241 / 0.585995 | 9.34324e-16 / 9.34324e-16 | finite |
| 96000 | SVF: BR | yes | 1.30996 | 0.00827151 / 0.0430086 / 0.203493 | 3.70488e-15 / 3.70488e-15 | finite |
| 96000 | Sallen-Key: LP | yes | 0.636152 | 0.215324 / 0.213393 / 0.0529893 | 9.20951e-23 / 9.20951e-23 | finite |
| 96000 | Sallen-Key: HP | yes | 0.61948 | 0.119594 / 0.118515 / 0.0294385 | 5.90004e-21 / 5.90004e-21 | finite |
| 96000 | Transistor ladder: LP | yes | 2.10502 | 0.24614 / 0.277242 / 6.39138e-19 | 3.14446e-18 / 3.14446e-18 | finite |
| 96000 | Diode ladder: LP | yes | 1.49329 | 0.00839262 / 0.0424709 / 0.156873 | 8.52654e-18 / 8.52654e-18 | finite |
| 192000 | SVF: LP | yes | 0.860357 | 0.0146299 / 0.0757673 / 0.454061 | 9.27698e-15 / 9.27659e-15 | finite |
| 192000 | SVF: HP | yes | 1.91239 | 0.0146535 / 0.0760505 / 0.514653 | 1.86904e-15 / 1.86865e-15 | finite |
| 192000 | SVF: BP | yes | 0.940991 | 0.01463 / 0.0758663 / 0.480143 | 1.86944e-15 / 1.86944e-15 | finite |
| 192000 | SVF: BR | yes | 1.27193 | 0.0058523 / 0.0303867 / 0.194667 | 7.40794e-15 / 7.40794e-15 | finite |
| 192000 | Sallen-Key: LP | yes | 0.634968 | 0.215274 / 0.214801 / 0.141508 | 1.84519e-22 / 1.84519e-22 | finite |
| 192000 | Sallen-Key: HP | yes | 0.683301 | 0.119614 / 0.119321 / 0.0786156 | 1.18078e-20 / 1.18078e-20 | finite |
| 192000 | Transistor ladder: LP | yes | 2.17484 | 0.246007 / 0.262418 / 0.322494 | 3.14537e-18 / 3.14474e-18 | finite |
| 192000 | Diode ladder: LP | yes | 1.28301 | 0.00595651 / 0.0304471 / 0.139988 | 8.53363e-18 / 8.53363e-18 | finite |

### Self-oscillation, buildup and recovery

At 48 kHz, the coarse grid brackets sustained oscillation onset rather than
estimating an exact critical resonance:

| Engine/response | Observation at drive 1 |
| --- | --- |
| SVF LP/HP/BP | .9 decays; 1.0 has long-lived, slowly decaying resonance; 1.05 and 1.1 sustain at all three cutoffs |
| SVF BR | Shared SVF engine; the response cancels much of the 1.0 oscillation, but sustains at 1.05/1.1 |
| SK LP/HP | Low/mid cutoff: .9 decays, 1.0 sustains; maximum cutoff: 1.05 decays to a tiny residual, 1.1 sustains |
| Transistor LP | Low/mid: 1.0 decays, 1.05 eventually sustains; maximum cutoff may decay rather than self-oscillate, depending on rate |
| Diode LP | .9 decays and 1.0 sustains at the three sampled cutoffs; low-cutoff onset builds slowly |

Transistor LP at maximum cutoff/resonance has no audible sustained oscillation
in the 44.1–96 kHz trials (48 kHz has a tiny finite ~6.55e-8 RMS cycle), but
does at 192 kHz. This existing sample-rate/model dependence is descriptive, not
a mandate to alter sound. Eight seconds was insufficient to classify transistor
LP at minimum cutoff/resonance 1.05: the extended measurements distinguish
saturating buildup from unbounded growth.

| Hz | Configuration | Resonance | RMS at 8 / 30 / 60 seconds | Last-window peak | Recovery final RMS |
| ---: | --- | ---: | --- | ---: | ---: |
| 44100 | Transistor ladder: LP | 1.05 | 0.00695488 / 0.089322 / 0.0909741 | 0.128937 | 3.14265e-18 |
| 48000 | Transistor ladder: LP | 1.05 | 0.00481751 / 0.0875773 / 0.0909349 | 0.128114 | 3.14277e-18 |
| 88200 | Transistor ladder: LP | 1.05 | 0.000635833 / 0.0328858 / 0.0871867 | 0.123787 | 3.14349e-18 |
| 96000 | Transistor ladder: LP | 1.05 | 0.000642091 / 0.0321864 / 0.0873072 | 0.123358 | 3.14445e-18 |
| 192000 | Transistor ladder: LP | 1.05 | 0.00138008 / 0.0507877 / 0.0852954 | 0.120928 | 3.14382e-18 |
| 48000 | Diode ladder: LP | 1 | 0.00491965 / 0.00538189 / 0.00538056 | 0.00758907 | 8.52257e-18 |

In the last ten seconds, transistor peak growth slows to at most about 0.16%
over those ten windows; 44.1/48 kHz peaks are essentially flat. RMS fluctuates
with non-integer cycle counts in each one-second window. The diode follow-up
has flat peaks. No accelerating or continuing material late-envelope growth
was observed. Finite-duration evidence is not an infinite-time stability proof.

Every measured oscillating configuration responded to reduced resonance.
All reached a full one-second recovery window at least 60 dB below the last
high-resonance RMS by the end of the second recovery second. This is a measured
relative marker, not an imposed DSP acceptance threshold or exact decay time.
The reduction itself follows the existing resonance smoother. Final tiny DC
residuals are consistent with existing `1e-18` nonlinear biases/float history;
exact digital zero is not promised.

| Configuration | Slowest measured -60 dB window end (s) | Least final attenuation (dB) | Final absolute RMS (max) |
| --- | ---: | ---: | ---: |
| SVF: LP | 2 | -234.747 | 9.27659e-15 |
| SVF: HP | 2 | -248.676 | 1.86865e-15 |
| SVF: BP | 2 | -248.676 | 1.86944e-15 |
| SVF: BR | 2 | -228.892 | 7.40794e-15 |
| Sallen-Key: LP | 2 | -419.389 | 1.84519e-22 |
| Sallen-Key: HP | 2 | -378.592 | 1.18078e-20 |
| Transistor ladder: LP | 2 | -207.678 | 3.14474e-18 |
| Diode ladder: LP | 2 | -294.780 | 8.84656e-18 |

### Measured smoothing and block behaviour

Full-range upward/downward steps were measured through the real processor,
one sample at a time, and compared exactly against mono/stereo processing with
64/1024/variable blocks. Trajectories matched at identical cumulative sample
positions. Prepare/reset/reprepare checks passed. The table spans both
directions and both smoothers; target errors shown are the full upward step.

| Hz | Measured 63% (ms) | Measured 99% (ms) | Upward cutoff error (semitones) | Upward resonance error |
| ---: | --- | --- | ---: | ---: |
| 44100 | 39.796–39.796 | 183.129–183.537 | 0.010147095 | 0.00019872189 |
| 48000 | 39.792–39.792 | 183.250–183.375 | 0.018432617 | 0.00013661385 |
| 88200 | 39.796–39.796 | 182.880–183.243 | 0.010864258 | 0.0002092123 |
| 96000 | 39.792–39.792 | 182.656–183.406 | 0.034606934 | 4.5537949e-05 |
| 192000 | 39.786–39.797 | 182.609–183.396 | 0.063751221 | 0.00027322769 |

All measured endpoints were unchanged between one and five seconds: float
rounding creates a small residual deadband, not a stalled whole ramp. At
192 kHz the largest full-range cutoff error is **0.0637512 semitone (6.375 cents)**;
the largest resonance error in the full-range ramp grid is **0.000273228**.
Legal one-/two-/ten-step changes also moved correctly but settled slightly
short; e.g. at 192 kHz, cutoff 69→70 settled at 69.9745026 and resonance
.7→.71 at .709699512. The unchanged-target controls stayed exactly at their
initial values. These are small precision observations, not instability or
block-/channel-dependent ramp duration. No smoothing correction was made.

**640 static block comparisons were exactly equal**, with one-sample processing
as reference: every rate/configuration/layout, block sizes 1/7/31/64/127/512/1024
and repeating `0,1,7,31,64,127,512,1024,0`. This compares the same unmodified
production DSP across partitions; it is not a new master-vs-feature render.
Source and normal-target configuration are unchanged from master, so there is
no production change for such a render to compare. Distinct stereo channels
matched independent mono references exactly at default and extreme settings.

Automation probes deliver targets requested at samples 111/577/1333/2001 at the
first block boundary at or after the request. With blocks of 64, delivery is
128/640/1344/2048; with 1024, 1024/1024/2048/2048. Multiple events at the same
boundary overwrite earlier targets before audio is processed. Corresponding
output differences from one-sample delivery were finite and expected. This
documents the harness's block delivery and ZED's lack of offset consumption;
it does not assert that every host uses exactly that quantisation strategy.
Static drive extremes and abrupt drive changes were finite. Existing topology
tests retain all 64 transitions, including accepted finite switching clicks.

### Denormals and diagnostic timing

`juce::ScopedNoDenormals` covers the processing callback, including filter and
smoother updates. No subnormal output samples or block-end subnormal history
values were observed across the supported datasets. Bit-pattern inspection is
used so flushing during a floating-point comparison cannot hide a subnormal.
All relevant child histories, inactive engines and both channels are inspected.
This does not instrument every intermediate operation or other CPU targets.

Release median callback cost in the ordinary oscillation grid was about
141 ns/frame for excitation, 135 for high-resonance silence and 120 for recovery.
These are diagnostic only: different block sizes, models, concurrent jobs and
OS scheduling prevent a controlled performance conclusion. No deterministic
timing assertion or denormal-performance claim is made.

## C. Informational out-of-spec observations

32/176.4/384 kHz were probed without rejecting or disabling those rates:
1,152 cases using the default, all-minimum and all-maximum profiles, eight
signals and both layouts. All 768 cases at 176.4/384 kHz were finite. At
32 kHz, **44 of 384 cases produced non-finite output/history**. Coefficient
snapshots remained finite; that alone does not ensure a stable recurrence.

| Hz | Configuration | Failed / total cases | First bad sample (range) | Largest finite value recorded |
| ---: | --- | ---: | --- | ---: |
| 32000 | Sallen-Key: LP | 14 / 48 | 100–110 | 6.68797e+37 |
| 32000 | Sallen-Key: HP | 14 / 48 | 99–110 | 7.27163e+37 |
| 32000 | Diode ladder: LP | 16 / 48 | 80–191 | 10 |

The failures were at pitch 135/resonance 1.1/drive 5. Pitch 135 converts to
~19.912 kHz, above the 16 kHz Nyquist frequency; SK/diode have no existing
Nyquist cap and their tangent/feedback domain changes. SK failed for all seven
non-silent inputs in both layouts; diode also failed after silence, consistent
with its non-zero internal bias exciting unstable history. Cases stop after
the first bad block; the first bad sample is counted from zero. These are
potentially host-disruptive results, not harmless full-scale exceedances.
They are **outside the supported specification and not automatically RC
blockers**. No rejection rule, clamp, limiter or other correction was added.

## D. Potential RC blockers

No supported-rate numerical RC blocker was established by this test grid:
no non-finite output, runaway envelope, failed resonance recovery or memory
error was observed. The highest-severity finding is the **out-of-spec 32 kHz
non-finite output**, for an explicit product decision rather than an automatic
RC failure. The float smoother precision issue is lower priority and has not
been demonstrated to prevent ordinary control changes or resonance recovery.

This is bounded empirical evidence; untested parameter trajectories, signal
levels and dwell times are not certified. Do not interpret a successful tool
exit code as a blanket stability pass: the tool deliberately records numerical
findings (including the 32 kHz failures) in CSV, while broken tool invariants
and existing regression assertions cause a non-zero exit.

## E. Non-blocking observations

- Sustained finite self-oscillation, finite peaks above 1, slow approach to a
  stable limit cycle, and tiny biased silence residuals are not failures.
- Cutoff/resonance float smoothing has a small history-dependent settling error;
  the exponential curve and rate-dependent time constant are otherwise intact.
- SK's one-sample resonance/coefficient ordering and drive's unsmoothed,
  separately polled block setters are existing behaviour, not changes here.
- Block-level event timing, drive stepping and minor immediate topology-switch
  clicks remain accepted limitations. No listening test was claimed.
- Existing deprecated JUCE `Font` constructor warnings remain in
  `ZedLookAndFeel.h` and `PluginEditor.cpp`; no new compiler warnings were
  introduced by the characterisation sources.

## F. Incomplete tests or limitations

### Validation and reproduction

Environment: native arm64 macOS 15.7.3 (24G419), Apple Clang 17.0.0
(`clang-1700.6.4.2`), CMake 4.4.3, Python 3.13.7, C++17, JUCE 8.0.6 at the
unchanged pinned commit `51a8a6d7aeae7326956d747737ccf1575e61e209`.

Release and Debug/AddressSanitizer builds succeeded. All six full measurement
modes completed, including the 60-second follow-ups and five-second settling
probes. All numeric CSV fields match between Release and ASan, excluding
diagnostic timing; generated summaries are identical. Supported output,
sampled coefficients and histories were finite in both. The same 44 out-of-spec
32 kHz cases failed numerically in both, without sanitizer reports.

Final CTest: **2/2 passed** in Release (5.94 s) and Debug/ASan (23.91 s).
`ZEDChannelLayouts` aggregates the unchanged layout, parameter, editor/state,
lifecycle/sample-rate and topology suites, including all 36 bus-layout
combinations, all eight state round-trips and all 64 topology transitions.
`ZEDStabilityToolSmoke` checks measurement arithmetic, NaN/Inf/subnormal
detection and nominal processing. Full characterisation requires the explicit
modes below in addition to CTest; it is not silently claimed by the smoke test.
No assertion, crash or AddressSanitizer memory error was reported. Leak checking
was disabled. `git diff --check` passed; production source diff and tracked
build-file list are empty. New files were also checked for whitespace and
machine-specific paths. Generated artefacts are excluded from the commit.

Commands below begin at the repository root. Every generated file, including
temporary compiler files and logs, stays under the new stability directories.
The existing pinned JUCE source checkout is reused read-only; omitting that
`FETCHCONTENT_SOURCE_DIR_JUCE` argument allows CMake to fetch the same pin into
the new build instead. These are reproducible commands for the final sources;
the extended/settling modes were added after inspecting the initial results.

```sh
mkdir -p build/stability-characterisation-results/tmp
export TMPDIR="$PWD/build/stability-characterisation-results/tmp"

cmake -S . -B build/stability-characterisation-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/build/macos-arm64-release/_deps/juce-src" \
  > build/stability-characterisation-results/release-configure.log 2>&1
cmake --build build/stability-characterisation-release \
  --target ZEDStabilityCharacterisation ZEDChannelTests --parallel 4 \
  > build/stability-characterisation-results/release-build.log 2>&1
ctest --test-dir build/stability-characterisation-release --output-on-failure \
  > build/stability-characterisation-results/release-ctest.log 2>&1
for mode in matrix mechanics boundaries oscillation extended out-of-spec; do
  build/stability-characterisation-release/ZEDStabilityCharacterisation \
    --mode "$mode" --output build/stability-characterisation-results/release \
    > "build/stability-characterisation-results/release-$mode.log" 2>&1 || exit 1
done
python3 Tests/SummariseStability.py build/stability-characterisation-results/release

cmake -S . -B build/stability-characterisation-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/build/macos-arm64-release/_deps/juce-src" \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address \
  > build/stability-characterisation-results/asan-configure.log 2>&1
cmake --build build/stability-characterisation-asan \
  --target ZEDStabilityCharacterisation ZEDChannelTests --parallel 4 \
  > build/stability-characterisation-results/asan-build.log 2>&1
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/stability-characterisation-asan --output-on-failure \
  > build/stability-characterisation-results/asan-ctest.log 2>&1
for mode in matrix mechanics boundaries oscillation extended out-of-spec; do
  ASAN_OPTIONS=detect_leaks=0 \
    build/stability-characterisation-asan/ZEDStabilityCharacterisation \
    --mode "$mode" --output build/stability-characterisation-results/asan \
    > "build/stability-characterisation-results/asan-$mode.log" 2>&1 || exit 1
done
python3 Tests/SummariseStability.py build/stability-characterisation-results/asan

cmake -S . -B build/stability-characterisation-normal-config \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=OFF \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/build/macos-arm64-release/_deps/juce-src" \
  > build/stability-characterisation-results/normal-configure.log 2>&1
git diff --check
git diff --exit-code HEAD -- Source
git ls-files build
git status --short --branch
```

The normal OFF configuration contains neither test target nor sanitizer flags.
All CMake additions are inside the existing opt-in conditional; no flags/macros
are added to the normal plug-in. ASan flags are supplied only to its separate
build directory to instrument the production library as well as the harness.
No pluginval/VST3 packaging run was required or performed for this test-only
change. No installed plug-in or Ableton folder was modified.

Raw CSVs include peak, RMS, mean, first bad frame, sampled coefficient/history
finiteness, subnormal observations and timing. `SummariseStability.py` checks
collection completeness and writes `summary.md` alongside them. Nothing in
those generated directories is proposed for tracking. Full-mode results are
reported here; the optional `--quick` flag was not used for the final datasets.

Limitations: finite deterministic grids are not exhaustive numerical proofs;
no arbitrary extreme-float input fuzzing, intra-block host automation transport,
concurrent parameter race test or listening assessment was performed. Onset
estimates use a coarse resonance grid, three pitches and finite observation
windows; no exact bifurcation frequency/threshold is claimed. Smoother trajectory
measurement uses the shared SVF processor path, while every engine is exercised
by output tests and existing lifecycle regressions. Coefficient/state inspection
is at block ends, not every intermediate operation. ASan is not a data-race or
undefined-behaviour sanitizer, and leak detection is disabled. Timing is not an
isolated benchmark. The initial exploratory sub-interval smoother requests
were replaced by legal parameter increments: host snapping must not be
misreported as a DSP stall.

Manual follow-up remains optional product review: listen at the five supported
rates to high-resonance excitation/silence and recovery, boundary cutoff sweeps,
drive steps and ordinary topology switches, in genuine mono and stereo layouts.
No manual DAW tests were performed for this task. Do not use the known 32 kHz
failure as an unattended listening test.

## G. Possible fixes for later consideration — not implemented

1. If out-of-spec resilience is required for the RC, investigate a narrowly
   scoped below-Nyquist coefficient-domain policy for SK/diode. It would need
   an explicit product decision, supported-rate sound comparisons and continued
   acceptance of other sample rates; do not simply reject 32 kHz or add a
   limiter. No precise corrective equation is proposed without that follow-up.
2. If the measured small settling error is undesirable, evaluate higher-precision
   smoother history/arithmetic while preserving the current exponential pitch
   curve and 4 Hz constant. Expect possible output differences requiring review;
   do not silently substitute a linear ramp or change cutoff quantisation.
3. Separately consider a single block drive snapshot and SK coefficient-update
   ordering only if their existing behaviour becomes a product requirement.
   Neither is established here as a release blocker; neither was changed.

No correction, drive smoothing, limiting, oversampling, sample-accurate event
handling or topology crossfade was added. This milestone contains only the
characterisation tools, their optional CMake registration and this report.
