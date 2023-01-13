
#include <sstream>
#include "Squinky.hpp"
#include "WidgetComposite.h"

#ifdef _MIX4
#include "DrawTimer.h"
#include "MixerModule.h"
#include "Mix4.h"
#include "ctrl/SqHelper.h"
#include "ctrl/SqMenuItem.h"
#include "ctrl/SqToggleLED.h"


#include "ctrl/SqWidgets.h"

#ifdef _TIME_DRAWING
static DrawTimer drawTimer("Mix4");
#endif

using Comp = Mix4<WidgetComposite>;

/**
 */
struct Mix4Module : public MixerModule
{
public:
    Mix4Module();
    /**
     *
     * Overrides of Module functions
     */
    
    void onSampleRateChange() override;

    // Override MixerModule
    void internalProcess() override;
    //void requestModuleSolo(SoloCommands) override;
    int getNumGroups() const override { return Comp::numGroups; }
    int getMuteAllParam() const override { return Comp::ALL_CHANNELS_OFF_PARAM; }
    int getSolo0Param() const override { return Comp::SOLO0_PARAM; }

protected:
    void setExternalInput(const float*) override;
    void setExternalOutput(float*) override;
private:
    std::shared_ptr<Comp> Mix4;

};

void Mix4Module::onSampleRateChange()
{
    Mix4->onSampleRateChange();
}

void Mix4Module::setExternalInput(const float* buf)
{
    Mix4->setExpansionInputs(buf);
}

void Mix4Module::setExternalOutput(float* buf)
{
    Mix4->setExpansionOutputs(buf);
}

Mix4Module::Mix4Module()
{
    config(Comp::NUM_PARAMS, Comp::NUM_INPUTS, Comp::NUM_OUTPUTS, Comp::NUM_LIGHTS);
    for (int channel = 0; channel<Comp::numChannels; ++channel) {
        configInput(channel + Comp::AUDIO0_INPUT, "Channel " + std::to_string(channel+1) + " Audio");
        configOutput(channel + Comp::CHANNEL0_OUTPUT, "Channel " + std::to_string(channel+1) + " Audio");
        configInput(channel + Comp::MUTE0_INPUT, "Channel " + std::to_string(channel+1) + " Mute CV");
        configInput(channel + Comp::LEVEL0_INPUT, "Channel " + std::to_string(channel+1) + " Volume Control CV");
        configInput(channel + Comp::PAN0_INPUT, "Channel " + std::to_string(channel+1) + " Pan Control CV");
    }
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    SqHelper::setupParams(icomp, this); 
    Mix4 = std::make_shared<Comp>(this);
    Mix4->init();
}

void Mix4Module::internalProcess()
{
    Mix4->step();
}

////////////////////
// module widget
////////////////////

struct Mix4Widget : ModuleWidget
{
    Mix4Widget(Mix4Module *);
 
    Label* addLabel(const Vec& v, const char* str, const NVGcolor& color = SqHelper::COLOR_BLACK)
    {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
        return label;
    }

    void makeStrip(
        Mix4Module*,
        std::shared_ptr<IComposite>,
        int channel);
    void appendContextMenu(Menu *menu) override;

#ifdef _TIME_DRAWING
    // Mix4: avg = 109.139913, stddev = 21.922893 (us) Quota frac=0.654839
    void draw(const DrawArgs &args) override
    {
        DrawLocker l(drawTimer);
        ModuleWidget::draw(args);
    }
#endif
private:
    Mix4Module* mixModule;
};

void Mix4Widget::appendContextMenu(Menu *menu)
{
    MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
   
    MenuLabel *spacerLabel2 = new MenuLabel();
    menu->addChild(spacerLabel2);
    SqMenuItem_BooleanParam2 * item = new SqMenuItem_BooleanParam2(mixModule, Comp::PRE_FADERa_PARAM);
    item->text = "Send 1 Pre-Fader";
    menu->addChild(item);

    item = new SqMenuItem_BooleanParam2(mixModule, Comp::PRE_FADERb_PARAM);
    item->text = "Send 2 Pre-Fader";
    menu->addChild(item);

    item = new SqMenuItem_BooleanParam2(mixModule, Comp::CV_MUTE_TOGGLE);
    item->text = "Mute CV toggles on/off";
    menu->addChild(item);
}

static const float channelX = 21;
static const float dX = 36;

static const float channelY = 350;
static const float channelDy = 30;   
static float volY = 0;

void Mix4Widget::makeStrip(
    Mix4Module*,
    std::shared_ptr<IComposite> icomp,
    int channel)
{
    const float x = channelX + channel * dX;

    float y = channelY;

    addOutput(createOutputCentered<PJ301MPort>(
        Vec(x, y),
        module,
        channel + Comp::CHANNEL0_OUTPUT));

    y -= channelDy;
    addInput(createInputCentered<PJ301MPort>(
        Vec(x, y),
        module,
        channel + Comp::AUDIO0_INPUT));

    y -= channelDy;
    addInput(createInputCentered<PJ301MPort>(
        Vec(x, y),
        module,
        channel + Comp::MUTE0_INPUT));

    y -= channelDy;
    addInput(createInputCentered<PJ301MPort>(
        Vec(x, y),
        module,
        channel + Comp::LEVEL0_INPUT));

    y -= channelDy;
    addInput(createInputCentered<PJ301MPort>(
        Vec(x, y),
        module,
        channel + Comp::PAN0_INPUT));

    y -= (channelDy -1);

    const float mutx = x-11;
    const float muty = y-12;
    auto _mute = SqHelper::createParam<LEDBezel>(
        icomp,
        Vec(mutx, muty),
        module,
        channel + Comp::MUTE0_PARAM);
    addParam(_mute);

    addChild(createLight<MuteLight<SquinkyLight>>(
        Vec(mutx + 2, muty + 2),
        module,
        channel + Comp::MUTE0_LIGHT));

    
    y -= (channelDy-1);
    SqToggleLED* tog = (createLight<SqToggleLED>(
        Vec(x-11, y-12),
        module,
        channel + Comp::SOLO0_LIGHT));
    std::string sLed = asset::system("res/ComponentLibrary/VCVBezel.svg");
    tog->addSvg(sLed.c_str(), true);
    tog->addSvg("res/SquinkyBezel.svg");
    tog->setHandler( [this, channel](bool ctrlKey) {
        sqmix::handleSoloClickFromUI<Comp>(mixModule, channel, ctrlKey);
    });
    addChild(tog);
   
    const float extraDy = 5;
    y -= (channelDy + extraDy);
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x, y),
        module,
        channel + Comp::GAIN0_PARAM));
    volY = y;

    y -= (channelDy + extraDy);
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x, y),
        module,
        channel + Comp::PAN0_PARAM));



    y -= (channelDy + extraDy);
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x, y),
        module,
        channel + Comp::SENDb0_PARAM));

    y -= (channelDy + extraDy);
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x, y),
        module,
        channel + Comp::SEND0_PARAM));
}

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */

Mix4Widget::Mix4Widget(Mix4Module *module)
{
    setModule(module);
    mixModule = module;
    box.size = Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    SqHelper::setPanel(this, "res/mix4_panel.svg");
    std::shared_ptr<IComposite> icomp = Comp::getDescription();

    for (int i=0; i< Comp::numChannels; ++i) {
        makeStrip(module, icomp, i);
    }

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild( createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model *modelMix4Module = createModel<Mix4Module, Mix4Widget>("squinkylabs-mix4");
#endif

