#include "PluginProcessor.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// This executable does NOT link LifecycleTests.cpp. Reuse the existing private
// friend declaration in a separate program, without changing production classes.
struct ZedLifecycleTestAccess
{
    static std::array<float, 2> smooth(const ZedAudioProcessor& p)
    { return { p.smootherCutoff.z1, p.smootherRes.z1 }; }
    static void values(const ZDOnePole& f, std::vector<float>& v, bool state)
    { if (state) v.push_back(f.z); else v.insert(v.end(), { f.ff, f.fb }); }
    static void values(const ZDOnePoleEx& f, std::vector<float>& v, bool state)
    {
        if (state) v.insert(v.end(), { f.z, f.feedback });
        else v.insert(v.end(), { f.ff, f.fb, f.gamma, f.delta, f.epsilon, f.a0 });
    }
    static void values(const ZDSVF& f, std::vector<float>& v, bool state)
    {
        if (state) v.insert(v.end(), { f.z1, f.z2, f.lp, f.hp, f.bp, f.br });
        else v.insert(v.end(), { f.wd, f.t, f.wa, f.g, f.r });
    }
    static void values(const ZDSK& f, std::vector<float>& v, bool state)
    {
        values(f.lpfOP1,v,state); values(f.lpfOP2,v,state); values(f.hpfOP,v,state);
        if (!state) v.insert(v.end(), { f.alpha0, f.cutoff, f.k, f.driveGain });
    }
    static void values(const ZDSKHPF& f, std::vector<float>& v, bool state)
    {
        values(f.hpfOP1,v,state); values(f.hpfOP2,v,state); values(f.lpfOP1,v,state);
        if (!state) v.insert(v.end(), { f.alpha0, f.cutoff, f.k, f.driveGain });
    }
    static void values(const ZDSKmm& f, std::vector<float>& v, bool state)
    { values(f.lpf,v,state); values(f.hpf,v,state); }
    static void values(const ZDML& f, std::vector<float>& v, bool state)
    {
        values(f.filter1,v,state); values(f.filter2,v,state); values(f.filter3,v,state); values(f.filter4,v,state);
        if (!state) v.insert(v.end(), { f.cutoff, f.k, f.driveGain });
    }
    static void values(const ZDDL& f, std::vector<float>& v, bool state)
    {
        values(f.lpf1,v,state); values(f.lpf2,v,state); values(f.lpf3,v,state); values(f.lpf4,v,state);
        if (!state) v.insert(v.end(), { f.gamma, f.sg1, f.sg2, f.sg3, f.sg4, f.cutoff, f.k, f.driveGain });
    }
    static std::vector<float> values(const ZedAudioProcessor& p, bool state)
    {
        std::vector<float> v;
        values(p.svfL,v,state); values(p.svfR,v,state);
        values(p.korgFilterL,v,state); values(p.korgFilterR,v,state);
        values(p.moogLadderL,v,state); values(p.moogLadderR,v,state);
        values(p.diodeLadderL,v,state); values(p.diodeLadderR,v,state);
        if (state) v.insert(v.end(), { p.smootherCutoff.z1, p.smootherRes.z1,
                                     p.dcblocker1.xm1, p.dcblocker1.ym1, p.dcblocker2.xm1, p.dcblocker2.ym1 });
        else v.insert(v.end(), { p.smootherCutoff.a0, p.smootherCutoff.b1, p.smootherRes.a0, p.smootherRes.b1 });
        return v;
    }
    static std::array<double, 5> cutoffs(const ZedAudioProcessor& p)
    { return { p.svfL.wd / (2.0 * M_PI), p.korgFilterL.lpf.cutoff, p.korgFilterL.hpf.cutoff,
               p.moogLadderL.cutoff, p.diodeLadderL.cutoff }; }
    static double denominator(const ZedAudioProcessor& p)
    {
        const auto& s=p.svfL;
        double minimum=std::abs(1.0+2.0*s.r*s.g+s.g*s.g);
        for (double d : { 1.0/p.korgFilterL.lpf.alpha0, 1.0/p.korgFilterL.hpf.alpha0,
                         1.0/p.diodeLadderL.lpf1.fb, 1.0/p.diodeLadderL.lpf2.fb,
                         1.0/p.diodeLadderL.lpf3.fb, 1.0/p.diodeLadderL.lpf4.fb,
                         1.0+p.diodeLadderL.k*p.diodeLadderL.gamma })
            minimum=std::min(minimum,std::abs(d));
        const double g=p.moogLadderL.filter1.ff/(1.0-p.moogLadderL.filter1.ff);
        return std::min(minimum,std::abs(1.0+p.moogLadderL.k*g*g*g));
    }
};

