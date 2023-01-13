
#include <sstream>
#include "Squinky.hpp"

#ifdef _FUN
#include "WidgetComposite.h"
#include "ctrl/SqWidgets.h"
#include "ctrl/SqHelper.h"
#include "ctrl/SqMenuItem.h"
#include "FunVCOComposite.h"

using Comp = FunVCOComposite<WidgetComposite>;

/**
 * Two position NKK
 * in V0.6 didn't need this - it just worked
 */

struct NKK2 : app::SvgSwitch {
	NKK2() {
        // add all up and all down image, no middle
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/NKK_0.svg")));
		addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/NKK_2.svg")));
	}
};

/**
 */
struct FunVModule : Module
{
public:
    FunVModule();
    /**
     *
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;

    Comp vco;
private:
};

void FunVModule::onSampleRateChange()
{
    float rate = SqHelper::engineGetSampleRate();
    vco.setSampleRate(rate);
}

#ifdef __V1x
FunVModule::FunVModule() : vco(this)
{
    // Set the number of components
    config(vco.NUM_PARAMS, vco.NUM_INPUTS, vco.NUM_OUTPUTS, vco.NUM_LIGHTS);
    configInput(vco.PITCH_INPUT,"1V/oct pitch");
    configInput(vco.FM_INPUT,"Frequency modulation");
    configInput(vco.SYNC_INPUT,"Sync");
    configInput(vco.PW_INPUT,"Pulse width modulation");
    configOutput(vco.SIN_OUTPUT,"Sine");
    configOutput(vco.TRI_OUTPUT,"Triangle");
    configOutput(vco.SAW_OUTPUT,"Saw");
    configOutput(vco.SQR_OUTPUT,"Square");
 
    onSampleRateChange();
    std::shared_ptr<IComposite> icomp = FunVCOComposite<WidgetComposite>::getDescription();
    SqHelper::setupParams(icomp, this);
}
#else
FunVModule::FunVModule()
    : Module(vco.NUM_PARAMS,
    vco.NUM_INPUTS,
    vco.NUM_OUTPUTS,
    vco.NUM_LIGHTS),
    vco(this)
{
    onSampleRateChange();
}
#endif

void FunVModule::step()
{
    vco.step();
}

////////////////////
// module widget
////////////////////

struct FunVWidget : ModuleWidget
{
    FunVWidget(FunVModule *);
    std::shared_ptr<IComposite> icomp = Comp::getDescription();


    void addTop3(FunVModule *, float verticalShift);
    void addMiddle4(FunVModule *, float verticalShift);
    void addJacks(FunVModule *, float verticalShift);

     Label* addLabel(const Vec& v, const char* str, const NVGcolor& color)
    {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
        return label;
    }
    Label* addLabel(const Vec& v, const char* str) 
    {
        return addLabel(v, str, SqHelper::COLOR_BLACK);
    }
};

void FunVWidget::addTop3(FunVModule * module, float verticalShift)
{
    const float left = 8;
    const float right = 112;
    const float center = 49;

    addParam(SqHelper::createParam<NKK2>(
        icomp,
        Vec(left, 66 + verticalShift),
        module,
        Comp::MODE_PARAM));
    addLabel(Vec(left -4, 48+ verticalShift), "anlg");
    addLabel(Vec(left -3, 108+ verticalShift), "dgtl");

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(center, 61 + verticalShift),
        module, 
        Comp::FREQ_PARAM));
    auto label = addLabel(Vec(center + 3, 40+ verticalShift), "pitch");
    label->fontSize = 16;

    addParam(SqHelper::createParam<NKK2>(
        icomp,
        Vec(right, 66 + verticalShift),
        module,
        Comp::SYNC_PARAM));
    addLabel(Vec(right-5, 48+ verticalShift), "hard");
    addLabel(Vec(right-2, 108+ verticalShift), "soft");
}

void FunVWidget::addMiddle4(FunVModule * module, float verticalShift)
{
    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(23, 143 + verticalShift),
        module, Comp::FINE_PARAM));
    addLabel(Vec(25, 124 +verticalShift), "fine");

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(91, 143 + verticalShift),
        module, Comp::PW_PARAM));
    addLabel(Vec(84, 124 +verticalShift), "p width");

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(23, 208 + verticalShift),
        module,
        Comp::FM_PARAM));
    addLabel(Vec(19, 188 +verticalShift), "fm cv");

    addParam(SqHelper::createParam<RoganSLBlue40>(
        icomp,
        Vec(91, 208 + verticalShift),
        module, 
        Comp::PWM_PARAM
    ));
    addLabel(Vec(82, 188 +verticalShift), "pwm cv");
}

void FunVWidget::addJacks(FunVModule * module, float verticalShift)
{
    const float col1 = 12;
    const float col2 = 46;
    const float col3 = 81;
    const float col4 = 115;
    const float outputLabelY = 300;

    // this is the v1 format
    addInput(createInput<PJ301MPort>(
        Vec(col1, 273+verticalShift),
        module,
        module->vco.PITCH_INPUT));
    addLabel(Vec(10, 255+verticalShift), "cv");

    addInput(createInput<PJ301MPort>(
        Vec(col2, 273+verticalShift),
        module,
        module->vco.FM_INPUT));
    addLabel(Vec(43, 255+verticalShift), "fm");

    addInput(createInput<PJ301MPort>(
        Vec(col3, 273+verticalShift),
        module,
        module->vco.SYNC_INPUT));
    addLabel(Vec(73, 255+verticalShift), "sync");

    addInput(createInput<PJ301MPort>(
        Vec(col4, 273+verticalShift),
        module,
        module->vco.PW_INPUT));
    addLabel(Vec(106, 255+verticalShift), "pwm");

    addOutput(createOutput<PJ301MPort>(
        Vec(col1, 317+verticalShift),
        module,
        module->vco.SIN_OUTPUT));
    addLabel(Vec(8, outputLabelY+verticalShift), "sin", SqHelper::COLOR_WHITE);

    addOutput(createOutput<PJ301MPort>(
        Vec(col2, 317+verticalShift),
        module,
        module->vco.TRI_OUTPUT));
    addLabel(Vec(44, outputLabelY+verticalShift), "tri", SqHelper::COLOR_WHITE);

    addOutput(createOutput<PJ301MPort>(
        Vec(col3, 317+verticalShift),
        module,
        module->vco.SAW_OUTPUT));
    addLabel(Vec(75, outputLabelY+verticalShift),
        "saw",
        SqHelper::COLOR_WHITE);

    addOutput(createOutput<PJ301MPort>(
        Vec(col4, 317+verticalShift),
        module,
        module->vco.SQR_OUTPUT));
 
    addLabel(Vec(111, outputLabelY+verticalShift), "sqr", SqHelper::COLOR_WHITE);
}

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */
#ifdef __V1x
FunVWidget::FunVWidget(FunVModule *module)
{
    setModule(module);
#else
FunVWidget::FunVWidget(FunVModule *module) : ModuleWidget(module)
{
#endif
    box.size = Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    SqHelper::setPanel(this, "res/fun_panel.svg");

    addTop3(module, 0);
    addMiddle4(module, 0);
    addJacks(module, 0);

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
#ifndef __V1x
Model *modelFunVModule = createModel<FunVModule,
    FunVWidget>("Squinky Labs",
    "squinkylabs-funv",
    "Functional VCO-1", OSCILLATOR_TAG);
#else
Model *modelFunVModule = createModel<FunVModule, FunVWidget>(
    "squinkylabs-funv");
#endif


#endif

