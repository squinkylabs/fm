
#include <sstream>
#include "Squinky.hpp"
#include "WidgetComposite.h"

#ifdef _FILT
#include "DrawTimer.h"
#include "Filt.h"
#include "ctrl/SqHelper.h"
#include "ctrl/SqMenuItem.h"
#include "ctrl/SqWidgets.h"
#include "ctrl/PopupMenuParamWidget.h"

#ifdef _TIME_DRAWING
static DrawTimer drawTimer("Filt");
#endif

using Comp = Filt<WidgetComposite>;

/**
 */
struct FiltModule : Module
{
public:
    FiltModule();
    /**
     *
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;
    json_t *dataToJson() override;
    void dataFromJson(json_t *rootJ) override;

    std::shared_ptr<Comp> filt;
    bool poly;

private:

};

void FiltModule::onSampleRateChange()
{
}

FiltModule::FiltModule()
{
    configBypass(Comp::L_AUDIO_INPUT, Comp::L_AUDIO_OUTPUT);
    configBypass(Comp::R_AUDIO_INPUT, Comp::R_AUDIO_OUTPUT);
    config(Comp::NUM_PARAMS, Comp::NUM_INPUTS, Comp::NUM_OUTPUTS, Comp::NUM_LIGHTS);
    configInput(Comp::CV_INPUT1,"Cutoff frequency");
    configInput(Comp::CV_INPUT2,"Cutoff frequency");
    configInput(Comp::Q_INPUT,"Filter resonance");
    configInput(Comp::DRIVE_INPUT,"Drive");
    configInput(Comp::SLOPE_INPUT,"Slope");
    configInput(Comp::EDGE_INPUT,"Edge");
    configInput(Comp::L_AUDIO_INPUT,"Left Audio");
    configInput(Comp::R_AUDIO_INPUT,"Right Audio");
    configOutput(Comp::L_AUDIO_OUTPUT,"Left Audio");
    configOutput(Comp::R_AUDIO_OUTPUT,"Right Audio");

    filt = std::make_shared<Comp>(this);
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    SqHelper::setupParams(icomp, this); 
    onSampleRateChange();
    filt->init();
    poly=false;
}

void FiltModule::step()
{
    filt->setPoly(poly);
    filt->step();
}

////////////////////
// module widget
////////////////////

struct FiltWidget : ModuleWidget
{
    FiltWidget(FiltModule *);
 
    Label* addLabel(const Vec& v, const char* str, const NVGcolor& color = SqHelper::COLOR_BLACK)
    {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
        return label;
    }
    
    FiltModule* module;

    void appendContextMenu(Menu* theMenu) override;
    void addParams(FiltModule *module, std::shared_ptr<IComposite> icomp);
    void addTrimmers(FiltModule *module, std::shared_ptr<IComposite> icomp);
    void addJacks(FiltModule *module, std::shared_ptr<IComposite> icomp);

#ifdef _TIME_DRAWING
    // Filt: avg = 160.360240, stddev = 32.932833 (us) Quota frac=0.962161
    void draw(const DrawArgs &args) override
    {
        DrawLocker l(drawTimer);
        ModuleWidget::draw(args);
    }
#endif
};


/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */
#ifdef __V1x
FiltWidget::FiltWidget(FiltModule *module) : module(module)
{
    setModule(module);
#else
FiltWidget::FiltWidget(FiltModule *module) : ModuleWidget(module)
{
#endif
    box.size = Vec(14 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    SqHelper::setPanel(this, "res/filter_panel.svg");
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    addParams(module, icomp);
    addTrimmers(module, icomp);
    addJacks(module, icomp);

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild( createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

static const float jacksX1 = 30;
static const float deltaXJack = 38;

void FiltWidget::addParams(FiltModule *module, std::shared_ptr<IComposite> icomp)
{
    const float deltaX = 46;        // was 50, 40 too close
    const float x1 = 30;
    const float x2 = x1 + deltaX;
    const float x3 = x2 + deltaX;
    const float x4 = x3 + deltaX;

    const float y1 = 80;
    const float y2 = 142;
    const float y3 = 186;
    const float yPole1 = y2 - 12;
    const float yVol1 = y1 - 12;
    const float dyPoles = 8;
    const float xLED = x4 + 26;
  
    const float labelDx = 22;
    const float labelY = -38;
   
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x1, y1),
        module,
        Comp::FC_PARAM));
    addLabel(
        Vec(x1+8-labelDx, y1 + labelY),
        "Fc");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x2, y1),
        module,
        Comp::Q_PARAM));
     addLabel(
        Vec(x2+10-labelDx, y1 + labelY),
        "Q");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x3, y1),
        module,
        Comp::DRIVE_PARAM));
     addLabel(
        Vec(x3+1-labelDx, y1 + labelY),
        "Drive");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x4, y1),
        module,
        Comp::MASTER_VOLUME_PARAM));
    addLabel(
        Vec(x4-labelDx+5, y1 + labelY),
        "Vol");

    for (int i=0; i<4; ++i) {

        switch (i) {
            case 0:
            case 1:
                addChild(createLightCentered<SmallLight<GreenLight>>(
                    Vec(xLED, yVol1 + dyPoles * (3 - i) ),
                    module,
                    Comp::VOL0_LIGHT + i));
                break;
            case 2:
                addChild(createLightCentered<SmallLight<YellowLight>>(
                    Vec(xLED, yVol1 + dyPoles * (3 - i) ),
                    module,
                    Comp::VOL0_LIGHT + i));
                break;
            case 3:
                addChild(createLightCentered<SmallLight<RedLight>>(
                    Vec(xLED, yVol1 + dyPoles * (3 - i) ),
                    module,
                    Comp::VOL0_LIGHT + i));
                break;
        }
    }

    // second row
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x1, y2),
        module,
        Comp::EDGE_PARAM));
    addLabel(
        Vec(x1+2-labelDx, y2 + labelY),
        "Edge");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x2, y2),
        module,
        Comp::SPREAD_PARAM));
    addLabel(
        Vec(x2-labelDx, y2 + labelY),
        "Caps");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x3, y2),
        module,
        Comp::BASS_MAKEUP_PARAM));
    addLabel(
        Vec(x3+2-labelDx, y2 + labelY),
        "Bass");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(x4, y2),
        module,
        Comp::SLOPE_PARAM));
    addLabel(
        Vec(x4+2-labelDx, y2 + labelY),
        "Slope");

    for (int i=0; i<4; ++i) {
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(xLED, yPole1 + dyPoles * (3 - i)),
            module,
            Comp::SLOPE0_LIGHT + i));
    }
    
    // Third row

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(jacksX1, y3+10),
        module,
        Comp::EDGE_TRIM_PARAM));

    PopupMenuParamWidget* p = SqHelper::createParam<PopupMenuParamWidget>(
        icomp,
        Vec(43, y3),
        module,
        Comp::TYPE_PARAM);
    p->box.size.x = 76;    // width
    p->box.size.y = 22;     // should set auto like button does
    p->text = "4P LP";
    p->setLabels(Comp::getTypeNames());
    addParam(p);
 
    p = SqHelper::createParam<PopupMenuParamWidget>(
        icomp,
        Vec(123, y3),
        module,
        Comp::VOICING_PARAM);
    p->box.size.x = 80;    // width
    p->box.size.y = 22;     // should set auto like button does
    p->text = "Transistor";
    p->setLabels(Comp::getVoicingNames());
    addParam(p);
 } 

