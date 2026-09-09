#include "Visuals.hpp"
#include "Plugin.hpp"
#include "Engine.hpp"
#include "SoundMath.hpp"
#include <algorithm>
#include <cmath>
#include <complex>

namespace {
constexpr double pi=3.14159265358979323846;
const juce::Colour panel{0xff1b2031}, background{0xff111624}, line{0xff414b69}, text{0xfff1f3fc}, muted{0xffb5bdd6}, amber{0xffefc274}, violet{0xffb8a0ff};
juce::Font font(float size,bool bold=false) { return juce::Font(juce::FontOptions(size,bold ? juce::Font::bold : juce::Font::plain)); }
float unit(double x) { return static_cast<float>(std::clamp(x,0.0,1.0)); }
void fft(std::array<std::complex<double>,16384>& a) {
    constexpr std::size_t n=16384;
    for(std::size_t i=1,j=0;i<n;++i) {
        auto bit=n>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit;
        if(i<j) std::swap(a[i],a[j]);
    }
    for(std::size_t len=2;len<=n;len<<=1) {
        const auto step=std::polar(1.0,-2*pi/static_cast<double>(len));
        for(std::size_t i=0;i<n;i+=len) {
            std::complex<double> w{1,0};
            for(std::size_t j=0;j<len/2;++j) {
                const auto u=a[i+j], v=a[i+j+len/2]*w;
                a[i+j]=u+v; a[i+j+len/2]=u-v; w*=step;
            }
        }
    }
}
}
GeigerVisuals::GeigerVisuals(GeigerPlugin& plugin):plugin_(plugin) {
    setTitle("Parameter explorer"); setDescription("Explanatory models and offline audio previews. Live detector activity is shown separately.");
    addAndMakeVisible(probe_); probe_.onClick=[this]{focus(geiger::Rate); refresh();};
    probe_.setTooltip("Return to the rate / listening-distance playground.");
    refresh();
}
GeigerVisuals::~GeigerVisuals() {endGesture();}
void GeigerVisuals::setPage(int page) {
    endGesture(); page_=juce::jlimit(0,3,page);
    constexpr geiger::Param initial[]{geiger::Rate,geiger::PulseWidth,geiger::Speaker,geiger::Attack};
    focused_=initial[page_]; dirty_=true; probe_.setVisible(page_==0); refresh();
}
void GeigerVisuals::focus(std::size_t index) {
    if(index>=geiger::Count || focused_==index || dragging_) return;
    focused_=index; dirty_=true;
    setMouseCursor(kind()==Kind::Probe ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
}
GeigerVisuals::Kind GeigerVisuals::kind() const noexcept {
    using namespace geiger;
    switch(focused_) {
        case Rate: return Kind::Probe;
        case Randomness: return Kind::Waiting;
        case DeadTime: return Kind::Throughput;
        case Recovery: return Kind::Recovery;
        case Afterpulse: return Kind::Afterpulse;
        case Background: case Clusters: case Wander: case WanderSpeed: case MotionDepth: case MotionRate: return Kind::Field;
        case Drive: return Kind::Drive;
        case Compensation: return Kind::Density;
        case Width: return Kind::Stereo;
        case Attack: case Release: case RunMode: case Power: return Kind::Envelope;
        case MidiPitch: return Kind::Pitch;
        case Retrigger: case Seed: return Kind::Repeat;
        case Speaker: case Resonance: case Tone: case Hiss: case Hum: case HumFrequency: case Distance: return Kind::Spectrum;
        default: return Kind::Wave;
    }
}
juce::Rectangle<float> GeigerVisuals::plot() const {
    return getLocalBounds().toFloat().withTrimmedTop(47).withTrimmedBottom(48).reduced(16,0);
}
void GeigerVisuals::resized() {probe_.setBounds(getWidth()-105,9,92,26);}
void GeigerVisuals::refresh() {
    const auto next=geiger::sanitize(plugin_.currentValues());
    if(!dirty_ && next==values_) {repaint(); return;}
    values_=next; reference_=values_; reference_[focused_]=geiger::definitions[focused_].initial;
    const auto k=kind();
    duration_=8;
    if(k==Kind::Wave) duration_=std::clamp(std::max(values_[geiger::Decay],reference_[geiger::Decay])*0.004+std::max(values_[geiger::PulseWidth],reference_[geiger::PulseWidth])*0.008,0.016,0.30);
    if(k==Kind::Envelope) duration_=std::max(0.8,0.08+std::max(values_[geiger::Attack],reference_[geiger::Attack])*0.004+0.25+std::max(values_[geiger::Release],reference_[geiger::Release])*0.004);
    if(k==Kind::Recovery) duration_=std::max(0.003,values_[geiger::DeadTime]*1e-6+std::max(values_[geiger::Recovery],reference_[geiger::Recovery])*0.005);
    current_=generate(values_); default_=generate(reference_); dirty_=false; repaint();
}
GeigerVisuals::Data GeigerVisuals::generate(geiger::Values v) const {
    using namespace geiger;
    Data result;
    const auto k=kind();
    if(k==Kind::Wave || k==Kind::Spectrum) {
        // Same production engine, fixed 48 kHz and deterministic seed. The
        // preview powers its own isolated circuit, never the running plug-in.
        Engine engine; v[Rate]=v[Background]=0; v[Power]=1; v[Attack]=0;
        if(k==Kind::Wave) v[Hiss]=v[Hum]=-90;
        const bool showMaster=focused_==Output;
        if(!showMaster) v[Output]=0;
        engine.configure(v); engine.prepare(48000); engine.setGate(true); engine.testClick();
        std::array<std::complex<double>,16384> spectrum{};
        const int count=k==Kind::Spectrum ? 16384 : static_cast<int>(duration_*48000);
        for(int n=0;n<count;++n) {
            const auto f=engine.tick(); const double sample=0.5*(f.left+f.right);
            result.peak=std::max(result.peak,std::max(std::abs(static_cast<double>(f.left)),std::abs(static_cast<double>(f.right))));
            const auto bin=std::min<std::size_t>(255,static_cast<std::size_t>(n)*256/static_cast<std::size_t>(count));
            result.low[bin]=std::min(result.low[bin],static_cast<float>(sample)); result.high[bin]=std::max(result.high[bin],static_cast<float>(sample));
            if(k==Kind::Spectrum) {
                // Half-cosine end taper keeps the initial impulse intact and
                // limits truncation leakage in long ringing/noise previews.
                const double taper=n<count*3/4 ? 1.0 : 0.5*(1+std::cos(pi*(n-count*3/4)/static_cast<double>(count/4)));
                spectrum[static_cast<std::size_t>(n)]=sample*taper;
            }
        }
        if(k==Kind::Spectrum) {
            fft(spectrum);
            for(std::size_t i=0;i<256;++i) {
                const double hz=20*std::pow(1000.0,i/255.0), bin=hz*16384/48000;
                const auto b=static_cast<std::size_t>(bin); const double frac=bin-static_cast<double>(b);
                const double amplitude=(std::abs(spectrum[b])*(1-frac)+std::abs(spectrum[b+1])*frac);
                result.curve[i]=unit((20*std::log10(std::max(1e-8,amplitude))+60)/90);
            }
        }
        return result;
    }
    if(k==Kind::Field) {
        Engine engine; engine.configure(v); engine.prepare(1000); engine.setGate(true);
        for(int n=0;n<8000;++n) {
            engine.tick(); const auto bin=std::min<std::size_t>(255,static_cast<std::size_t>(n)*256/8000);
            const double rate=engine.incomingCps(); result.peak=std::max(result.peak,rate);
            result.curve[bin]=std::max(result.curve[bin],unit(std::log1p(rate)/std::log(120001.0)));
        }
        return result;
    }
    if(k==Kind::Waiting) {
        Random random; random.seed(static_cast<std::uint64_t>(v[Seed]));
        for(int i=0;i<8192;++i) {
            const double r=v[Randomness]*0.01, wait=(1-r)-r*std::log(random.uniform());
            const auto bin=static_cast<std::size_t>(std::clamp(wait/5*256,0.0,255.0));
            result.curve[bin]+=1.0f;
        }
        const float max=*std::max_element(result.curve.begin(),result.curve.end());
        for(auto& x:result.curve) x/=std::max(1.0f,max);
        return result;
    }
    if(k==Kind::Repeat) {
        // Two equal-length seeded timing previews. Repeat on Play resets the
        // timing stream; off continues it. This is not the live count meter.
        Engine engine; auto p=v; p[Rate]=std::clamp(v[Rate],2.0,80.0); p[Background]=0; p[Attack]=0;
        engine.configure(p); engine.prepare(1000); engine.setGate(true);
        for(int pass=0;pass<2;++pass) {
            if(pass==1 && v[Retrigger]>0.5) {engine.reset(); engine.setGate(true);}
            for(int n=0;n<2000;++n) {
                const auto before=engine.detected(); engine.tick();
                const auto bin=static_cast<std::size_t>(pass*128+n*128/2000);
                if(engine.detected()!=before) result.curve[bin]=1;
            }
        }
        return result;
    }
    for(std::size_t i=0;i<256;++i) {
        const double x=i/255.0;
        double y=0;
        switch(k) {
            case Kind::Throughput: { const double rate=x*12000; y=(rate/(1+rate*v[DeadTime]*1e-6))/12000; break; }
            case Kind::Recovery: {
                const double t=x*duration_-v[DeadTime]*1e-6;
                y=t<0 ? 0 : (v[Recovery]<=0 ? 1 : 0.25+0.75*(-std::expm1(-t/(v[Recovery]*0.001)))); break;
            }
            case Kind::Afterpulse: {
                const double ms=x*7; y=ms<0.25 ? 1-ms/0.25 : ((ms>v[DeadTime]*0.001+0.4 && ms<v[DeadTime]*0.001+4) ? v[Afterpulse]*0.01 : 0); break;
            }
            case Kind::Drive: {
                const double drive=std::pow(10.0,v[Drive]/20); y=0.5+0.5*softClip((2*x-1)*drive)/std::sqrt(drive); break;
            }
            case Kind::Density: y=densityGain(x*12000,v[Compensation]); break;
            case Kind::Stereo: y=std::abs(2*x-1)<=v[Width]*0.01+0.005 ? 0.8 : 0.05; break;
            case Kind::Pitch: y=0.5+(v[MidiPitch]>0.5 ? (2*x-1)*0.48 : 0); break;
            case Kind::Envelope: {
                const double time=x*duration_, hold=0.25+std::max(values_[Attack],reference_[Attack])*0.004;
                const double a=v[Attack]*0.001,r=v[Release]*0.001;
                if(focused_==RunMode && v[RunMode]<0.5) y=1;
                else if(time>0.08 && time<0.08+hold) y=a<=0 ? 1 : -std::expm1(-(time-0.08)/a);
                else if(time>=0.08+hold) y=(a<=0 ? 1 : -std::expm1(-hold/a))*std::exp(-(time-0.08-hold)/r);
                if(focused_==RunMode && v[RunMode]>1.5) y*=0.65;
                if(focused_==Power) y*=v[Power];
                break;
            }
            default: break;
        }
        result.curve[i]=unit(y);
    }
    return result;
}
void GeigerVisuals::paint(juce::Graphics& g) {
    using namespace geiger;
    g.setColour(panel); g.fillRoundedRectangle(getLocalBounds().toFloat(),14);
    g.setColour(line); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),14,1);
    const auto k=kind(); const auto r=plot();
    g.setColour(violet); g.setFont(font(13,true));
    g.drawText(k==Kind::Probe ? "PROBE PLAYGROUND" : juce::String(definitions[focused_].name).toUpperCase(),16,8,getWidth()-(page_==0 ? 124 : 32),22,juce::Justification::left);
    g.setColour(muted); g.setFont(font(11));
    g.drawText(k==Kind::Probe ? "Drag horizontally for rate, vertically for distance" : "PARAMETER PREVIEW  /  not live detector activity",16,29,getWidth()-32,16,juce::Justification::left);
    g.setColour(background); g.fillRoundedRectangle(r,6);
    g.setColour(line.withAlpha(0.55f));
    const int xDivisions=k==Kind::Spectrum ? 3 : 4;
    for(int i=1;i<xDivisions;++i) {
        const float x=r.getX()+r.getWidth()*static_cast<float>(i)/static_cast<float>(xDivisions);
        g.drawVerticalLine(static_cast<int>(x),r.getY(),r.getBottom());
    }
    for(int i=1;i<4;++i) {
        const float y=r.getY()+r.getHeight()*static_cast<float>(i)/4;
        g.drawHorizontalLine(static_cast<int>(y),r.getX(),r.getRight());
    }
    juce::String axis,caption;
    if(k==Kind::Probe) {
        const float x=unit(std::log1p(values_[Rate])/std::log(12001.0)), y=unit(values_[Distance]*0.01);
        const auto p=juce::Point<float>(r.getX()+x*r.getWidth(),r.getY()+y*r.getHeight());
        g.setColour(amber.withAlpha(0.16f)); g.fillEllipse(p.x-18,p.y-18,36,36);
        g.setColour(amber); g.drawEllipse(p.x-9,p.y-9,18,18,1.5f);
        g.setColour(text); g.fillEllipse(p.x-3,p.y-3,6,6);
        axis="QUIET                                        INTENSE";
        caption="Near at top / far at bottom. Hover a knob to explore.";
    } else if(k==Kind::Wave) {
        const double scale=0.46*r.getHeight()/std::max({0.15,current_.peak,default_.peak});
        const auto draw=[&](const Data& data,juce::Colour colour) {
            g.setColour(colour);
            for(std::size_t i=0;i<256;++i) {
                const float x=r.getX()+static_cast<float>(i)*r.getWidth()/256;
                g.drawLine(x,r.getCentreY()-static_cast<float>(data.high[i]*scale),x,r.getCentreY()-static_cast<float>(data.low[i]*scale),1.1f);
            }
        };
        draw(default_,muted.withAlpha(0.42f)); draw(current_,amber);
        axis="0 ms                      "+juce::String(duration_*1000,0)+" ms   /   peak "+juce::String(20*std::log10(std::max(1e-6,current_.peak)),1)+" dBFS";
        caption="Rendered pulse. Gray: this control at its default.";
    } else {
        const auto draw=[&](const Data& data,juce::Colour colour) {
            juce::Path path;
            for(std::size_t i=0;i<256;++i) {
                const float x=r.getX()+static_cast<float>(i)*r.getWidth()/255;
                const float y=r.getBottom()-3-data.curve[i]*(r.getHeight()-6);
                if(i==0) path.startNewSubPath(x,y); else path.lineTo(x,y);
            }
            g.setColour(colour); g.strokePath(path,juce::PathStrokeType(1.8f));
        };
        draw(default_,muted.withAlpha(0.4f)); draw(current_,amber);
        switch(k) {
            case Kind::Spectrum: axis="20 Hz           200             2k            20k"; caption="Rendered click + noise spectrum / fixed -60..+30 dB magnitude"; break;
            case Kind::Waiting: axis="0                 1                 3                 5x"; caption="Waiting time / mean interval. Gray: default timing."; break;
            case Kind::Throughput: axis="Incoming 0                          12,000 cps"; caption="Detected cps: steady Poisson / no afterpulses."; break;
            case Kind::Recovery: axis="0                       "+juce::String(duration_*1000,1)+" ms after a count"; caption="Electrical recovery: 0 in dead time, then 25..100%."; break;
            case Kind::Afterpulse: axis="Primary           secondary window             7 ms"; caption="Height shows secondary chance, not pulse energy."; break;
            case Kind::Field: axis="0 s                     8 s / peak "+juce::String(current_.peak,0)+" cps"; caption="Seeded field simulation / logarithmic count scale."; break;
            case Kind::Drive: axis="Input -1                         0                         +1"; caption="Static drive curve. Gray: drive at its default."; break;
            case Kind::Density: axis="Detected 0                         12,000 cps"; caption="Pre-drive gain (1 to 0). Live gain: "+juce::String(20*std::log10(std::max(1e-6f,plugin_.telemetry().balance.load())),1)+" dB"; break;
            case Kind::Stereo: axis="LEFT                       CENTER                       RIGHT"; caption="Individual click pan range. Zero is true mono."; break;
            case Kind::Envelope: axis="0                       "+juce::String(duration_,2)+" s / gate opens, closes"; caption="Field-density envelope, not the click's ring decay."; break;
            case Kind::Pitch: axis="C2                        C4                        C6"; caption="MIDI transposition: -24 to +24 st. C4 is neutral."; break;
            case Kind::Repeat: axis="PLAY 1  /  2 seconds            PLAY 2  / 2 seconds"; caption="Seeded example (2..80 cps). On repeats; off continues."; break;
            default: break;
        }
    }
    if(focused_==geiger::WanderSpeed && values_[geiger::Wander]==0) caption="Field wander is 0%. Raise it to hear speed changes.";
    if(focused_==geiger::MotionRate && values_[geiger::MotionDepth]==0) caption="Breathing is 0%. Raise it to hear speed changes.";
    if(focused_==geiger::HumFrequency && values_[geiger::Hum]<=-89.9) caption="Supply hum is off. Raise it to hear 50 / 60 Hz.";
    if(focused_==geiger::RunMode && values_[geiger::RunMode]>1.5) caption="Example MIDI note: density envelope at 65% velocity.";
    g.setColour(muted); g.setFont(font(11));
    if(k==Kind::Spectrum) {
        const juce::String labels[]{"20 Hz","200 Hz","2 kHz","20 kHz"};
        for(int i=0;i<4;++i) {
            const int x=static_cast<int>(r.getX()+r.getWidth()*static_cast<float>(i)/3);
            const int left=i==0 ? x : (i==3 ? x-60 : x-30);
            g.drawText(labels[i],left,getHeight()-44,60,18,i==0 ? juce::Justification::left : (i==3 ? juce::Justification::right : juce::Justification::centred));
        }
    } else g.drawFittedText(axis,14,getHeight()-44,getWidth()-28,18,juce::Justification::centred,1);
    g.setFont(font(11.5f));
    g.drawFittedText(caption,12,getHeight()-25,getWidth()-24,21,juce::Justification::centred,1);
}
void GeigerVisuals::endGesture() {
    if(!dragging_) return;
    plugin_.endParameterGesture(geiger::parameterId(geiger::Rate));
    plugin_.endParameterGesture(geiger::parameterId(geiger::Distance)); dragging_=false;
}
void GeigerVisuals::moveProbe(const juce::MouseEvent& e) {
    if(!dragging_) return;
    const auto r=plot(); if(r.isEmpty()) return;
    const double x=unit((e.position.x-r.getX())/r.getWidth()),y=unit((e.position.y-r.getY())/r.getHeight());
    plugin_.setParameterFromGui(geiger::parameterId(geiger::Rate),std::expm1(x*std::log(12001.0)));
    plugin_.setParameterFromGui(geiger::parameterId(geiger::Distance),100*y); refresh();
}
void GeigerVisuals::mouseDown(const juce::MouseEvent& e) {
    if(kind()!=Kind::Probe || !plot().contains(e.position)) return;
    dragging_=true; plugin_.beginParameterGesture(geiger::parameterId(geiger::Rate)); plugin_.beginParameterGesture(geiger::parameterId(geiger::Distance)); moveProbe(e);
}
void GeigerVisuals::mouseDrag(const juce::MouseEvent& e) {moveProbe(e);}
void GeigerVisuals::mouseUp(const juce::MouseEvent&) {endGesture();}
bool GeigerVisuals::dataIsFinite() const noexcept {
    for(const auto* d:{&current_,&default_}) {
        if(!std::isfinite(d->peak)) return false;
        for(const auto* a:{&d->low,&d->high,&d->curve}) for(float x:*a) if(!std::isfinite(x)) return false;
    }
    return true;
}
