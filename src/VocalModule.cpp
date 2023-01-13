#include "Squinky.hpp"

#ifdef _GROWLER
#include "DrawTimer.h"
#include "WidgetComposite.h"
#include "VocalAnimator.h"
#include "ctrl/SqMenuItem.h"
#include "ctrl/SqWidgets.h"

#ifdef _TIME_DRAWING
static DrawTimer drawTimer("Growler");
#endif

using Comp = VocalAnimator<WidgetComposite>;

/**
 * Implementation class for VocalWidget
 */
struct VocalModule : Module
{
    VocalModule();

    /**
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;
    std::shared_ptr<Comp> animator;
private:
    typedef float T;
};

VocalModule::VocalModule()
{
    configBypass(Comp::AUDIO_INPUT, Comp::AUDIO_OUTPUT);
    config(Comp::NUM_PARAMS, Comp::NUM_INPUTS, Comp::NUM_OUTPUTS, Comp::NUM_LIGHTS);
    configOutput(Comp::LFO0_OUTPUT,"LFO 1");
    configOutput(Comp::LFO1_OUTPUT,"LFO 2");
    configOutput(Comp::LFO2_OUTPUT,"LFO 3");
    configInput(Comp::LFO_RATE_CV_INPUT,"LFO Rate");
    configInput(Comp::FILTER_FC_CV_INPUT,"Filter FC");
    configInput(Comp::FILTER_Q_CV_INPUT,"Filter Q");
    configInput(Comp::FILTER_MOD_DEPTH_CV_INPUT,"Filter modulation depth");
    configInput(Comp::AUDIO_INPUT,"Audio");
    configOutput(Comp::AUDIO_OUTPUT,"Audio");

    animator = std::make_shared<Comp>(this);
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    SqHelper::setupParams(icomp, this);

    onSampleRateChange();
    animator->init();
}

void VocalModule::onSampleRateChange()
{
    T rate = SqHelper::engineGetSampleRate();
    animator->setSampleRate(rate);
}

void VocalModule::step()
{
    animator->step();
}

////////////////////
// module widget
////////////////////

struct VocalWidget : ModuleWidget
{
    VocalWidget(VocalModule *);

#ifdef _TIME_DRAWING
    // Growler: avg = 40.949576, stddev = 13.453620 (us) Quota frac=0.245697
    void draw(const DrawArgs &args) override
    {
        DrawLocker l(drawTimer);
        ModuleWidget::draw(args);
    }
#endif
};

struct NKK2 : SqHelper::SvgSwitch
{
    NKK2()
    {
        addFrame(APP->window->loadSvg(
            asset::system("res/ComponentLibrary/NKK_0.svg").c_str()));
        addFrame(APP->window->loadSvg(
            asset::system("res/ComponentLibrary/NKK_2.svg").c_str()));
    }
};

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */
VocalWidget::VocalWidget(VocalModule *module)
{
    setModule(module);

    const float width = 14 * RACK_GRID_WIDTH;
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    this->box.size = Vec(width, RACK_GRID_HEIGHT);
    SqHelper::setPanel(this, "res/vocal_animator_panel.svg");
    /**
     *  LEDs and LFO outputs
     */

    const float lfoBlockY = 38;     // was 22. move down to make space

    const float ledX = width - 46;
    const float ledY = lfoBlockY + 7.5;
    const float ledSpacingY = 30;

    const float lfoOutY = lfoBlockY;
    const float lfoOutX = width - 30;

    const float lfoInputX = 24;
    const float lfoInputY = lfoBlockY + 0;
    const float lfoTrimX = 68;
    const float lfoTrimY = lfoInputY + 3;

    const float lfoRateKnobX = 100;
    const float lfoRateKnobY = lfoBlockY + 24;

   // addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(
   //     Vec(ledX, ledY), module, module->animator.LFO0_LIGHT));
    addChild(createLight<MediumLight<GreenLight>>(
        Vec(ledX, ledY),
        module,
        Comp::LFO0_LIGHT));

    addChild(createLight<MediumLight<GreenLight>>(
        Vec(ledX, ledY + ledSpacingY),
        module,
        Comp::LFO1_LIGHT));

    addChild(createLight<MediumLight<GreenLight>>(
        Vec(ledX, ledY + 2 * ledSpacingY),
        module,
        Comp::LFO2_LIGHT));

    addOutput(createOutput<PJ301MPort>(
        Vec(lfoOutX, lfoOutY),
        module,
        Comp::LFO0_OUTPUT));

    addOutput(createOutput<PJ301MPort>(
        Vec(lfoOutX, lfoOutY + 1 * ledSpacingY),
        module,
        Comp::LFO1_OUTPUT));

    addOutput(createOutput<PJ301MPort>(
        Vec(lfoOutX, lfoOutY + 2 * ledSpacingY),
        module,
        Comp::LFO2_OUTPUT));

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(lfoRateKnobX, lfoRateKnobY),
        module,
        Comp::LFO_RATE_PARAM));

    addInput(createInput<PJ301MPort>(
        Vec(lfoInputX, lfoInputY),
        module,
        Comp::LFO_RATE_CV_INPUT));

    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(lfoTrimX, lfoTrimY),
        module,
        Comp::LFO_RATE_TRIM_PARAM));

    // the matrix switch
    addParam(SqHelper::createParam<NKK>(
        icomp,
        Vec(42, 65),
        module,
        Comp::LFO_MIX_PARAM));

     /**
      * Parameters and CV
      */
    const float mainBlockY = 140;
    const float mainBlockX = 20;

    const float colSpacingX = 64;

    const float knobX = mainBlockX + 0;
    const float knobY = mainBlockY + 24;

    const float trimX = mainBlockX + 11;
    const float trimY = mainBlockY + 78;

    const float inputX = mainBlockX + 8;
    const float inputY = mainBlockY + 108;

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(knobX, knobY),
        module,
        Comp::FILTER_FC_PARAM));

    addInput(createInput<PJ301MPort>(
        Vec(inputX, inputY),
        module,
        Comp::FILTER_FC_CV_INPUT));

    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(trimX, trimY),
        module,
        Comp::FILTER_FC_TRIM_PARAM));

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(knobX + colSpacingX, knobY),
        module,
        Comp::FILTER_Q_PARAM));

    addInput(createInput<PJ301MPort>(
        Vec(inputX + colSpacingX, inputY),
        module,
        Comp::FILTER_Q_CV_INPUT));

    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(trimX + colSpacingX, trimY),
        module,
        Comp::FILTER_Q_TRIM_PARAM));

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(knobX + 2 * colSpacingX, knobY),
        module,
        Comp::FILTER_MOD_DEPTH_PARAM));

    addInput(createInput<PJ301MPort>(
        Vec(inputX + 2 * colSpacingX, inputY),
        module,
        Comp::FILTER_MOD_DEPTH_CV_INPUT));

    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(trimX + 2 * colSpacingX, trimY),
        module,
        Comp::FILTER_MOD_DEPTH_TRIM_PARAM));

    const float row3 = 310;

    // I.O on row 3
    const float AudioInputX = inputX;
    const float outputX = inputX + 2 * colSpacingX;

    addInput(createInput<PJ301MPort>(
        Vec(AudioInputX, row3),
        module,
        Comp::AUDIO_INPUT));

    addOutput(createOutput<PJ301MPort>(
        Vec(outputX, row3),
        module,
        Comp::AUDIO_OUTPUT));

    const float bassX = inputX + colSpacingX - 4;
    const float bassY = row3 - 8;

     // the bass boost switch
#if 1 // get this working
    addParam(SqHelper::createParam<NKK2>(
        icomp,
        Vec(bassX, bassY),
        module,
        Comp::BASS_EXP_PARAM));
#endif

 /*************************************************
  *  screws
  */
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model *modelVocalModule = createModel<VocalModule, VocalWidget>("squinkylabs-vocalanimator");
#endif