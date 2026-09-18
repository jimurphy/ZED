#include "PluginProcessor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

// Narrow private friendship: no public introspection API or differing class
// definitions/test macros in the plug-in. Detects even stored child rates whose
// coefficients are subsequently overwritten by the parent (Sallen-Key).
struct ZedLifecycleTestAccess
{
    static void rate(const ZDOnePole& f, double sr) { check(f.sr == sr, "Pole rate mismatch"); }
    static void rate(const ZDOnePoleEx& f, double sr) { check(f.sr == sr, "Extended pole rate mismatch"); }
    static void rate(const ZDSVF& f, double sr) { check(f.sr == sr, "SVF rate mismatch"); }
    static void rate(const ZDSK& f, double sr)
    {
        check(f.sr == sr, "SK LP rate mismatch");
        rate(f.lpfOP1, sr); rate(f.lpfOP2, sr); rate(f.hpfOP, sr);
    }
    static void rate(const ZDSKHPF& f, double sr)
    {
        check(f.sr == sr, "SK HP rate mismatch");
        rate(f.hpfOP1, sr); rate(f.hpfOP2, sr); rate(f.lpfOP1, sr);
    }
    static void rate(const ZDSKmm& f, double sr)
    {
        check(f.sr == sr, "SK wrapper rate mismatch"); rate(f.lpf, sr); rate(f.hpf, sr);
    }
    static void rate(const ZDML& f, double sr)
    {
        check(f.sr == sr, "Transistor ladder rate mismatch");
        rate(f.filter1, sr); rate(f.filter2, sr); rate(f.filter3, sr); rate(f.filter4, sr);
    }
    static void rate(const ZDDL& f, double sr)
    {
        check(f.sr == sr, "Diode ladder rate mismatch");
        rate(f.lpf1, sr); rate(f.lpf2, sr); rate(f.lpf3, sr); rate(f.lpf4, sr);
    }
    static void rate(const ZedAudioProcessor& p, double sr)
    {
        rate(p.svfL, sr); rate(p.svfR, sr);
        rate(p.korgFilterL, sr); rate(p.korgFilterR, sr);
        rate(p.moogLadderL, sr); rate(p.moogLadderR, sr);
        rate(p.diodeLadderL, sr); rate(p.diodeLadderR, sr);
        const float x = std::exp(-2.0f * M_PI * 4.0f / sr);
        check(p.smootherCutoff.a0 == 1.0f - x && p.smootherCutoff.b1 == -x,
              "Cutoff smoother rate mismatch");
        check(p.smootherRes.a0 == 1.0f - x && p.smootherRes.b1 == -x,
              "Resonance smoother rate mismatch");
    }
    static void cleared(const ZDOnePole& f) { check(f.z == 0.0f, "Pole history retained"); }
    static void cleared(const ZDOnePoleEx& f)
    { check(f.z == 0.0f && f.feedback == 0.0f, "Extended pole history retained"); }
    static void cleared(const ZDSVF& f)
    {
        check(f.z1 == 0 && f.z2 == 0 && f.hp == 0 && f.bp == 0 && f.lp == 0 && f.br == 0,
              "SVF history retained");
    }
    static void cleared(const ZDSK& f) { cleared(f.lpfOP1); cleared(f.lpfOP2); cleared(f.hpfOP); }
    static void cleared(const ZDSKHPF& f) { cleared(f.hpfOP1); cleared(f.hpfOP2); cleared(f.lpfOP1); }
    static void cleared(const ZDSKmm& f) { cleared(f.lpf); cleared(f.hpf); }
    static void cleared(const ZDML& f)
    { cleared(f.filter1); cleared(f.filter2); cleared(f.filter3); cleared(f.filter4); }
    static void cleared(const ZDDL& f)
    { cleared(f.lpf1); cleared(f.lpf2); cleared(f.lpf3); cleared(f.lpf4); }
    static void cleared(const ZedAudioProcessor& p)
    {
        cleared(p.svfL); cleared(p.svfR); cleared(p.korgFilterL); cleared(p.korgFilterR);
        cleared(p.moogLadderL); cleared(p.moogLadderR); cleared(p.diodeLadderL); cleared(p.diodeLadderR);
        check(p.dcblocker1.xm1 == 0 && p.dcblocker1.ym1 == 0
              && p.dcblocker2.xm1 == 0 && p.dcblocker2.ym1 == 0, "DC blocker history retained");
        check(p.smootherCutoff.z1 == p.cutoffParameter->load()
              && p.smootherRes.z1 == p.resParameter->load(), "Smoothers not synchronized");
    }
    static void exciteDC(ZedAudioProcessor& p) { p.dcblocker1.dsp(1); p.dcblocker2.dsp(-1); }
};

