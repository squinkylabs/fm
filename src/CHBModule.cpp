#include "ctrl/SemitoneDisplay.h"
#include "Squinky.hpp"

#ifdef _CHB
#include "IComposite.h"
#include "ctrl/SqWidgets.h"
#include "ctrl/SqMenuItem.h"
#include "DrawTimer.h"
#include "WidgetComposite.h"
#include <sstream>

#include "CHB.h"
#include "IMWidgets.hpp"

#ifdef _TIME_DRAWING
static DrawTimer drawTimer("Cheby");
#endif

using Comp = CHB<WidgetComposite>;

/**
 */
struct CHBModule : Module
{
public:
    CHBModule();

    /**
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;

    CHB<WidgetComposite> chb;
private:
};

CHBModule::CHBModule() : chb(this)
{
    config(Comp::NUM_PARAMS, Comp::NUM_INPUTS, Comp::NUM_OUTPUTS, Comp::NUM_LIGHTS);
    for (int i = 0; i < 10; ++i) {
        configInput(chb.H0_INPUT + i,"Harmonic " + std::to_string(i+1));
    }
    // top row
    configInput(CHB<WidgetComposite>::CV_INPUT,"1V/oct");
    configInput(CHB<WidgetComposite>::PITCH_MOD_INPUT,"Modulation depth");
    configInput(CHB<WidgetComposite>::LINEAR_FM_INPUT,"Linear frequency modulation");
    configInput(CHB<WidgetComposite>::EVEN_INPUT,"Even level control");
    configInput(CHB<WidgetComposite>::SLOPE_INPUT,"Slope");
    configInput(CHB<WidgetComposite>::ODD_INPUT,"Odd level control");
    //bottom row
    configInput(CHB<WidgetComposite>::AUDIO_INPUT,"Audio");
    configInput(CHB<WidgetComposite>::GAIN_INPUT,"Gain");
    configInput(CHB<WidgetComposite>::ENV_INPUT,"Envelope generator");
    configInput(CHB<WidgetComposite>::RISE_INPUT,"Rise");
    configInput(CHB<WidgetComposite>::FALL_INPUT,"Fall");
    configOutput(CHB<WidgetComposite>::MIX_OUTPUT,"Mix"); 

    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    SqHelper::setupParams(icomp, this);
}

void CHBModule::step()
{
    chb.step();
}

void CHBModule::onSampleRateChange()
{
    chb.onSampleRateChange();
}

////////////////////
// module widget
////////////////////

struct CHBWidget : ModuleWidget
{
    friend struct CHBEconomyItem;
    CHBWidget(CHBModule *);

    /**
     * Helper to add a text label to this widget
     */
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
        ModuleWidget::step();
        semitoneDisplay.step();
    }

#ifdef _TIME_DRAWING
    //Cheby: avg = 114.110832, stddev = 37.485271 (us) Quota frac=0.684665
    void draw(const DrawArgs &args) override
    {
        DrawLocker l(drawTimer);
        ModuleWidget::draw(args);
    }
#endif

private:
    void addHarmonics(CHBModule *module, std::shared_ptr<IComposite>);
    void addRow1(CHBModule *module, std::shared_ptr<IComposite>);
    void addRow2(CHBModule *module, std::shared_ptr<IComposite>);
    void addRow3(CHBModule *module, std::shared_ptr<IComposite>);
    void addRow4(CHBModule *module, std::shared_ptr<IComposite>);

    void addBottomJacks(CHBModule *module);
    void resetMe(CHBModule *module);

    // TODO: still used?
    // This is the gain which when run throught all the lookup tables
    // gives a gain of 1.
    const float defaultGainParam = .63108f;

    int numHarmonics = 10;
    CHBModule* const module;
    std::vector<ParamWidget* > harmonicParams;
    std::vector<float> harmonicParamMemory;
    ParamWidget* gainParam = nullptr;

    SemitoneDisplay semitoneDisplay;
};

/**
 * Global coordinate constants
 */
const float colHarmonicsJacks = 21;
const float rowFirstHarmonicJackY = 47;
const float harmonicJackSpacing = 32;
const float harmonicTrimDeltax = 27.5;

// columns of knobs
const float col1 = 95;
const float col2 = 150;
const float col3 = 214;
const float col4 = 268;

// rows of knobs
const float row1 = 75;
const float row2 = 131;
const float row3 = 201;
const float row4 = 237;
const float row5 = 287;
const float row6 = 332;

const float labelAboveKnob = 33;
const float labelAboveJack = 30;

inline void CHBWidget::addHarmonics(CHBModule *module, std::shared_ptr<IComposite> icomp)
{
    for (int i = 0; i < numHarmonics; ++i) {
        const float row = rowFirstHarmonicJackY + i * harmonicJackSpacing;
        addInput(createInputCentered<PJ301MPort>(
            Vec(colHarmonicsJacks, row),
            module,
            module->chb.H0_INPUT + i));

       // const float defaultValue = (i == 0) ? 1 : 0;
        auto p = SqHelper::createParamCentered<Trimpot>(
            icomp,
            Vec(colHarmonicsJacks + harmonicTrimDeltax, row),
            module,
            Comp::PARAM_H0 + i);
        addParam(p);
        harmonicParams.push_back(p);
    }
}

