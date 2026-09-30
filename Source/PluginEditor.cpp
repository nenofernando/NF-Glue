#include "PluginEditor.h"
#include "NFGlueBinaryData.h"
#include "FactoryPresets.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.0.0-test"
#endif

namespace
{
juce::Image readyAsset(const char* data, int size) { return juce::ImageCache::getFromMemory(data, size); }

// Base layout (1200x400): knob centres and the shared knob row.
constexpr float kKnobY = 180.0f;
// Five knobs evenly spaced (~207 apart); the vertical GR meter sits under the Power button (same centre X).
constexpr float kThresholdX = 133.0f, kRatioX = 340.0f, kAttackX = 546.0f, kReleaseX = 753.0f, kOutputX = 960.0f, kMeterX = 1108.0f;
constexpr float kKnobBox = 145.0f;

juce::String formatSeconds(double v){ auto s=juce::String(v,2); if(s.endsWithChar('0')) s=s.dropLastCharacters(1); return s+" s"; }
juce::String formatMs(double v){ return juce::String(v,1)+" ms"; }
juce::String formatOut(double v){ auto s=juce::String(v,1); if(v>0.05) s="+"+s; else s="0.0"; return s+" dB"; }
juce::String formatDb(double v){ return juce::String(juce::roundToInt(v))+" dB"; }
juce::String formatRatio(double v,bool doubled){ const int base = nfglue::kRatioSteps[juce::jlimit(0,nfglue::kNumRatioSteps-1,juce::roundToInt(v))]; return juce::String(doubled ? base*2 : base)+":1"; }
}

void NFGluePowerButton::paintButton(juce::Graphics& g,bool,bool)
{
    NFGlueLookAndFeel::drawPowerBody(g, getLocalBounds().toFloat());
}