namespace
{
juce::RangedAudioParameter& parameter(ZedAudioProcessor& p, const char* id)
{
    for (auto* item : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(item))
            if (ranged->paramID == id) return *ranged;
    throw std::runtime_error("Missing parameter");
}
void set(ZedAudioProcessor& p, const char* id, float value)
{
    auto& item = parameter(p, id);
    item.setValueNotifyingHost(item.convertTo0to1(value));
}
void configure(ZedAudioProcessor& p, int configuration, int channels)
{
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses = layout.inputBuses;
    check(p.setBusesLayout(layout), "Lifecycle layout rejected");
    set(p, zed::filterConfigurationID, static_cast<float>(configuration));
    set(p, "cutoff", 76.0f); set(p, "resonance", 0.55f); set(p, "drive", 1.3f);
}
struct ParameterWatch : juce::AudioProcessorParameter::Listener
{
    ZedAudioProcessor& processor;
    std::vector<float> values;
    int notifications = 0;
    explicit ParameterWatch(ZedAudioProcessor& p) : processor(p)
    {
        for (auto* item : p.getParameters()) { values.push_back(item->getValue()); item->addListener(this); }
    }
    ~ParameterWatch() override { for (auto* item : processor.getParameters()) item->removeListener(this); }
    void parameterValueChanged(int, float) override { ++notifications; }
    void parameterGestureChanged(int, bool) override { ++notifications; }
    void verify() const
    {
        check(notifications == 0, "Lifecycle sent parameter notification");
        int i = 0;
        for (auto* item : processor.getParameters()) check(item->getValue() == values[i++], "Lifecycle mutated parameter");
    }
};
void prepare(ZedAudioProcessor& p, double sr)
{
    ParameterWatch watch(p);
    // Deliberately stale processor metadata: the callback argument is authoritative.
    p.setRateAndBufferSizeDetails(32000.0, 512);
    p.prepareToPlay(sr, 512);
    ZedLifecycleTestAccess::rate(p, sr);
    ZedLifecycleTestAccess::cleared(p);
    p.setRateAndBufferSizeDetails(sr, 512);
    watch.verify();
}
void reset(ZedAudioProcessor& p)
{
    ParameterWatch watch(p); p.reset(); watch.verify(); ZedLifecycleTestAccess::cleared(p);
}
void release(ZedAudioProcessor& p)
{
    ParameterWatch watch(p); p.releaseResources(); watch.verify(); ZedLifecycleTestAccess::cleared(p);
}
std::vector<float> render(ZedAudioProcessor& p, int channels, int active = -2)
{
    std::vector<float> result;
    juce::MidiBuffer midi;
    int position = 0;
    ParameterWatch watch(p);
    for (int repeat = 0; repeat < 3; ++repeat)
        for (int count : { 0, 1, 7, 64, 257, 512, 3, 0 })
        {
            juce::AudioBuffer<float> buffer(channels, count);
            buffer.clear();
            for (int c = 0; c < channels; ++c)
                if (active == -2 || active == c)
                    for (int s = 0; s < count; ++s)
                        buffer.setSample(c, s, 0.19f * std::sin(0.037f * (position + s))
                            + (position + s == 0 ? 0.4f : 0.0f));
            p.processBlock(buffer, midi);
            for (int s = 0; s < count; ++s)
                for (int c = 0; c < channels; ++c)
                {
                    const auto value = buffer.getSample(c, s);
                    check(std::isfinite(value), "Lifecycle non-finite output"); result.push_back(value);
                }
            position += count;
        }
    watch.verify();
    return result;
}
void equal(const std::vector<float>& a, const std::vector<float>& b)
{ check(a == b, "Reset/reprepare output differs from fresh processor"); }
void nonSilent(const std::vector<float>& values)
{
    double energy = 0;
    for (float value : values) energy += static_cast<double>(value) * value;
    check(energy > 1.0e-8, "Unexpectedly silent lifecycle output");
}
void testRate(double sr)
{
    for (int configuration = 0; configuration < 8; ++configuration)
    {
        ZedAudioProcessor mono, stereoLeft, stereoRight, silent;
        configure(mono, configuration, 1); configure(stereoLeft, configuration, 2);
        configure(stereoRight, configuration, 2); configure(silent, configuration, 1);
        for (auto* p : { &mono, &stereoLeft, &stereoRight, &silent }) prepare(*p, sr);
        const auto m = render(mono, 1), l = render(stereoLeft, 2, 0), r = render(stereoRight, 2, 1);
        const auto s = render(silent, 1, -1);
        nonSilent(m); nonSilent(l); nonSilent(r);
        for (size_t i = 0; i < m.size(); ++i)
            check(m[i] == l[2*i] && m[i] == r[2*i+1] && s[i] == l[2*i+1] && s[i] == r[2*i],
                  "Lifecycle channel independence failed");
        for (int channels : { 1, 2 })
        {
            ZedAudioProcessor used, fresh, quiet;
            configure(used, configuration, channels); configure(fresh, configuration, channels);
            configure(quiet, configuration, channels);
            prepare(used, sr); prepare(fresh, sr); prepare(quiet, sr);
            // Excite all topologies in both channels so reset cannot accidentally
            // clear only the currently selected filter or just one SK branch.
            for (int index = 0; index < 8; ++index)
            { set(used, zed::filterConfigurationID, static_cast<float>(index)); render(used, channels); }
            set(used, zed::filterConfigurationID, static_cast<float>(configuration));
            // Pending parameter changes must be reflected without a processing call.
            set(used, "cutoff", 83); set(used, "resonance", 0.42f);
            set(fresh, "cutoff", 83); set(fresh, "resonance", 0.42f);
            set(quiet, "cutoff", 83); set(quiet, "resonance", 0.42f);
            prepare(fresh, sr); prepare(quiet, sr);
            ZedLifecycleTestAccess::exciteDC(used);
            reset(used);
            equal(render(used, channels), render(fresh, channels));
            reset(used);
            const auto silence = render(used, channels, -1);
            equal(silence, render(quiet, channels, -1));
            // Existing tanh(+1e-18) bias yields tiny fresh-state residuals.
            for (float value : silence) check(std::abs(value) < 1.0e-12f, "Stale output after reset");
            release(used); prepare(used, sr); prepare(fresh, sr);
            equal(render(used, channels), render(fresh, channels));
            juce::MemoryBlock state;
            used.getStateInformation(state);
            ZedAudioProcessor restored;
            configure(restored, (configuration + 1) % 8, channels);
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            for (int i = 0; i < used.getParameters().size(); ++i)
                check(used.getParameters()[i]->getValue() == restored.getParameters()[i]->getValue(),
                      "Lifecycle state restoration changed parameter");
            prepare(restored, sr); prepare(used, sr);
            equal(render(restored, channels), render(used, channels));
        }
    }
    std::cout << "PASS: lifecycle rate " << sr
              << ", all 8 choices, mono/stereo, nested rates, reset/silence/release/state\n";
}
void testTransitions()
{
    for (int channels : { 1, 2 })
        for (int configuration = 0; configuration < 8; ++configuration)
        {
            ZedAudioProcessor used;
            configure(used, configuration, channels);
            for (double sr : { 44100.0, 96000.0, 48000.0, 96000.0, 44100.0, 192000.0, 192000.0 })
            {
                ZedAudioProcessor fresh;
                configure(fresh, configuration, channels);
                prepare(used, sr); prepare(fresh, sr);
                equal(render(used, channels), render(fresh, channels));
            }
        }
    std::cout << "PASS: repeated/rate-changing preparation equals fresh output exactly\n";
}
}

void runLifecycleTests()
{
    ZedAudioProcessor unprepared;
    configure(unprepared, 5, 2);
    reset(unprepared);
    release(unprepared);
    reset(unprepared);
    std::cout << "PASS: reset/release before preparation\n";
    // Fractional rate detects accidental narrowing to int or float in any child.
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, 48000.123456789 }) testRate(sr);
    testTransitions();
}
