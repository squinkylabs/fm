#include "Squinky.hpp"

#ifdef _SUPER
#include "WidgetComposite.h"
#include "ctrl/SqWidgets.h"
#include "ctrl/SqMenuItem.h"
#include "Super.h"
#include "ctrl/ToggleButton.h"
#include "ctrl/SemitoneDisplay.h"
#include "DrawTimer.h"
#include "IMWidgets.hpp"

#include <sstream>

#ifdef _TIME_DRAWING
static DrawTimer drawTimer("Saws");
#endif

using Comp = Super<WidgetComposite>;

/**
 */
struct SuperModule : Module
{
public:
    SuperModule();
    /**
     *
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;

    std::shared_ptr<Comp> super;
};

void SuperModule::onSampleRateChange()
{
}

SuperModule::SuperModule()
{
    config(Comp::NUM_PARAMS, Comp::NUM_INPUTS, Comp::NUM_OUTPUTS, Comp::NUM_LIGHTS);
    configInput(Super<WidgetComposite>::DETUNE_INPUT,"Detune");
    configInput(Super<WidgetComposite>::MIX_INPUT,"Mix");
    configInput(Super<WidgetComposite>::CV_INPUT,"1V/oct");
    configInput(Super<WidgetComposite>::TRIGGER_INPUT,"Trigger");
    configInput(Super<WidgetComposite>::FM_INPUT,"Frequency modulation");
    configOutput(Super<WidgetComposite>::MAIN_OUTPUT_LEFT,"Main left");
    configOutput(Super<WidgetComposite>::MAIN_OUTPUT_RIGHT,"Main right");

    paramQuantities[Comp::OCTAVE_PARAM]->snapEnabled = true;
    paramQuantities[Comp::OCTAVE_PARAM]->smoothEnabled = false;
    super = std::make_shared<Comp>(this);
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    SqHelper::setupParams(icomp, this);
    onSampleRateChange();
    super->init();
}

void SuperModule::step()
{
    super->step();
}

////////////////////
// module widget
////////////////////

struct superWidget : ModuleWidget
{
    superWidget(SuperModule *);

    Label* addLabel(const Vec& v, const char* str, const NVGcolor& color = SqHelper::COLOR_BLACK)
    {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
        return label;
    }
    void step() override
    {
        semitoneDisplay.step();
        ModuleWidget::step();
    }

    SuperModule* superModule = nullptr;
    void addPitchKnobs(SuperModule *, std::shared_ptr<IComposite>);
    void addOtherKnobs(SuperModule *, std::shared_ptr<IComposite>);
    void addJacks(SuperModule *);
    void appendContextMenu(Menu *menu) override;
 
    SemitoneDisplay semitoneDisplay;

#ifdef _TIME_DRAWING
    //Saws: avg = 793.744399, stddev = 271.946036 (us) Quota frac=4.762466
    //new switches:  Saws: avg = 27.301192, stddev = 6.996107 (us) Quota frac=0.163807
    // old, but only Saws: avg = 362.616217, stddev = 41.723176 (us) Quota frac=2.175697

    void draw(const DrawArgs &args) override
    {
        DrawLocker l(drawTimer);
        ModuleWidget::draw(args);
    }
#endif
};


void superWidget::appendContextMenu(Menu *menu)
{
    MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
    
    //   HARD_PAN_PARAM,
    //    ALTERNATE_PAN_PARAM,
   // MenuLabel *spacerLabel2 = new MenuLabel();
   // menu->addChild(spacerLabel2);

    SqMenuItem_BooleanParam2 * item = new SqMenuItem_BooleanParam2(superModule, Comp::HARD_PAN_PARAM);
    item->text = "Hard Pan";
    menu->addChild(item);
}

const float col1 = 40;
const float col2 = 110;

const float row1 = 71;
const float row2 = 134;
const float row3 = 220;
const float row4 = 250;

const float jackRow1 = 290+2;
const float jackRow2 = 332+2;

const float labelOffsetBig = -40;
const float labelOffsetSmall = -32;

void superWidget::addPitchKnobs(SuperModule* module, std::shared_ptr<IComposite> icomp)
{
    // Octave
    Blue30SnapKnob* oct = SqHelper::createParamCentered<Blue30SnapKnob>(
        icomp,
        Vec(col1, row1),
        module, 
        Comp::OCTAVE_PARAM);
    //oct->snap = true;
    //oct->smooth = false;
    addParam(oct);
    Label* l = addLabel(
        Vec(col1 - 23, row1 + labelOffsetBig),
        "Oct");
    semitoneDisplay.setOctLabel(l, Super<WidgetComposite>::OCTAVE_PARAM);

    // Semi
    Blue30SnapKnob* semi = SqHelper::createParamCentered<Blue30SnapKnob>(
        icomp,
        Vec(col2, row1),
        module,
        Super<WidgetComposite>::SEMI_PARAM);
    semi->snap = true;
    semi->smooth = false;
    addParam(semi);
    l = addLabel(
        Vec(col2 - 27, row1 + labelOffsetBig),
        "Semi");
    semitoneDisplay.setSemiLabel(l, Super<WidgetComposite>::SEMI_PARAM);

    // Fine
    addParam(SqHelper::createParamCentered<RoganSLBlue40>(
        icomp,
        Vec(col1, row2),
        module,
        Comp::FINE_PARAM));
    addLabel(
        Vec(col1 - 19,
        row2 + labelOffsetBig),
        "Fine");

    // FM
    addParam(SqHelper::createParamCentered<RoganSLBlue40>(
        icomp,
        Vec(col2, row2),
        module, 
        Comp::FM_PARAM));
    addLabel(
        Vec(col2 - 15, row2 + labelOffsetBig),
        "FM");
}

void superWidget::addOtherKnobs(SuperModule *, std::shared_ptr<IComposite> icomp)
{
    // Detune
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col1, row3), 
        module, 
        Comp::DETUNE_PARAM));
    addLabel(
        Vec(col1 - 27, row3 + labelOffsetSmall),
        "Detune");

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(col1, row4), 
        module, 
        Comp::DETUNE_TRIM_PARAM));

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col2, row3), 
        module, 
        Comp::MIX_PARAM));
    addLabel(
        Vec(col2 - 18, row3 + labelOffsetSmall),
        "Mix");
    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(col2, row4), 
        module, 
        Comp::MIX_TRIM_PARAM));
}

const float jackX = 27;
const float jackDx = 33;
const float jackOffsetLabel = -30;
const float jackLabelPoints = 11;

void superWidget::addJacks(SuperModule *)
{
    Label* l = nullptr;
    // first row
    addInput(createInputCentered<PJ301MPort>(
        Vec(jackX, jackRow1),
        module,
        Super<WidgetComposite>::DETUNE_INPUT));
    l = addLabel(
        Vec(jackX - 25, jackRow1 + jackOffsetLabel),
        "Detune");
    l->fontSize = jackLabelPoints;

    addInput(createInputCentered<PJ301MPort>(
        Vec(jackX + 2 * jackDx, jackRow1),
        module,
        Super<WidgetComposite>::MIX_INPUT));
    l = addLabel(
        Vec(jackX + 2 * jackDx - 15, jackRow1 + jackOffsetLabel),
        "Mix");
    l->fontSize = jackLabelPoints;

    // second row
    addInput(createInputCentered<PJ301MPort>(
        Vec(jackX, jackRow2),
        module,
        Super<WidgetComposite>::CV_INPUT));
    l = addLabel(
        Vec(jackX - 20, jackRow2 + jackOffsetLabel),
        "V/Oct");
    l->fontSize = jackLabelPoints;

    addInput(createInputCentered<PJ301MPort>(
        Vec(jackX + 1 * jackDx, jackRow2),
        module,
        Super<WidgetComposite>::TRIGGER_INPUT));
    l = addLabel(
        Vec(jackX + 1 * jackDx - 17, jackRow2 + jackOffsetLabel),
        "Trig");
    l->fontSize = jackLabelPoints;

    addInput(createInputCentered<PJ301MPort>(
        Vec(jackX + 2 * jackDx, jackRow2),
        module,
        Super<WidgetComposite>::FM_INPUT));
    l = addLabel(
        Vec(jackX + 2 * jackDx - 14, jackRow2 + jackOffsetLabel), "FM");
    l->fontSize = jackLabelPoints;

    addOutput(createOutputCentered<PJ301MPort>(
        Vec(jackX + 3 * jackDx, jackRow2),
        module,
        Super<WidgetComposite>::MAIN_OUTPUT_LEFT));
    l = addLabel(
        Vec(jackX + 3 * jackDx - 21, jackRow2 + jackOffsetLabel),
        "Out L", SqHelper::COLOR_WHITE);
    l->fontSize = jackLabelPoints;

     addOutput(createOutputCentered<PJ301MPort>(
        Vec(jackX + 3 * jackDx, jackRow1),
        module,
        Super<WidgetComposite>::MAIN_OUTPUT_RIGHT));
    l = addLabel(
        Vec(jackX + 3 * jackDx - 21, jackRow1 + jackOffsetLabel),
        "Out R", SqHelper::COLOR_WHITE);
    l->fontSize = jackLabelPoints;
}

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */

superWidget::superWidget(SuperModule *module) : semitoneDisplay(module)
{
    setModule(module);
    superModule = module;

    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    box.size = Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    SqHelper::setPanel(this, "res/super_panel.svg");

    // Is this really needed?
    auto border = new PanelBorderWidget();
    border->box = box;
    addChild(border);

    addPitchKnobs(module, icomp);
    addOtherKnobs(module, icomp);
    addJacks(module);

    // the "classic" switch
    #if 1
    ToggleButton* tog = SqHelper::createParamCentered<ToggleButton>(
        icomp,
        Vec(83, 164),
        module,
        Comp::CLEAN_PARAM);
    tog->addSvg("res/clean-switch-01.svg");
    tog->addSvg("res/clean-switch-02.svg");
    tog->addSvg("res/clean-switch-03.svg");
    #else
    SvgSwitch* tog =  SqHelper::createParam<::rack::app::SvgSwitch>(
        icomp,
        Vec(83, 164),
        module,
        Comp::CLEAN_PARAM);
    tog->fb->removeChild(tog->shadow);
    tog->addFrame(SqHelper::loadSvg("res/clean-switch-01.svg"));
    tog->addFrame(SqHelper::loadSvg("res/clean-switch-02.svg"));
    tog->addFrame(SqHelper::loadSvg("res/clean-switch-03.svg"));
    #endif
    addParam(tog);

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model *modelSuperModule = createModel<SuperModule,
    superWidget>("squinkylabs-super");

#endif