NFGlueAudioProcessorEditor::NFGlueAudioProcessorEditor(NFGlueAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),grMeter(p.gainReductionDb),
     thresholdCap("-40 to 0 dB",-20.0,true,[this](double v){ return formatDb(v - (boost2x.getToggleState() ? nfglue::kBoostThresholdDropDb : 0.0)); }),ratioCap("2:1 - 20:1",(double)nfglue::kDefaultRatioIndex,true,[this](double v){ return formatRatio(v, boost2x.getToggleState()); }),
     attackCap("0.5-10 ms",5.5,false,formatMs),releaseCap("0.25-2.5 s",1.25,false,formatSeconds),
     outputCap("0 to +24 dB",0.0,false,formatOut)
{
    setLookAndFeel(&look);
    setResizable(true,true);
    getConstrainer()->setFixedAspectRatio(3.0);
    getConstrainer()->setSizeLimits(750,250,1800,600);
    {
        const int w = juce::jlimit(750, 1800, (int) p.apvts.state.getProperty("uiWidth", 810));   // size chosen with the resize handle survives close / reopen
        setSize(w, w/3);
    }

    addAndMakeVisible(logoButton);
    logoButton.setTooltip("Double-click: reset UI size");
    logoButton.onDoubleClick = [this]{ setSize(810,270); };

    addAndMakeVisible(menuButton);
    menuButton.setTooltip("About");
    menuButton.onClick = [this]{ showMainMenu(); };
    addAndMakeVisible(presetBar);
    presetBar.setTooltip("Preset: click the name for the list, arrows = previous / next");
    presetBar.onPrev = [this]{ stepPreset(-1); };
    presetBar.onNext = [this]{ stepPreset(+1); };
    presetBar.onMenu = [this]{ showPresetMenu(); };
    presetBar.setName(nfglue::PresetManager::getCurrentPresetName(processor.apvts));
    processor.apvts.state.addListener(this);

    struct K{ juce::Slider* s; double def; };
    for(auto k:{K{&thresholdKnob,-20.0},K{&ratioKnob,(double)nfglue::kDefaultRatioIndex},K{&attackKnob,5.5},K{&releaseKnob,1.25},K{&outputKnob,0.0}}){
        addAndMakeVisible(*k.s);
        k.s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        // Same 270-degree sweep as the tick marks drawn in drawScale().
        k.s->setRotaryParameters(juce::MathConstants<float>::pi*1.25f, juce::MathConstants<float>::pi*2.75f, true);
        k.s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        k.s->setDoubleClickReturnValue(true,k.def);
    }
    for(auto* c:{&thresholdCap,&ratioCap,&attackCap,&releaseCap,&outputCap}) addAndMakeVisible(*c);
    addAndMakeVisible(power);power.setClickingTogglesState(true);
    addAndMakeVisible(boost2x);boost2x.setClickingTogglesState(true);boost2x.setTooltip("2x: doubles the ratio and lowers the threshold by 6 dB (heavier compression)");
    boost2x.onStateChange=[this]{ratioCap.repaint();thresholdCap.repaint();};
    for(auto* b:{&thresholdBubble,&ratioBubble,&attackBubble,&releaseBubble}) addAndMakeVisible(*b);
    addAndMakeVisible(outputBubble);
    addAndMakeVisible(grMeter);

    // Floating value readouts, only while the user is interacting (never on host automation).
    thresholdKnob.onValueChange = [this]{ if (thresholdKnob.isMouseOverOrDragging()) thresholdBubble.showRaw(formatDb(thresholdKnob.getValue() - (boost2x.getToggleState() ? nfglue::kBoostThresholdDropDb : 0.0))); };
    ratioKnob.onValueChange     = [this]{ if (ratioKnob.isMouseOverOrDragging())     ratioBubble.showRaw(formatRatio(ratioKnob.getValue(), boost2x.getToggleState())); };
    attackKnob.onValueChange    = [this]{ if (attackKnob.isMouseOverOrDragging())    attackBubble.showRaw(formatMs(attackKnob.getValue())); };
    releaseKnob.onValueChange   = [this]{ if (releaseKnob.isMouseOverOrDragging())   releaseBubble.showRaw(formatSeconds(releaseKnob.getValue())); };
    outputKnob.onValueChange    = [this]{ if (outputKnob.isMouseOverOrDragging())    outputBubble.showRaw(formatOut(outputKnob.getValue())); };

    auto& a=processor.apvts;
    thresholdA=std::make_unique<SA>(a,"threshold",thresholdKnob);ratioA=std::make_unique<SA>(a,"ratio",ratioKnob);
    attackA=std::make_unique<SA>(a,"attack",attackKnob);releaseA=std::make_unique<SA>(a,"release",releaseKnob);
    thresholdCapA=std::make_unique<SA>(a,"threshold",thresholdCap.slider);ratioCapA=std::make_unique<SA>(a,"ratio",ratioCap.slider);
    attackCapA=std::make_unique<SA>(a,"attack",attackCap.slider);releaseCapA=std::make_unique<SA>(a,"release",releaseCap.slider);
    powerA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"power",power);
    boostA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"ratio2x",boost2x);
    power.onStateChange=[this]{repaint();};
    outputGainA=std::make_unique<SA>(a,"outputGain",outputKnob);outputCapA=std::make_unique<SA>(a,"outputGain",outputCap.slider);
}
NFGlueAudioProcessorEditor::~NFGlueAudioProcessorEditor(){processor.apvts.state.removeListener(this);cancelPendingUpdate();setLookAndFeel(nullptr);}

juce::Rectangle<int> NFGlueAudioProcessorEditor::scaleBounds(juce::Rectangle<float> b) const
{
    return { juce::roundToInt(offsetX + b.getX()*layoutScale), juce::roundToInt(offsetY + b.getY()*layoutScale),
             juce::roundToInt(b.getWidth()*layoutScale), juce::roundToInt(b.getHeight()*layoutScale) };
}

void NFGlueAudioProcessorEditor::showMainMenu()
{
    juce::PopupMenu menu;
    menu.addItem(3, "About");
    juce::Component::SafePointer<NFGlueAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0) return;
            if (result == 3) safeThis->showAbout();
        });
}