void FiltWidget::addTrimmers(FiltModule *module, std::shared_ptr<IComposite> icomp)
 {
    const float yTrim = 240;
    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(jacksX1 + 0 * deltaXJack, yTrim),
        module,
        Comp::FC1_TRIM_PARAM));

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(jacksX1 + 1 * deltaXJack, yTrim),
        module,
        Comp::FC2_TRIM_PARAM));

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(jacksX1 + 2 * deltaXJack, yTrim),
        module,
        Comp::Q_TRIM_PARAM));

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(jacksX1 + 3 * deltaXJack, yTrim),
        module,
        Comp::DRIVE_TRIM_PARAM));
    
    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(jacksX1 + 4 * deltaXJack, yTrim),
        module,
        Comp::SLOPE_TRIM_PARAM));
 }

void FiltWidget::addJacks(FiltModule *module, std::shared_ptr<IComposite> icomp)
 {
    const float x1 = jacksX1;
    const float yJacks1 = 286-2;
    const float yJacks2 = 330+2;
   
    const float JackLabelY = -31;

    // row 1
    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 0 * deltaXJack, yJacks1),
        module,
        Comp::CV_INPUT1));
    addLabel(
        Vec(x1 + 0 * deltaXJack -18, yJacks1 + JackLabelY),
        "CV1");

    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 1 * deltaXJack, yJacks1),
        module,
        Comp::CV_INPUT2));
    addLabel(
        Vec(x1 + 1 * deltaXJack -18, yJacks1 + JackLabelY),
        "CV2");

    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 2 * deltaXJack, yJacks1),
        module,
        Comp::Q_INPUT));
    addLabel(
        Vec(x1 + 2 * deltaXJack -12, yJacks1 + JackLabelY),
        "Q");
    
    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 3 * deltaXJack, yJacks1),
        module,
        Comp::DRIVE_INPUT));
    addLabel(
        Vec(x1 + 3 * deltaXJack - 22, yJacks1 + JackLabelY),
        "Drive");

    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 4 * deltaXJack, yJacks1),
        module,
        Comp::SLOPE_INPUT));
    addLabel(
        Vec(x1 + 4 * deltaXJack - 21, yJacks1 + JackLabelY),
        "Slope");

    // row2
     addInput(createInputCentered<PJ301MPort>(
        Vec(x1, yJacks2),
        module,
        Comp::EDGE_INPUT));
    addLabel(
        Vec(x1-21, yJacks2 + JackLabelY),
        "Edge");

    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 1 * deltaXJack, yJacks2),
        module,
        Comp::L_AUDIO_INPUT));
    addLabel(
        Vec(x1 + 1 * deltaXJack -18, yJacks2 + JackLabelY),
        "In L");

    addInput(createInputCentered<PJ301MPort>(
        Vec(x1 + 2 * deltaXJack, yJacks2),
        module,
        Comp::R_AUDIO_INPUT));
    addLabel(
        Vec(x1 + 2 * deltaXJack -18, yJacks2 + JackLabelY),
        "In R");
  
    addOutput(createOutputCentered<PJ301MPort>(
        Vec(x1 + 3 * deltaXJack, yJacks2),
        module,
        Comp::L_AUDIO_OUTPUT));
    addLabel(
        Vec(x1 - 6 + 3 * deltaXJack -18, yJacks2 + JackLabelY),
        "Out L",
        SqHelper::COLOR_WHITE);
    addOutput(createOutputCentered<PJ301MPort>(
        Vec(x1 + 4 * deltaXJack, yJacks2),
        module,
        Comp::R_AUDIO_OUTPUT));
    addLabel(
        Vec(x1 - 6 + 4 * deltaXJack -18, yJacks2 + JackLabelY),
        "Out R",
        SqHelper::COLOR_WHITE);
}

json_t *FiltModule::dataToJson()
{
    json_t *rootJ = json_object();
    const bool up = poly;
    json_object_set_new(rootJ, "Polyphonic", json_boolean(up));
    return rootJ;
}

void FiltModule::dataFromJson(json_t *rootJ)
{
    json_t *driverJ = json_object_get(rootJ, "Polyphonic");
    if (driverJ) {
        const bool up = json_boolean_value(driverJ);
        if (up)
            poly = up;
    }
}

void FiltWidget::appendContextMenu(Menu* theMenu) 
{
    theMenu->addChild(new MenuSeparator());
    theMenu->addChild(createBoolPtrMenuItem("Stereo Polyphonic", "", &module->poly));

}


#ifdef __V1x
Model *modelFiltModule = createModel<FiltModule, FiltWidget>("squinkylabs-filt");
#else

Model *modelFiltModule = Model::create<FiltModule,
    FiltWidget>("Squinky Labs",
    "squinkylabs-filt",
    "Stairway: Ladder Filter", FILTER_TAG, DISTORTION_TAG, DUAL_TAG);
#endif
#endif