void CHBWidget::addRow1(CHBModule *module, std::shared_ptr<IComposite> icomp)
{
    const float row = row1;
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col1, row),
        module,
        CHB<WidgetComposite>::PARAM_RISE));
    addLabel(Vec(col1 - 20, row - labelAboveKnob), "Rise");

    Blue30SnapKnob* p = SqHelper::createParamCentered<Blue30SnapKnob>(
        icomp,
        Vec(col2, row),
        module,
        CHB<WidgetComposite>::PARAM_OCTAVE);

    p->snap = true;
	p->smooth = false;
    addParam(p);

    semitoneDisplay.setOctLabel(
        addLabel(Vec(col2 - 22, row1 - labelAboveKnob), "Octave"),
        CHB<WidgetComposite>::PARAM_OCTAVE);

    addParam(SqHelper::createParamCentered<Blue30SnapKnob>(
        icomp,
        Vec(col3, row),
        module,
        CHB<WidgetComposite>::PARAM_SEMIS));

    semitoneDisplay.setSemiLabel(
        addLabel(Vec(col3 - 26, row - labelAboveKnob), "Semi"),
        CHB<WidgetComposite>::PARAM_SEMIS);

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col4, row1),
        module,
        CHB<WidgetComposite>::PARAM_TUNE));
    addLabel(Vec(col4 - 22, row1 - labelAboveKnob), "Tune");
}

void CHBWidget::addRow2(CHBModule *module, std::shared_ptr<IComposite> icomp)
{
    const float row = row2;

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col1, row),
        module,
        CHB<WidgetComposite>::PARAM_FALL));
    addLabel(Vec(col1 - 18, row - labelAboveKnob), "Fall");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col3, row),
        module,
        CHB<WidgetComposite>::PARAM_PITCH_MOD_TRIM));
    addLabel(Vec(col3 - 20, row - labelAboveKnob), "Mod");

    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col4, row),
        module,
        CHB<WidgetComposite>::PARAM_LINEAR_FM_TRIM));
    addLabel(Vec(col4 - 20, row - labelAboveKnob), "LFM");

    addParam(SqHelper::createParamCentered<CKSS>(
        icomp,
        Vec(col2, row),
        module,
        CHB<WidgetComposite>::PARAM_FOLD));
    auto l = addLabel(Vec(col2 - 18, row - 30), "Fold");
    l->fontSize = 11;
    l = addLabel(Vec(col2 - 17, row + 10), "Clip");
    l->fontSize = 11;

    addChild(createLightCentered<SmallLight<GreenRedLight>>(
        Vec(col2 - 16, row),
        module,
        CHB<WidgetComposite>::GAIN_GREEN_LIGHT));
}

void CHBWidget::addRow3(CHBModule *module, std::shared_ptr<IComposite> icomp)
{
    const float row = row3;

    gainParam = SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col1, row),
        module,
        CHB<WidgetComposite>::PARAM_EXTGAIN);
    addParam(gainParam);
    addLabel(Vec(col1 - 21, row - labelAboveKnob), "Gain");

    //even
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col2, row),
        module,
        CHB<WidgetComposite>::PARAM_MAG_EVEN));
    addLabel(Vec(col2 - 21.5, row - labelAboveKnob), "Even");

    // slope
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col3, row),
        module,
        CHB<WidgetComposite>::PARAM_SLOPE));
    addLabel(Vec(col3 - 23, row - labelAboveKnob), "Slope");

    //odd
    addParam(SqHelper::createParamCentered<RoganSLBlue30>(
        icomp,
        Vec(col4, row),
        module,
        CHB<WidgetComposite>::PARAM_MAG_ODD));
    addLabel(Vec(col4 - 19, row - labelAboveKnob), "Odd");

}

void CHBWidget::addRow4(CHBModule *module, std::shared_ptr<IComposite> icomp)
{
    float row = row4;

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(col1, row),
        module,
        CHB<WidgetComposite>::PARAM_EXTGAIN_TRIM));

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(col2, row),
        module,
        CHB<WidgetComposite>::PARAM_EVEN_TRIM));

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(col3, row),
        module,
        CHB<WidgetComposite>::PARAM_SLOPE_TRIM));

    addParam(SqHelper::createParamCentered<Trimpot>(
        icomp,
        Vec(col4, row),
        module,
        CHB<WidgetComposite>::PARAM_ODD_TRIM));
}

static const char* labels[] = {
    "V/Oct",
    "Mod",
    "LFM",
    "Even",
    "Slope",
    "Odd",
    "Ext In",
    "Gain",
    "EG",
    "Rise",
    "Fall",
    "Out",
    nullptr,
};
static const int offsets[] = {
    -1,
    2,
    2,
    0,
    0,      // slope
    2,      // odd
    -2,     // ext gain
    1,          // gain
    5,
    2,          // rise
    4,
    3
};