// Preset tab: factory presets, then save / load
void NFGlueAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto current = nfglue::PresetManager::getCurrentPresetName(processor.apvts);
    for (int i = 0; i < nfglue::kNumFactoryPresets; ++i)
        menu.addItem(100 + i, nfglue::kFactoryPresets[i].name, true, current == nfglue::kFactoryPresets[i].name);
    menu.addSeparator();
    menu.addItem(1, "Save Preset...");
    menu.addItem(2, "Load Preset...");
    juce::Component::SafePointer<NFGlueAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBar).withMinimumWidth(presetBar.getWidth()),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0) return;
            if (result == 1) safeThis->handleSavePreset();
            else if (result == 2) safeThis->handleLoadPreset();
            else if (result >= 100) nfglue::PresetManager::applyFactoryPreset(safeThis->processor.apvts, result - 100);
        });
}

void NFGlueAudioProcessorEditor::stepPreset(int direction)
{
    const auto current = nfglue::PresetManager::getCurrentPresetName(processor.apvts);
    int index = -1;
    for (int i = 0; i < nfglue::kNumFactoryPresets; ++i) if (current == nfglue::kFactoryPresets[i].name) { index = i; break; }
    const int n = nfglue::kNumFactoryPresets;
    index = index < 0 ? (direction > 0 ? 0 : n - 1) : (index + direction + n) % n;
    nfglue::PresetManager::applyFactoryPreset(processor.apvts, index);
}

void NFGlueAudioProcessorEditor::showAbout()
{
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "About NF Glue",
        juce::String("NF Glue V") + JucePlugin_VersionString + "\nNF Audio Tools - Nenno Fernando");
}

void NFGlueAudioProcessorEditor::handleSavePreset()
{
    presetFileChooser = std::make_unique<juce::FileChooser>("Save NF Glue Preset", nfglue::PresetManager::getPresetsDirectory(), "*.nfgluepreset");
    juce::Component::SafePointer<NFGlueAudioProcessorEditor> safeThis(this);
    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;
    presetFileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            if (!file.hasFileExtension("nfgluepreset")) file = file.withFileExtension("nfgluepreset");
            auto result = nfglue::PresetManager::savePreset(safeThis->processor.apvts, file);
            if (result.failed())
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NF Glue", result.getErrorMessage());
        }
        safeThis->presetFileChooser.reset();
    });
}

void NFGlueAudioProcessorEditor::handleLoadPreset()
{
    presetFileChooser = std::make_unique<juce::FileChooser>("Load NF Glue Preset", nfglue::PresetManager::getPresetsDirectory(), "*.nfgluepreset");
    juce::Component::SafePointer<NFGlueAudioProcessorEditor> safeThis(this);
    presetFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                auto result = nfglue::PresetManager::loadPreset(safeThis->processor.apvts, file);
                if (result.failed())
                    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NF Glue", result.getErrorMessage());
            }
            safeThis->presetFileChooser.reset();
        });
}

void NFGlueAudioProcessorEditor::drawScale(juce::Graphics& g,juce::Point<float> c,const std::vector<Tick>& ticks)
{
    const float radius = 85.0f;
    g.setColour(juce::Colours::white);
    for (const auto& t : ticks)
    {
        const float a = t.deg * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        const float len = t.major ? 13.0f : 6.0f;
        g.drawLine(juce::Line<float>(c + dir*radius, c + dir*(radius+len)), t.major ? 2.4f : 1.4f);
        if (t.label.isNotEmpty())
        {
            const auto p = c + dir*(radius+t.labelOffset);
            g.setFont(juce::Font(juce::FontOptions(t.fontSize)));
            g.drawText(t.label, juce::Rectangle<float>(p.x-26.0f, p.y-12.0f, 52.0f, 24.0f), juce::Justification::centred);
        }
    }
}

void NFGlueAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a1b11));
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(layoutScale).translated(offsetX, offsetY));

    static const juce::Image chassis = readyAsset(NFGlueBinaryData::_01_chassis_1200x400_png, NFGlueBinaryData::_01_chassis_1200x400_pngSize);
    {
        juce::Graphics::ScopedSaveState s(g);
        g.setOpacity(1.0f);
        if (chassis.isValid()) g.drawImage(chassis, {0.0f,0.0f,1200.0f,400.0f}, juce::RectanglePlacement::stretchToFit);
    }

    g.setColour(juce::Colour(0xffeef2ee));
    { static const juce::Image nfLogo=readyAsset(NFGlueBinaryData::_10_logo_nf_audio_tools_png,NFGlueBinaryData::_10_logo_nf_audio_tools_pngSize);
      if(nfLogo.isValid()) g.drawImage(nfLogo,juce::Rectangle<float>(42.0f,3.0f,108.0f,62.0f),juce::RectanglePlacement::centred); }
    g.drawLine(148.0f,14.0f,148.0f,45.0f,2.0f);
    g.setFont(juce::Font(juce::FontOptions(30.0f,juce::Font::bold)).withExtraKerningFactor(.08f));
    g.drawText("NF GLUE",168,9,230,44,juce::Justification::centredLeft);

    // Tick marks: 270-degree sweep, 0 deg = straight up, clockwise (matches the rotary parameters).
    auto stepped = [](std::initializer_list<const char*> labels){ std::vector<Tick> t; int i=0; const int n=(int)labels.size();
        for(auto* l:labels) t.push_back({-135.0f+(float)i++*270.0f/(float)(n-1), l, true, 15.0f}); return t; };
    auto continuous = [](const juce::String& lo,const juce::String& mid,const juce::String& hi)
    {
        std::vector<Tick> t; for(int i=0;i<=12;++i) t.push_back({-135.0f+i*22.5f, {}, i%3==0, 14.0f});
        t[0].label=lo; t[6].label=mid; t[12].label=hi; t[6].fontSize=16.0f; return t;
    };
    drawScale(g,{kThresholdX,kKnobY},continuous("-40","-20","0")); // same scale style as Attack/Release/Output
    { // Ratio: the five original steps keep the normal size; the in-between ones are smaller so the dial stays clean.
      std::vector<Tick> t; const int n = nfglue::kNumRatioSteps;
      for (int i = 0; i < n; ++i)
      {
          const int r = nfglue::kRatioSteps[i];
          const bool big = (r == 2 || r == 4 || r == 8 || r == 12 || r == 20);
          const float deg = -135.0f + (float)i * 270.0f / (float)(n - 1);
          // Side labels sit closer to the dial so they stay clear of the neighbouring Threshold scale.
          t.push_back({deg, juce::String(r), big, big ? 15.0f : 10.5f, std::abs(deg) >= 60.0f ? 19.0f : 29.0f});
      }
      drawScale(g,{kRatioX,kKnobY},t); }
    drawScale(g,{kAttackX,kKnobY},continuous("0.5ms","5.5ms","10ms"));
    drawScale(g,{kReleaseX,kKnobY},continuous("0.25s","1.25s","2.5s"));
    drawScale(g,{kOutputX,kKnobY},continuous("0","+12","+24")); // makeup gain: 0 dB at the far left

    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(20.0f,juce::Font::bold)));
    const std::pair<float,const char*> names[]={{kThresholdX,"THRESHOLD"},{kRatioX,"RATIO"},{kAttackX,"ATTACK"},{kReleaseX,"RELEASE"},{kOutputX,"OUTPUT"}};
    for (auto& n : names) g.drawText(n.second, juce::Rectangle<int>((int)n.first-80,266,160,24), juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(15.0f,juce::Font::bold)));
    g.drawText("GAIN RED", juce::Rectangle<int>((int)kMeterX-50,266,100,24), juce::Justification::centred);

    NFGlueLookAndFeel::drawScrew(g, {7.0f,    14.0f, 46.0f, 46.0f});
    NFGlueLookAndFeel::drawScrew(g, {1143.0f, 14.0f, 46.0f, 46.0f});
    NFGlueLookAndFeel::drawScrew(g, {7.0f,    329.0f, 46.0f, 46.0f});
    NFGlueLookAndFeel::drawScrew(g, {1143.0f, 329.0f, 46.0f, 46.0f});
    const bool powered = processor.apvts.getRawParameterValue("power")->load() > 0.5f;
    NFGlueLookAndFeel::drawLed(g, {1095.0f, 24.0f, 20.0f, 20.0f}, powered);

    // Footer signature, flanked by thin lines.
    g.setColour(juce::Colours::white);g.setFont(15.0f);
    g.drawText("NF AUDIO TOOLS", juce::Rectangle<int>(0,358,1200,18), juce::Justification::centred);
    juce::GlyphArrangement footerGlyphs;
    footerGlyphs.addLineOfText(g.getCurrentFont(), "NF AUDIO TOOLS", 0.0f, 0.0f);
    const float footerTextWidth = footerGlyphs.getBoundingBox(0,-1,true).getWidth();
    const float midX = 600.0f, lineY = 367.0f, gap = footerTextWidth*0.5f + 14.0f;
    g.drawLine(midX-190.0f, lineY, midX-gap, lineY, 1.4f);
    g.drawLine(midX+gap, lineY, midX+190.0f, lineY, 1.4f);

    // Version, bottom-left; derived from CMakeLists.txt's project(NFGlue VERSION ...).
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawText("V" JucePlugin_VersionString, juce::Rectangle<int>(70,357,80,20), juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(15.0f,juce::Font::bold)));
}

