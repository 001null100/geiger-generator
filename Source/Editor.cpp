#include "Editor.hpp"
#include "Plugin.hpp"
#include "Parameters.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
const juce::Colour background{0xff10121d},panel{0xff1b2031},raised{0xff252c42},line{0xff414b69};
const juce::Colour text{0xfff1f3fc},muted{0xffb5bdd6},amber{0xffefc274},violet{0xffb8a0ff},rose{0xffe887a1};
constexpr float pi=juce::MathConstants<float>::pi;
juce::Font font(float size,bool bold=false) {return juce::Font(juce::FontOptions(size,bold ? juce::Font::bold : juce::Font::plain));}
void card(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour colour=panel) {
    g.setColour(colour); g.fillRoundedRectangle(r,14);
    g.setColour(line.withAlpha(0.5f)); g.drawRoundedRectangle(r.reduced(0.5f),14,1);
}
juce::String valueText(std::size_t i,double value) {
    const auto& d=geiger::definitions[i];
    if((i==geiger::Hiss || i==geiger::Hum) && value<=-89.9) return "Off";
    if(i==geiger::Seed) return juce::String(static_cast<int>(value));
    int decimals=(d.maximum<=20 || (i==geiger::Rate && value<100)) ? 2 : 1;
    if(i==geiger::Tone || i==geiger::DeadTime || (i==geiger::Rate && value>=100)) decimals=0;
    return juce::String(value,decimals)+(d.unit[0] ? " "+juce::String(d.unit) : "");
}
}
class GeigerLook final : public juce::LookAndFeel_V4 {
public:
    GeigerLook() {
        setColour(juce::Slider::textBoxTextColourId,text); setColour(juce::Slider::textBoxBackgroundColourId,background);
        setColour(juce::Slider::textBoxOutlineColourId,line); setColour(juce::Slider::trackColourId,violet);
        setColour(juce::Slider::thumbColourId,amber); setColour(juce::Slider::backgroundColourId,raised);
        setColour(juce::ComboBox::backgroundColourId,raised); setColour(juce::ComboBox::textColourId,text);
        setColour(juce::ComboBox::outlineColourId,line); setColour(juce::ComboBox::arrowColourId,amber);
        setColour(juce::PopupMenu::backgroundColourId,panel); setColour(juce::PopupMenu::textColourId,text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,line); setColour(juce::PopupMenu::highlightedTextColourId,text);
        setColour(juce::TextButton::buttonColourId,raised); setColour(juce::TextButton::buttonOnColourId,line);
        setColour(juce::TextButton::textColourOffId,text); setColour(juce::TextButton::textColourOnId,amber);
        setColour(juce::ToggleButton::textColourId,muted); setColour(juce::ToggleButton::tickColourId,amber);
        setColour(juce::TooltipWindow::backgroundColourId,raised); setColour(juce::TooltipWindow::textColourId,text);
        setColour(juce::TooltipWindow::outlineColourId,line);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {return font(14);}
    juce::Font getTextButtonFont(juce::TextButton&,int) override {return font(14,true);}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,float start,float end,juce::Slider& s) override {
        auto r=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)).reduced(8);
        const float radius=std::min(r.getWidth(),r.getHeight())*0.5f;
        const auto center=r.getCentre(); const float angle=start+position*(end-start);
        juce::Path base,active; base.addCentredArc(center.x,center.y,radius,radius,0,start,end,true);
        active.addCentredArc(center.x,center.y,radius,radius,0,start,angle,true);
        g.setColour(line); g.strokePath(base,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(s.isEnabled() ? amber : muted); g.strokePath(active,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float body=radius*0.75f;
        g.setGradientFill(juce::ColourGradient(raised.brighter(0.2f),center.x,center.y-body,background,center.x,center.y+body,false));
        g.fillEllipse(center.x-body,center.y-body,2*body,2*body);
        g.setColour(line.brighter(0.2f)); g.drawEllipse(center.x-body,center.y-body,2*body,2*body,1);
        const float ax=std::sin(angle),ay=-std::cos(angle);
        g.setColour(text); g.drawLine(center.x+ax*body*0.45f,center.y+ay*body*0.45f,center.x+ax*body*0.87f,center.y+ay*body*0.87f,3);
        if(s.hasKeyboardFocus(true)) {g.setColour(violet); g.drawEllipse(center.x-radius-5,center.y-radius-5,2*radius+10,2*radius+10,1.5f);}
    }
};
class GeigerControl final : public juce::Component {
public:
    GeigerControl(GeigerPlugin& p,std::size_t index,std::function<void(const char*)> hover):plugin(p),id(index),onHover(std::move(hover)) {
        const auto& d=geiger::definitions[id];
        setName(d.name); setTitle(d.name); setDescription(d.help); addMouseListener(this,true);
        if(d.choices[0]) {
            auto options=juce::StringArray::fromTokens(d.choices,"|","");
            combo.addItemList(options,1); combo.setTooltip(d.help); combo.setTitle(d.name);
            addAndMakeVisible(combo);
            combo.onChange=[this]{gesture(combo.getSelectedId()-1);};
        } else {
            addAndMakeVisible(slider); slider.setTitle(d.name); slider.setTooltip(d.help);
            slider.setSliderStyle(id==geiger::Output ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,id==geiger::Rate ? 150 : 114,25);
            slider.setRange(d.minimum,d.maximum,id==geiger::Seed ? 1.0 : 0.0);
            if(d.midpoint>d.minimum && d.midpoint<d.maximum) slider.setSkewFactorFromMidPoint(d.midpoint);
            slider.setDoubleClickReturnValue(true,d.initial); slider.setScrollWheelEnabled(false);
            slider.setNumDecimalPlacesToDisplay(2); slider.setMouseDragSensitivity(id==geiger::Rate ? 260 : 180);
            slider.textFromValueFunction=[this](double v){return valueText(id,v);};
            slider.valueFromTextFunction=[this](const juce::String& s){return s.trim().equalsIgnoreCase("off") ? geiger::definitions[id].minimum : s.getDoubleValue();};
            slider.onDragStart=[this]{dragging=true; plugin.beginParameterGesture(geiger::parameterId(id));};
            slider.onValueChange=[this]{if(dragging) plugin.setParameterFromGui(geiger::parameterId(id),slider.getValue()); else gesture(slider.getValue());};
            slider.onDragEnd=[this]{plugin.endParameterGesture(geiger::parameterId(id)); dragging=false;};
        }
        refresh();
    }
    ~GeigerControl() override {if(dragging) plugin.endParameterGesture(geiger::parameterId(id));}
    void refresh() {
        if(dragging || slider.isMouseButtonDown() || combo.isPopupActive()) return;
        for(int i=0;i<slider.getNumChildComponents();++i)
            if(auto* label=dynamic_cast<juce::Label*>(slider.getChildComponent(i)); label && label->isBeingEdited()) return;
        const double v=plugin.parameters().value(geiger::parameterId(id));
        if(geiger::definitions[id].choices[0]) combo.setSelectedId(static_cast<int>(v)+1,juce::dontSendNotification);
        else slider.setValue(v,juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {
        const bool hero=id==geiger::Rate;
        if(id!=geiger::Output && id!=geiger::ClickModel) card(g,getLocalBounds().toFloat(),hero ? juce::Colour(0xff262435) : panel);
        g.setColour(hero ? amber : muted); g.setFont(font(hero ? 15 : 14,hero));
        g.drawText(geiger::definitions[id].name,10,5,getWidth()-20,24,juce::Justification::centred);
        if(hero) {g.setFont(font(12)); g.setColour(muted); g.drawText("INCOMING EVENTS / SECOND",5,getHeight()-22,getWidth()-10,17,juce::Justification::centred);}
    }
    void resized() override {
        auto r=getLocalBounds().reduced(8); r.removeFromTop(24);
        if(id==geiger::Rate) r.removeFromBottom(20);
        if(geiger::definitions[id].choices[0]) combo.setBounds(r.withSizeKeepingCentre(std::min(r.getWidth(),230),34));
        else slider.setBounds(r);
    }
    void mouseEnter(const juce::MouseEvent&) override {onHover(geiger::definitions[id].help);}
    std::size_t parameter() const {return id;}
    bool editing() const {return dragging;}
private:
    void gesture(double v) {const auto pid=geiger::parameterId(id); plugin.beginParameterGesture(pid); plugin.setParameterFromGui(pid,v); plugin.endParameterGesture(pid);}
    GeigerPlugin& plugin;
    std::size_t id;
    std::function<void(const char*)> onHover;
    juce::Slider slider;
    juce::ComboBox combo;
    bool dragging=false;
};
class GeigerMonitor final : public juce::Component {
public:
    explicit GeigerMonitor(GeigerPlugin& p):plugin(p) {setTitle("Detector activity"); setDescription("Actual simulated detector count rate and audio pulse trace.");}
    void tick(bool calm) {
        const auto& t=plugin.telemetry();
        const auto total=t.detected.load(std::memory_order_relaxed);
        flash=calm ? 0.0f : std::min(1.0f,flash*0.72f+(total!=previous ? 0.45f : 0.0f)); previous=total;
        const float target=std::clamp(std::log10(t.cps.load(std::memory_order_relaxed)+1.0f)/4.08f,0.0f,1.0f);
        needle+=0.2f*(target-needle); repaint();
    }
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat(); card(g,r);
        const auto& t=plugin.telemetry();
        const float width=r.getWidth(),height=r.getHeight();
        g.setColour(muted); g.setFont(font(12,true)); g.drawText("DETECTOR / LIVE COUNTS",18,10,getWidth()-36,20,juce::Justification::left);
        g.setColour(t.running.load() ? amber : line); g.fillEllipse(width-28,18,9,9);
        const float cx=width*0.28f,cy=height*0.62f,rad=std::min(width*0.22f,height*0.44f);
        juce::Path arc; arc.addCentredArc(cx,cy,rad,rad,0,-pi*0.43f,pi*0.43f,true);
        g.setColour(line); g.strokePath(arc,juce::PathStrokeType(3));
        for(int i=0;i<=20;++i) {
            float a=-pi*0.43f+i*pi*0.86f/20;
            float s=std::sin(a),c=-std::cos(a);
            g.setColour(i>=16 ? rose : muted.withAlpha(0.7f));
            g.drawLine(cx+s*(rad-3),cy+c*(rad-3),cx+s*(rad-(i%5==0 ? 13:8)),cy+c*(rad-(i%5==0 ? 13:8)),1.5f);
        }
        float a=-pi*0.43f+needle*pi*0.86f;
        g.setColour(amber); g.drawLine(cx,cy,cx+std::sin(a)*(rad-17),cy-std::cos(a)*(rad-17),2.5f);
        g.fillEllipse(cx-5,cy-5,10,10);
        if(flash>0.01f) {g.setColour(amber.withAlpha(flash*0.18f)); g.fillEllipse(cx-15,cy-15,30,30);}
        g.setFont(font(11)); g.setColour(muted); g.drawText("LOG COUNT SCALE",static_cast<int>(cx-rad),static_cast<int>(cy+9),static_cast<int>(rad*2),16,juce::Justification::centred);
        const int tx=static_cast<int>(width*0.53f);
        const float cps=t.cps.load(std::memory_order_relaxed);
        g.setColour(text); g.setFont(font(width<360 ? 28 : 38,true));
        g.drawText(juce::String(cps,cps<100 ? 1 : 0),tx,43,getWidth()-tx-15,48,juce::Justification::left);
        g.setFont(font(13)); g.setColour(amber); g.drawText("detected / second",tx,91,getWidth()-tx-10,20,juce::Justification::left);
        const auto counts=t.detected.load(),missed=t.missed.load();
        const double loss=counts+missed>0 ? 100.0*static_cast<double>(missed)/static_cast<double>(counts+missed) : 0;
        g.setFont(font(12)); g.setColour(muted); g.drawText("Tube loss  "+juce::String(loss,1)+"%",tx,115,getWidth()-tx-10,20,juce::Justification::left);
        auto wave=r.reduced(17); wave.setY(height-37); wave.setHeight(21);
        g.setColour(background); g.fillRoundedRectangle(wave,4);
        const auto head=t.scopeHead.load(std::memory_order_acquire);
        const float dx=wave.getWidth()/128;
        g.setColour(violet.withAlpha(0.9f));
        for(std::uint32_t i=0;i<128;++i) {
            const float p=t.scope[(head+i)%128].load(std::memory_order_relaxed);
            const float h=std::min(18.0f,2.0f+40.0f*std::sqrt(std::max(p,0.0f)));
            g.fillRect(wave.getX()+i*dx,wave.getCentreY()-h*0.5f,std::max(1.0f,dx-1),h);
        }
    }
private:
    GeigerPlugin& plugin; std::uint64_t previous=0; float flash=0,needle=0;
};
class GeigerExplorer final : public juce::Component {
public:
    explicit GeigerExplorer(GeigerPlugin& p):plugin(p) {setMouseCursor(juce::MouseCursor::CrosshairCursor); setTitle("Rate and listening-distance pad");}
    ~GeigerExplorer() override {end();}
    void paint(juce::Graphics& g) override {
        card(g,getLocalBounds().toFloat());
        g.setColour(violet); g.setFont(font(12,true)); g.drawText("PROBE PLAYGROUND",16,10,getWidth()-32,20,juce::Justification::left);
        auto r=field();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff151c2d),r.getX(),r.getBottom(),juce::Colour(0xff42334e),r.getRight(),r.getY(),false));
        g.fillRoundedRectangle(r,7);
        g.setColour(line.withAlpha(0.6f));
        for(int i=1;i<6;++i) g.drawVerticalLine(static_cast<int>(r.getX()+r.getWidth()*i/6),r.getY(),r.getBottom());
        for(int i=1;i<4;++i) g.drawHorizontalLine(static_cast<int>(r.getY()+r.getHeight()*i/4),r.getX(),r.getRight());
        const auto x=static_cast<float>(std::log1p(plugin.parameters().value(geiger::parameterId(geiger::Rate)))/std::log(12001.0));
        const auto y=static_cast<float>(plugin.parameters().value(geiger::parameterId(geiger::Distance))*0.01);
        const auto pos=juce::Point<float>(r.getX()+x*r.getWidth(),r.getY()+y*r.getHeight());
        g.setColour(amber.withAlpha(0.15f)); g.fillEllipse(pos.x-19,pos.y-19,38,38);
        g.setColour(amber); g.drawEllipse(pos.x-10,pos.y-10,20,20,1.5f);
        g.setColour(text); g.fillEllipse(pos.x-4,pos.y-4,8,8);
        g.setFont(font(11)); g.setColour(muted);
        g.drawText("QUIET",16,getHeight()-42,64,17,juce::Justification::left);
        g.drawText("INTENSE",getWidth()-84,getHeight()-42,68,17,juce::Justification::right);
        g.setFont(font(12)); g.drawText("Drag: rate / listening distance",10,getHeight()-23,getWidth()-20,17,juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        active=true; plugin.beginParameterGesture(geiger::parameterId(geiger::Rate)); plugin.beginParameterGesture(geiger::parameterId(geiger::Distance)); move(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {move(e);}
    void mouseUp(const juce::MouseEvent&) override {end();}
private:
    juce::Rectangle<float> field() const {return getLocalBounds().toFloat().withTrimmedTop(40).withTrimmedBottom(46).reduced(16,0);}
    void end() {if(active) {plugin.endParameterGesture(geiger::parameterId(geiger::Rate)); plugin.endParameterGesture(geiger::parameterId(geiger::Distance)); active=false;}}
    void move(const juce::MouseEvent& e) {
        const auto r=field(); if(r.getWidth()<=0 || r.getHeight()<=0) return;
        const double x=std::clamp((e.position.x-r.getX())/r.getWidth(),0.0f,1.0f);
        const double y=std::clamp((e.position.y-r.getY())/r.getHeight(),0.0f,1.0f);
        plugin.setParameterFromGui(geiger::parameterId(geiger::Rate),std::expm1(x*std::log(12001.0)));
        plugin.setParameterFromGui(geiger::parameterId(geiger::Distance),100*y); repaint();
    }
    GeigerPlugin& plugin; bool active=false;
};
GeigerEditor::GeigerEditor(GeigerPlugin& p):plugin_(p),look_(std::make_unique<GeigerLook>()) {
    setLookAndFeel(look_.get()); setOpaque(true); setTitle("Geiger Generator");
    for(std::size_t i=0;i<geiger::Count;++i) {
        if(i==geiger::Power) continue;
        auto c=std::make_unique<GeigerControl>(p,i,[this](const char* s){help(s);}); addAndMakeVisible(*c); controls_.push_back(std::move(c));
    }
    monitor_=std::make_unique<GeigerMonitor>(p); explorer_=std::make_unique<GeigerExplorer>(p);
    addAndMakeVisible(*monitor_); addAndMakeVisible(*explorer_);
    const char* names[]{"01  FIELD","02  CIRCUIT","03  SPEAKER","04  BEHAVIOR"};
    for(int i=0;i<4;++i) {auto& b=tabs_[i]; b.setButtonText(names[i]); b.onClick=[this,i]{selectPage(i);}; addAndMakeVisible(b);}
    for(auto* b:{&power_,&test_,&seed_,&panic_}) addAndMakeVisible(*b);
    addAndMakeVisible(presets_); presets_.setTextWhenNothingSelected("Factory presets"); presets_.setTitle("Factory presets");
    for(std::size_t i=0;i<geiger::presetNames.size();++i) presets_.addItem(geiger::presetNames[i],static_cast<int>(i)+1);
    presets_.onChange=[this]{if(presets_.getSelectedId()>0) plugin_.applyPreset(static_cast<std::size_t>(presets_.getSelectedId()-1)); presets_.setSelectedId(0,juce::dontSendNotification);};
    power_.setClickingTogglesState(true); power_.onClick=[this]{auto id=geiger::parameterId(geiger::Power); plugin_.beginParameterGesture(id); plugin_.setParameterFromGui(id,power_.getToggleState()?1:0); plugin_.endParameterGesture(id);};
    power_.setTooltip(geiger::definitions[geiger::Power].help);
    test_.onClick=[this]{plugin_.testClick();}; test_.setTooltip("Audition one pulse through the current circuit, even when Transport or MIDI gate is closed. Power must be on.");
    seed_.onClick=[this]{auto id=geiger::parameterId(geiger::Seed); plugin_.beginParameterGesture(id); plugin_.setParameterFromGui(id,1+juce::Random::getSystemRandom().nextInt(999999)); plugin_.endParameterGesture(id);};
    panic_.onClick=[this]{plugin_.panic();}; panic_.setColour(juce::TextButton::textColourOffId,rose); panic_.setTooltip("Power off and clear all held notes and transducer ringing.");
    addAndMakeVisible(calm_); calm_.setTooltip("Disable detection glow while keeping the count readout and useful meters.");
    addAndMakeVisible(help_); help_.setFont(font(13)); help_.setColour(juce::Label::textColourId,muted); help_.setJustificationType(juce::Justification::centredLeft);
    help("Turn Intensity for density. Choose a click model for character. Double-click a knob to reset; type a value for precision.");
    setSize(1120,760); selectPage(0); startTimerHz(30);
}
GeigerEditor::~GeigerEditor() {stopTimer(); controls_.clear(); explorer_.reset(); monitor_.reset(); setLookAndFeel(nullptr);}
void GeigerEditor::help(const char* s) {help_.setText(s,juce::dontSendNotification);}
void GeigerEditor::selectPage(int page) {
    page_=juce::jlimit(0,3,page);
    for(auto& c:controls_) c->setVisible(geiger::definitions[c->parameter()].page<0 || geiger::definitions[c->parameter()].page==page_);
    for(int i=0;i<4;++i) tabs_[i].setToggleState(i==page_,juce::dontSendNotification);
    resized(); repaint();
}
void GeigerEditor::timerCallback() {
    for(auto& c:controls_) if(c->isVisible()) c->refresh();
    power_.setToggleState(plugin_.parameters().value(geiger::parameterId(geiger::Power))>0.5,juce::dontSendNotification);
    monitor_->tick(calm_.getToggleState()); explorer_->repaint();
}
void GeigerEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setColour(amber); g.setFont(font(30,true)); g.drawText("GEIGER",24,13,200,37,juce::Justification::left);
    g.setColour(muted); g.setFont(font(11,true)); g.drawText("GENERATOR  /  NULL AUDIO",25,48,230,21,juce::Justification::left);
    g.setColour(line); g.drawHorizontalLine(getHeight()-60,24.0f,static_cast<float>(getWidth()-24));
    g.setFont(font(11)); g.setColour(muted.withAlpha(0.8f));
    g.drawText("SYNTHETIC COUNTS. REAL CHARACTER.   /   SOUND DESIGN ONLY",24,getHeight()-25,getWidth()-48,18,juce::Justification::left);
    if(page_==3) {
        g.setColour(muted); g.setFont(font(15));
        g.drawFittedText("MIDI gate uses note velocity for density. C4 is neutral when MIDI pitch is enabled.\nSeeded playback repeats on host Reset, or on Play in Transport mode.\nField envelopes change event density; transducer tails remain free to ring.",
            40,getHeight()-190,getWidth()-80,92,juce::Justification::centred,4);
    }
}
void GeigerEditor::resized() {
    const int w=getWidth(),h=getHeight();
    const int headerLeft=std::max(218,w/5),powerWidth=82,outputWidth=145;
    power_.setBounds(w-24-powerWidth,26,powerWidth,36);
    const int outputX=w-24-powerWidth-12-outputWidth;
    const int middle=outputX-headerLeft-20;
    const int modelWidth=middle/2;
    for(auto& c:controls_) {
        if(c->parameter()==geiger::Output) c->setBounds(outputX,4,outputWidth,73);
        if(c->parameter()==geiger::ClickModel) c->setBounds(headerLeft,2,modelWidth,76);
    }
    presets_.setBounds(headerLeft+modelWidth+12,32,middle-modelWidth-12,33);
    const int top=88,displayH=std::clamp(h/3-35,170,230),inner=w-48;
    const int heroW=std::max(174,inner/5),explorerW=std::max(240,inner*29/100),monitorW=inner-heroW-explorerW-24;
    for(auto& c:controls_) if(c->parameter()==geiger::Rate) c->setBounds(24,top,heroW,displayH);
    monitor_->setBounds(24+heroW+12,top,monitorW,displayH);
    explorer_->setBounds(w-24-explorerW,top,explorerW,displayH);
    const int tabY=top+displayH+18;
    const int tabW=std::clamp((w-500)/4,100,132);
    for(int i=0;i<4;++i) tabs_[i].setBounds(24+i*(tabW+6),tabY,tabW,34);
    test_.setBounds(w-24-104,tabY,104,34); seed_.setBounds(w-24-104-8-96,tabY,96,34);
    panic_.setBounds(w-24-104-8-96-8-84,tabY,84,34);
    calm_.setBounds(w-24-104-8-96-8-84-8-124,tabY,124,34);
    const int gridY=tabY+49,gridBottom=h-70;
    std::vector<GeigerControl*> visible;
    for(auto& c:controls_) if(geiger::definitions[c->parameter()].page==page_) visible.push_back(c.get());
    const int columns=6,rows=2,gap=10,cellW=(inner-(columns-1)*gap)/columns;
    const int cellH=std::max(90,(gridBottom-gridY-gap)/rows);
    for(std::size_t i=0;i<visible.size();++i) visible[i]->setBounds(24+static_cast<int>(i%columns)*(cellW+gap),gridY+static_cast<int>(i/columns)*(cellH+gap),cellW,cellH);
    help_.setBounds(24,h-56,w-48,31);
}
bool GeigerEditor::layoutIsValid() const {
    for(const auto& c:controls_) if(c->isVisible() && !getLocalBounds().contains(c->getBounds())) return false;
    for(const auto& b:tabs_) if(!getLocalBounds().contains(b.getBounds())) return false;
    return getLocalBounds().contains(monitor_->getBounds()) && getLocalBounds().contains(explorer_->getBounds());
}