static const int ids[] = {
    // top row
    CHB<WidgetComposite>::CV_INPUT,
    CHB<WidgetComposite>::PITCH_MOD_INPUT,
    CHB<WidgetComposite>::LINEAR_FM_INPUT,
    CHB<WidgetComposite>::EVEN_INPUT,
    CHB<WidgetComposite>::SLOPE_INPUT,
    CHB<WidgetComposite>::ODD_INPUT,
    //bottom row
    CHB<WidgetComposite>::AUDIO_INPUT,
    CHB<WidgetComposite>::GAIN_INPUT,
    CHB<WidgetComposite>::ENV_INPUT,
    CHB<WidgetComposite>::RISE_INPUT,
    CHB<WidgetComposite>::FALL_INPUT,
    CHB<WidgetComposite>::MIX_OUTPUT
};

void CHBWidget::addBottomJacks(CHBModule *module)
{
    const float jackCol1 = 93;
    const int numCol = 6;
    const float deltaX = 36;
    for (int jackRow = 0; jackRow < 2; ++jackRow) {
        for (int jackCol = 0; jackCol < numCol; ++jackCol) {
            const Vec pos(jackCol1 + deltaX * jackCol,
                jackRow == 0 ? row5 : row6);
            const int index = jackCol + numCol * jackRow;

            auto color = SqHelper::COLOR_BLACK;
            if (index == 11) {
                color = SqHelper::COLOR_WHITE;
            }

            const int id = ids[index];
            if (index == 11) {
                addOutput(createOutputCentered<PJ301MPort>(
                    pos,
                    module,
                    id));
            } else {
                addInput(createInputCentered<PJ301MPort>(
                    pos,
                    module,
                    id));
            }

            auto l = addLabel(Vec(pos.x - 20 + offsets[index], pos.y - labelAboveJack),
                labels[index],
                color);
            l->fontSize = 11;
        }
    }
}

void CHBWidget::resetMe(CHBModule *module)
{
    bool isOnlyFundamental = true;
    bool isAll = true;
    bool havePreset = !harmonicParamMemory.empty();
//    const float val0 = harmonicParams[0]->value;
    const float val0 = SqHelper::getValue(harmonicParams[0]);
    if (val0 < .99) {
        isOnlyFundamental = false;
        isAll = false;
    }

    for (int i = 1; i < numHarmonics; ++i) {
        const float value = SqHelper::getValue(harmonicParams[i]);
        if (value < .9) {
            isAll = false;
        }

        if (value > .1) {
            isOnlyFundamental = false;
        }
    }

    if (!isOnlyFundamental && !isAll) {
        // take snapshot
        if (harmonicParamMemory.empty()) {
            harmonicParamMemory.resize(numHarmonics);
        }
        for (int i = 0; i < numHarmonics; ++i) {
            harmonicParamMemory[i] = SqHelper::getValue(harmonicParams[i]);
        }
    }

    // fundamental -> all
    if (isOnlyFundamental) {
        for (int i = 0; i < numHarmonics; ++i) {
            //harmonicParams[i]->setValue(1);
            SqHelper::setValue(harmonicParams[i], 1.f);
        }
    }
    // all -> preset, if any
    else if (isAll && havePreset) {
        for (int i = 0; i < numHarmonics; ++i) {
            SqHelper::setValue(harmonicParams[i], harmonicParamMemory[i]);
        }
    }
    // preset -> fund. if no preset all -> fund
    else {
        for (int i = 0; i < numHarmonics; ++i) {
            SqHelper::setValue(harmonicParams[i], (i == 0) ? 1 : 0);
        }
    }

    SqHelper::setValue(gainParam, defaultGainParam);
}

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */

CHBWidget::CHBWidget(CHBModule *module) :
  //  numHarmonics(module->chb.numHarmonics),
    module(module),
    semitoneDisplay(module)
{
    if (module) {
        numHarmonics = module->chb.numHarmonics;
    }
    setModule(module);

    box.size = Vec(20 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    {
        SqHelper::setPanel(this, "res/chb_panel.svg");
        auto border = new PanelBorderWidget();
        border->box = box;
        addChild(border);
    }

    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    addHarmonics(module, icomp);
    addRow1(module, icomp);
    addRow2(module, icomp);
    addRow3(module, icomp);
    addRow4(module, icomp);

    auto sw = new SQPush(
        "res/preset-button-up.svg",
        "res/preset-button-down.svg");
    Vec pos(64, 360);
    sw->center(pos);
    sw->onClick([this, module]() {
        this->resetMe(module);
    });

    addChild(sw);
    addBottomJacks(module);

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model *modelCHBModule = createModel<CHBModule, CHBWidget>(
    "squinkylabs-CHB2");
#endif