void NFGlueAudioProcessorEditor::resized()
{
    if (getWidth() > 0) processor.apvts.state.setProperty("uiWidth", getWidth(), nullptr);   // remembered for the next time the window opens
    const float scaleX = getWidth()  / 1200.0f, scaleY = getHeight() / 400.0f;
    layoutScale = juce::jmin(scaleX, scaleY);
    offsetX = (getWidth()  - 1200.0f * layoutScale) * 0.5f;
    offsetY = (getHeight() - 400.0f  * layoutScale) * 0.5f;

    auto layoutBand = [&](juce::Slider& knob, ValueCapsule& cap, float cx)
    {
        knob.setBounds(scaleBounds({cx-kKnobBox*0.5f, kKnobY-kKnobBox*0.5f, kKnobBox, kKnobBox}));
        cap.setBounds(scaleBounds({cx-65.0f, 294.0f, 130.0f, 54.0f}));
    };
    layoutBand(thresholdKnob,thresholdCap,kThresholdX);layoutBand(ratioKnob,ratioCap,kRatioX);
    layoutBand(attackKnob,attackCap,kAttackX);layoutBand(releaseKnob,releaseCap,kReleaseX);
    layoutBand(outputKnob,outputCap,kOutputX);

    boost2x.setBounds(scaleBounds({kRatioX+75.0f, 294.0f, 48.0f, 32.0f}));
    power.setBounds(scaleBounds({1075.0f, 43.0f, 66.0f, 66.0f}));
    // Bar (27px into the 80px box) is centred on the Power button's X.
    grMeter.setBounds(scaleBounds({kMeterX-27.0f, 106.0f, 80.0f, 154.0f}));
    outputBubble.setBounds(scaleBounds({kOutputX-38.0f, 202.0f, 76.0f, 24.0f}));
    logoButton.setBounds(scaleBounds({42.0f, 3.0f, 108.0f, 62.0f}));
    thresholdBubble.setBounds(scaleBounds({kThresholdX-38.0f, 202.0f, 76.0f, 24.0f}));
    ratioBubble.setBounds(scaleBounds({kRatioX-38.0f, 202.0f, 76.0f, 24.0f}));
    attackBubble.setBounds(scaleBounds({kAttackX-38.0f, 202.0f, 76.0f, 24.0f}));
    releaseBubble.setBounds(scaleBounds({kReleaseX-38.0f, 202.0f, 76.0f, 24.0f}));
    menuButton.setBounds(scaleBounds({1020.0f, 25.0f, 34.0f, 28.0f}));
    presetBar.setBounds(scaleBounds({848.0f, 28.0f, 157.0f, 21.0f}));
}