namespace
{
using Access=ZedLifecycleTestAccess;
const std::array<double,5> rates {44100,48000,88200,96000,192000};
const std::array<int,9> varying {0,1,7,31,64,127,512,1024,0};
void require(bool good,const char* message) { if (!good) throw std::runtime_error(message); }
bool subnormal(float value)
{
    std::uint32_t bits=0; std::memcpy(&bits,&value,sizeof bits);
    return (bits & 0x7f800000u)==0 && (bits & 0x007fffffu)!=0;
}
struct Stats
{
    std::uint64_t samples=0, subs=0;
    double peak=0, sum=0, squares=0;
    bool finite=true;
    void add(float x)
    {
        if (!std::isfinite(x)) { finite=false; return; }
        ++samples; subs+=subnormal(x); peak=std::max(peak,std::abs(static_cast<double>(x)));
        sum+=x; squares+=static_cast<double>(x)*x;
    }
    double rms() const { return samples ? std::sqrt(squares/samples) : 0; }
    double mean() const { return samples ? sum/samples : 0; }
};
struct Controls { float cutoff,resonance,drive; };
struct Rig
{
    ZedAudioProcessor processor;
    std::array<juce::RangedAudioParameter*,4> parameters {};
    double rate;
    int channels, configuration;
    juce::MidiBuffer midi;
    Rig(double sr,int config,int count,Controls c) : rate(sr),channels(count),configuration(config)
    {
        const char* ids[] {"cutoff","resonance","drive",zed::filterConfigurationID};
        for (int i=0;i<4;++i)
            for (auto* p : processor.getParameters())
                if (auto* r=dynamic_cast<juce::RangedAudioParameter*>(p))
                    if (r->paramID==ids[i]) parameters[i]=r;
        for (auto* p : parameters) require(p!=nullptr,"Missing parameter");
        set(0,c.cutoff); set(1,c.resonance); set(2,c.drive); set(3,static_cast<float>(config));
        auto layout=processor.getBusesLayout();
        layout.inputBuses.set(0,count==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        layout.outputBuses=layout.inputBuses;
        require(processor.setBusesLayout(layout),"Layout rejected");
        processor.setRateAndBufferSizeDetails(sr,1024); processor.prepareToPlay(sr,1024);
    }
    void set(int index,float value)
    { parameters[index]->setValueNotifyingHost(parameters[index]->convertTo0to1(value)); }
    void process(juce::AudioBuffer<float>& b) { processor.processBlock(b,midi); }
};
// Unscaled measurement signals: no output normalization, clipping or limiter.
float signal(int kind,std::uint64_t n,double sr,double duration,int channel)
{
    const double t=static_cast<double>(n)/sr;
    const double phase=channel*0.73;
    const auto hash=static_cast<std::uint32_t>(n+channel*7919)*1664525u+1013904223u;
    const auto noise=hash ^ ((hash >> 13)*2246822519u);
    switch (kind)
    {
        case 0: return 0;
        case 1: return n==0 ? (channel ? -0.4f:0.5f) : 0;
        case 2: return channel ? -0.2f:0.25f;
        case 3: return static_cast<float>(0.25*std::sin(2*M_PI*40*t+phase));
        case 4: return static_cast<float>(0.25*std::sin(2*M_PI*1000*t+phase));
        case 5: return static_cast<float>(0.25*std::sin(2*M_PI*(sr*0.45)*t+phase));
        case 6: return (static_cast<float>(noise >> 8)/16777215.0f-0.5f)*0.5f;
        default:
        {
            const double ratio=std::log(sr*0.45/20.0);
            return static_cast<float>(0.25*std::sin(2*M_PI*20*duration/ratio*(std::exp(ratio*t/duration)-1)+phase));
        }
    }
}
const char* signalName(int kind)
{ const char* names[] {"silence","impulse","dc","sine40","sine1000","sine_high","noise","sweep"}; return names[kind]; }
struct Measurement
{
    Stats output;
    bool coeffFinite=true,stateFinite=true;
    std::uint64_t stateSubs=0,frames=0;
    double minimumDenominator=std::numeric_limits<double>::infinity(), nanoseconds=0;
    long long firstBadFrame=-1;
};
Measurement measure(Rig& r,int kind,int frames,int fixed=0,int automation=0)
{
    Measurement m;
    int iteration=0,position=0;
    while (position<frames)
    {
        const int count=std::min(frames-position,fixed ? fixed : varying[iteration++ % varying.size()]);
        if (automation==1)
        {
            const float fraction=static_cast<float>(position)/frames;
            r.set(0,12+123*(fraction<0.5f ? 2*fraction : 2*(1-fraction)));
        }
        if (automation==2) r.set(2,(iteration%8<4)?0.5f:5.0f);
        juce::AudioBuffer<float> b(r.channels,count);
        for (int c=0;c<r.channels;++c)
            for (int s=0;s<count;++s) b.setSample(c,s,signal(kind,position+s,r.rate,frames/r.rate,c));
        const auto start=std::chrono::steady_clock::now(); r.process(b);
        m.nanoseconds+=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count();
        for (int s=0;s<count;++s)
            for (int c=0;c<r.channels;++c)
            {
                m.output.add(b.getSample(c,s));
                if (!m.output.finite && m.firstBadFrame<0) m.firstBadFrame=position+s;
            }
        for (float value:Access::values(r.processor,false)) m.coeffFinite &= std::isfinite(value);
        for (float value:Access::values(r.processor,true))
        { m.stateFinite &= std::isfinite(value); m.stateSubs+=subnormal(value); }
        m.minimumDenominator=std::min(m.minimumDenominator,Access::denominator(r.processor));
        position+=count; m.frames=position;
        if (!m.output.finite || !m.coeffFinite || !m.stateFinite) break;
    }
    return m;
}
void header(std::ofstream& f)
{
    f << "rate,configuration,channels,cutoff,resonance,drive,signal,phase,window,frames,finite,coeff_finite,state_finite,peak,rms,mean,subnormal_output,state_subnormal_observations,min_abs_denominator,ns_per_frame,first_bad_frame\n" << std::setprecision(12);
}
void row(std::ofstream& f,const Rig& r,Controls c,const char* sig,const char* phase,int window,const Measurement& m)
{
    f << r.rate << ',' << r.configuration << ',' << r.channels << ',' << c.cutoff << ',' << c.resonance << ',' << c.drive
      << ',' << sig << ',' << phase << ',' << window << ',' << m.frames << ',' << m.output.finite << ',' << m.coeffFinite
      << ',' << m.stateFinite << ',' << m.output.peak << ',' << m.output.rms() << ',' << m.output.mean() << ',' << m.output.subs
      << ',' << m.stateSubs << ',' << m.minimumDenominator << ',' << (m.frames?m.nanoseconds/m.frames:0)
      << ',' << m.firstBadFrame << '\n';
}
void matrix(const std::filesystem::path& root,bool quick,bool outside)
{
    std::ofstream f(root/(outside?"out_of_spec.csv":"matrix.csv")); header(f);
    const std::vector<double> selected=outside?std::vector<double>{32000,176400,384000}:std::vector<double>(rates.begin(),rates.end());
    const std::vector<Controls> profiles { {57,.7f,1},{12,.01f,.5f},{135,1.1f,5},{12,1.1f,1},
        {135,.01f,5},{76,.5f,2.5f},{57,.7f,5},{135,.7f,1},{12,.7f,1},{69,1.05f,5},{69,1,1} };
    for (double rate:selected)
    {
        for (int config=0;config<8;++config)
            for (int channels: {1,2})
                for (size_t profile=0;profile<(outside?3:profiles.size());++profile)
                    for (int kind=0;kind<8;++kind)
                    {
                        Rig r(rate,config,channels,profiles[profile]);
                        const auto m=measure(r,kind,quick?1024:std::max(4096,static_cast<int>(rate*.1)));
                        row(f,r,profiles[profile],signalName(kind),"static",0,m);
                    }
        std::cout << "Recorded matrix " << rate << " Hz\n" << std::flush;
    }
    require(f.good(),"Matrix write failed");
}
void oscillation(const std::filesystem::path& root,bool quick)
{
    std::ofstream f(root/"oscillation.csv"); header(f);
    for (double rate:rates)
    {
        for (int config=0;config<8;++config)
            for (float cutoff: {12.0f,69.0f,135.0f})
            {
                std::vector<float> resonances {1.05f,1.1f};
                if (rate==48000) resonances.insert(resonances.end(),{.7f,.8f,.9f,1.0f});
                for (float resonance:resonances)
                {
                    Controls c {cutoff,resonance,1}; Rig r(rate,config,1,c);
                    auto m=measure(r,6,static_cast<int>(rate*.02),512);
                    row(f,r,c,"noise","excite",0,m);
                    bool good=m.output.finite && m.stateFinite && m.coeffFinite;
                    const int highWindows=quick?2:8, recoverWindows=quick?2:6;
                    for (int w=0;w<highWindows && good;++w)
                    {
                        m=measure(r,0,static_cast<int>(rate),1024); row(f,r,c,"silence","high",w,m);
                        good=m.output.finite && m.stateFinite && m.coeffFinite;
                    }
                    r.set(1,.01f); c.resonance=.01f;
                    for (int w=0;w<recoverWindows && good;++w)
                    {
                        m=measure(r,0,static_cast<int>(rate),1024); row(f,r,c,"silence","recovery",w,m);
                        good=m.output.finite && m.stateFinite && m.coeffFinite;
                    }
                    f.flush();
                }
            }
        std::cout << "Recorded oscillation/recovery " << rate << " Hz\n" << std::flush;
    }
    require(f.good(),"Oscillation write failed");
}
void boundaries(const std::filesystem::path& root,bool quick)
{
    std::ofstream f(root/"boundaries.csv"); header(f);
    std::ofstream cutoffFile(root/"internal_cutoffs.csv");
    cutoffFile << "rate,pitch,svf_hz,sk_lp_hz,sk_hp_hz,transistor_hz,diode_hz\n" << std::setprecision(12);
    for (double rate:rates)
    {
        for (float pitch: {12.0f,135.0f})
        {
            Rig r(rate,0,1,{pitch,.7f,1}); cutoffFile << rate << ',' << pitch;
            for (double hz:Access::cutoffs(r.processor)) cutoffFile << ',' << hz;
            cutoffFile << '\n';
        }
        for (int config=0;config<8;++config)
            for (int channels: {1,2})
                for (int mode=0;mode<3;++mode)
                {
                    Controls c {12,1.1f,5}; Rig r(rate,config,channels,c);
                    if (mode==0) r.set(0,135); // Abrupt target, existing smoother handles it.
                    const auto m=measure(r,6,static_cast<int>(rate*(quick?.1:2.0)),0,mode==1?1:mode==2?2:0);
                    row(f,r,c,"noise",mode==0?"cutoff_step":mode==1?"cutoff_triangle":"drive_steps",0,m);
                }
        std::cout << "Recorded boundaries " << rate << " Hz\n" << std::flush;
    }
}
void settling(const std::filesystem::path& root);
void extended(const std::filesystem::path& root)
{
    std::ofstream f(root/"extended.csv"); header(f);
    const auto trial=[&](double rate,int config,int channels,Controls c,int seconds)
    {
        Rig r(rate,config,channels,c);
        auto m=measure(r,6,static_cast<int>(rate*.02),512);
        row(f,r,c,"noise","excite",0,m);
        for (int phase=0;phase<2;++phase)
        {
            if (phase==1) { r.set(1,.01f); c.resonance=.01f; }
            for (int w=0;w<(phase==0?seconds:10);++w)
            {
                if (!m.output.finite || !m.stateFinite || !m.coeffFinite) break;
                m=measure(r,0,static_cast<int>(rate),1024);
                row(f,r,c,"silence",phase==0?"high":"recovery",w,m);
            }
        }
        f.flush();
    };
    for (double rate:rates)
    {
        // Resolve slow low-frequency buildup before classifying it as growth.
        trial(rate,6,1,{12,1.05f,1},60);
        // High-drive stereo excitation/recovery supplements the longer mono grid.
        for (int config=0;config<8;++config)
            for (float cutoff: {12.0f,69.0f,135.0f})
                trial(rate,config,2,{cutoff,1.1f,5},12);
        std::cout << "Recorded extended dwell " << rate << " Hz\n" << std::flush;
    }
    trial(48000,7,1,{12,1,1},60);
    require(f.good(),"Extended write failed");
    settling(root);
}
void settling(const std::filesystem::path& root)
{
    std::ofstream settling(root/"settling.csv");
    settling << "rate,case,parameter,initial,target,value_at_one_second,value_at_five_seconds,unchanged_after_one_second\n" << std::setprecision(12);
    for (double rate:rates)
        for (int test=0;test<5;++test)
        {
            const Controls start=test==0?Controls{12,.01f,1}:Controls{69,.7f,1};
            // Use actual host-visible steps (pitch interval 1, resonance .01).
            // Sub-interval requests would be snapped by the parameter, not stalled ramps.
            const float pitchSteps[] {0,1,2,10,0};
            const float resonanceSteps[] {0,0.01f,0.02f,0.1f,0};
            const Controls end=test==0?Controls{135,1.1f,1}:Controls{69+pitchSteps[test],.7f+resonanceSteps[test],1};
            Rig r(rate,0,1,start); r.set(0,end.cutoff); r.set(1,end.resonance);
            measure(r,0,static_cast<int>(rate),1024); const auto first=Access::smooth(r.processor);
            measure(r,0,static_cast<int>(4*rate),1024); const auto last=Access::smooth(r.processor);
            for (int parameter=0;parameter<2;++parameter)
                settling << rate << ',' << test << ',' << (parameter==0?"cutoff":"resonance") << ','
                         << (parameter==0?start.cutoff:start.resonance) << ',' << (parameter==0?end.cutoff:end.resonance)
                         << ',' << first[parameter] << ',' << last[parameter] << ',' << (first[parameter]==last[parameter]) << '\n';
        }
    require(settling.good(),"Settling write failed");
}
std::vector<float> render(Rig& r,int count,int fixed,bool automation=false,std::ofstream* events=nullptr)
{
    const int times[] {111,577,1333,2001};
    const Controls targets[] {{135,1.1f,5},{12,.01f,.5f},{120,1.05f,5},{57,.7f,1}};
    int position=0,iteration=0,next=0;
    std::vector<float> result;
    while (position<count)
    {
        int length=std::min(count-position,fixed?fixed:varying[iteration++%varying.size()]);
        if (automation)
            while (next<4 && times[next]<=position)
            {
                r.set(0,targets[next].cutoff); r.set(1,targets[next].resonance); r.set(2,targets[next].drive);
                if (events) *events << r.rate << ',' << r.configuration << ',' << fixed << ',' << times[next] << ',' << position << '\n';
                ++next;
            }
        juce::AudioBuffer<float> b(r.channels,length);
        for (int c=0;c<r.channels;++c)
            for (int s=0;s<length;++s) b.setSample(c,s,signal(6,position+s,r.rate,1,c));
        r.process(b);
        for (int s=0;s<length;++s)
            for (int c=0;c<r.channels;++c) result.push_back(b.getSample(c,s));
        position+=length;
    }
    return result;
}
void mechanics(const std::filesystem::path& root,bool quick)
{
    std::ofstream blocks(root/"blocks.csv"), events(root/"automation_events.csv");
    blocks << "rate,configuration,channels,block,static_identical,automation_finite,automation_max_difference\n" << std::setprecision(12);
    events << "rate,configuration,block,requested_sample,applied_sample\n";
    for (double rate:rates)
        for (int config=0;config<8;++config)
            for (int channels: {1,2})
            {
                Rig reference(rate,config,channels,{57,.7f,1}); const auto ref=render(reference,4096,1);
                Rig precise(rate,config,channels,{57,.7f,1}); const auto automated=render(precise,4096,1,true);
                for (int size: {0,1,7,31,64,127,512,1024})
                {
                    Rig r(rate,config,channels,{57,.7f,1}); const auto output=render(r,4096,size);
                    require(output==ref,"Static output depends on block size");
                    Rig a(rate,config,channels,{57,.7f,1}); const auto ao=render(a,4096,size,true,channels==1?&events:nullptr);
                    bool finite=true; double error=0;
                    for (size_t i=0;i<ao.size();++i)
                    { finite &= std::isfinite(ao[i]); error=std::max(error,std::abs(static_cast<double>(ao[i])-automated[i])); }
                    blocks << rate << ',' << config << ',' << channels << ',' << size << ",1," << finite << ',' << error << '\n';
                }
                // Distinct left/right deterministic noise against separate mono references.
                if (channels==2)
                    for (Controls c: {Controls{12,1.1f,5},Controls{135,1.1f,5},Controls{57,.7f,1}})
                    {
                        Rig stereo(rate,config,2,c), left(rate,config,1,c), right(rate,config,1,c);
                        for (int size: varying)
                        {
                            juce::AudioBuffer<float> b(2,size),l(1,size),r(1,size);
                            for (int s=0;s<size;++s)
                            {
                                b.setSample(0,s,signal(6,s,rate,1,0)); b.setSample(1,s,signal(6,s,rate,1,1));
                                l.setSample(0,s,b.getSample(0,s)); r.setSample(0,s,b.getSample(1,s));
                            }
                            stereo.process(b); left.process(l); right.process(r);
                            for (int s=0;s<size;++s)
                                require(b.getSample(0,s)==l.getSample(0,s) && b.getSample(1,s)==r.getSample(0,s),"Channel independence failure");
                        }
                    }
            }
    std::cout << "PASS: block invariance and channel independence; recorded quantized automation\n" << std::flush;

    std::ofstream f(root/"smoothing.csv");
    f << "rate,direction,parameter,t63_ms,t90_ms,t99_ms,final_value,target,absolute_error,mono_stereo_and_block_identical\n" << std::setprecision(12);
    for (double rate:rates)
        for (int direction=0;direction<2;++direction)
        {
            const Controls start=direction==0?Controls{12,.01f,1}:Controls{135,1.1f,1};
            const Controls end=direction==0?Controls{135,1.1f,1}:Controls{12,.01f,1};
            const std::array<float,2> initial {start.cutoff,start.resonance}, target {end.cutoff,end.resonance};
            Rig p(rate,0,1,start); require(Access::smooth(p.processor)==initial,"Smoother initialization mismatch");
            p.set(0,end.cutoff); p.set(1,end.resonance);
            const int length=static_cast<int>(rate*(quick?.3:1.0));
            std::vector<std::array<float,2>> trajectory;
            std::array<std::array<int,3>,2> reached {{{-1,-1,-1},{-1,-1,-1}}};
            const double fractions[] {1-std::exp(-1.0),.9,.99};
            juce::AudioBuffer<float> one(1,1);
            for (int i=0;i<length;++i)
            {
                one.clear(); p.process(one); const auto v=Access::smooth(p.processor); trajectory.push_back(v);
                for (int parameter=0;parameter<2;++parameter)
                    for (int j=0;j<3;++j)
                        if (reached[parameter][j]<0 && (v[parameter]-initial[parameter])/(target[parameter]-initial[parameter])>=fractions[j])
                            reached[parameter][j]=i+1;
            }
            for (int channels: {1,2})
                for (int size: {0,64,1024})
                {
                    Rig r(rate,0,channels,start); r.set(0,end.cutoff); r.set(1,end.resonance);
                    int position=0,iteration=0;
                    while (position<length)
                    {
                        const int n=std::min(length-position,size?size:varying[iteration++%varying.size()]);
                        juce::AudioBuffer<float> b(channels,n); b.clear(); r.process(b); position+=n;
                        require(Access::smooth(r.processor)==(position?trajectory[position-1]:initial),"Smoother depends on channel/block count");
                    }
                    r.processor.reset(); require(Access::smooth(r.processor)==target,"Reset did not synchronize smoother");
                    r.processor.prepareToPlay(rate==44100?96000:44100,1024);
                    require(Access::smooth(r.processor)==target,"Reprepare did not synchronize smoother");
                }
            for (int parameter=0;parameter<2;++parameter)
            {
                f << rate << ',' << direction << ',' << (parameter==0?"cutoff":"resonance");
                for (int sample:reached[parameter]) f << ',' << (sample<0?-1:sample*1000.0/rate);
                f << ',' << trajectory.back()[parameter] << ',' << target[parameter] << ','
                  << std::abs(trajectory.back()[parameter]-target[parameter]) << ",1\n";
            }
        }
    std::cout << "PASS: smoother initialization/reset/block/channel consistency; recorded ramps\n" << std::flush;
}
void smoke()
{
    Stats s; s.add(1); s.add(-1); s.add(0);
    require(s.peak==1 && s.mean()==0 && std::abs(s.rms()-std::sqrt(2.0/3))<1e-12,"Metric implementation failure");
    for (float x: {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
    { Stats bad; bad.add(x); require(!bad.finite,"Non-finite detector failure"); }
    require(subnormal(std::numeric_limits<float>::denorm_min()) && !subnormal(0) && !subnormal(1),"Subnormal detector failure");
    Rig r(48000,0,1,{57,.7f,1}); auto m=measure(r,6,1024);
    require(m.output.finite && m.coeffFinite && m.stateFinite && m.output.rms()>0,"Nominal smoke failure");
    std::cout << "PASS: measurement arithmetic, NaN/Inf/subnormal detection, nominal processing\n";
}
}
int main(int argc,char** argv)
{
    try
    {
        std::string mode="smoke"; std::filesystem::path output; bool quick=false;
        for (int i=1;i<argc;++i)
        {
            const std::string arg=argv[i];
            if (arg=="--quick") quick=true;
            else if (arg=="--mode" && i+1<argc) mode=argv[++i];
            else if (arg=="--output" && i+1<argc) output=argv[++i];
            else throw std::runtime_error("Usage: --mode smoke|matrix|oscillation|extended|settling|boundaries|mechanics|out-of-spec --output directory [--quick]");
        }
        require(!output.empty(),"Output directory required");
        // All reproducible measurements belong to the project's ignored build tree.
        auto canonical=std::filesystem::weakly_canonical(output).generic_string();
        require(canonical.find("/ZED/build/")!=std::string::npos,"Output must be beneath ZED/build/");
        std::filesystem::create_directories(output);
        juce::ScopedJuceInitialiser_GUI init;
        smoke();
        if (mode=="matrix") matrix(output,quick,false);
        else if (mode=="oscillation") oscillation(output,quick);
        else if (mode=="boundaries") boundaries(output,quick);
        else if (mode=="mechanics") mechanics(output,quick);
        else if (mode=="extended") extended(output);
        else if (mode=="settling") settling(output);
        else if (mode=="out-of-spec") matrix(output,quick,true);
        else require(mode=="smoke","Unknown mode");
        // Findings (including non-finite DSP) are CSV data, not suppressed failures.
        // Exit failure is reserved for broken tool invariants or regression checks.
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
