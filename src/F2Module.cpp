
#include "Squinky.hpp"
#include "WidgetComposite.h"

#ifdef _F2
// #include "F2.h"
#include "F2_Poly.h"
#include "SqStream.h"
#include "ctrl/PopupMenuParamWidget.h"
#include "ctrl/SqHelper.h"
#include "ctrl/SqMenuItem.h"
#include "ctrl/SqTooltips.h"
#include "ctrl/SqWidgets.h"

using Comp = F2_Poly<WidgetComposite>;

class OnOffQuantity : public SqTooltips::SQParamQuantity {
public:
    OnOffQuantity(const ParamQuantity& other) : SqTooltips::SQParamQuantity(other) {
    }
    std::string getDisplayValueString() override {
        auto value = getValue();
        return value < .5 ? "Off" : "On";
    }
};

class TopologyQuantity : public SqTooltips::SQParamQuantity {
public:
    TopologyQuantity(const ParamQuantity& other) : SqTooltips::SQParamQuantity(other) {
    }
    std::string getDisplayValueString() override {
        int value = int(std::round(getValue()));
        std::string tip;
        switch (value) {
            case 0:
                tip = "12 dB/octave multi-mode";
                break;
            case 1:
                tip = "24 dB/octave multi-mode";
                break;
            case 2:
                tip = "two 12 dB/octave in parallel";
                break;
            case 3:
                tip = "two 12 dB/octave subtracted";
                break;
            default:
                assert(false);
        }
        return tip;
    }
};

class ModeQuantity : public SqTooltips::SQParamQuantity {
public:
    ModeQuantity(const ParamQuantity& other) : SqTooltips::SQParamQuantity(other) {
    }
    std::string getDisplayValueString() override {
        int value = int(std::round(getValue()));
        std::string tip;
        switch (value) {
            case 0:
                tip = "lowpass";
                break;
            case 2:
                tip = "highpass";
                break;
            case 1:
                tip = "bandpass";
                break;
            case 3:
                tip = "notch";
                break;
            default:
                assert(false);
        }
        return tip;
    }
};

class AttenQuantity : public SqTooltips::SQParamQuantity {
public:
    AttenQuantity(const ParamQuantity& other) : SqTooltips::SQParamQuantity(other) {
    }
    std::string getDisplayValueString() override {
        float value = getValue();
        SqStream str;
        str.precision(0);
        str.add(value * 100);
        str.add("%");
        return str.str();
    }
};

/**
 */
struct F2Module : Module {
public:
    F2Module();
    /**
     *
     * Overrides of Module functions
     */
    void process(const ProcessArgs& args) override;
    void onSampleRateChange() override;

    std::shared_ptr<Comp> blank;

private:
};

void F2Module::onSampleRateChange() {
}

F2Module::F2Module() {
    configBypass(Comp::AUDIO_INPUT, Comp::AUDIO_OUTPUT);
    config(Comp::NUM_PARAMS, Comp::NUM_INPUTS, Comp::NUM_OUTPUTS, Comp::NUM_LIGHTS);

        configInput(Comp::FC_INPUT, "Frequency Cutoff");
        configInput(Comp::Q_INPUT, "Filter resonance");
        configInput(Comp::R_INPUT, "Resonance breakpoint distance");
        configInput(Comp::AUDIO_INPUT, "Audio");
        configOutput(Comp::AUDIO_OUTPUT, "Audio");

    blank = std::make_shared<Comp>(this);
    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    SqHelper::setupParams(icomp, this);

    onSampleRateChange();
    blank->init();

    SqTooltips::changeParamQuantity<OnOffQuantity>(this, Comp::LIMITER_PARAM);
    SqTooltips::changeParamQuantity<TopologyQuantity>(this, Comp::TOPOLOGY_PARAM);
    SqTooltips::changeParamQuantity<ModeQuantity>(this, Comp::MODE_PARAM);
    SqTooltips::changeParamQuantity<AttenQuantity>(this, Comp::FC_TRIM_PARAM);
}

void F2Module::process(const ProcessArgs& args) {
    blank->process(args);
}

////////////////////
// module widget
////////////////////

struct F2Widget : ModuleWidget {
    F2Widget(F2Module*);

#ifdef _LAB
    Label* addLabel(const Vec& v, const char* str, const NVGcolor& color = SqHelper::COLOR_BLACK) {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
        return label;
    }
#endif

    void appendContextMenu(Menu* menu) override;

    void addJacks(F2Module* module, std::shared_ptr<IComposite> icomp);
    void addKnobs(F2Module* module, std::shared_ptr<IComposite> icomp);
    void addLights(F2Module* module);
};

void F2Widget::appendContextMenu(Menu* theMenu) {
    MenuLabel* spacerLabel = new MenuLabel();
    theMenu->addChild(spacerLabel);

    //   SqMenuItem_BooleanParam2* item = new SqMenuItem_BooleanParam2(module, Comp::CV_UPDATE_FREQ);
    //    item->text = "CV Fidelity";

    SqMenuItem_BooleanParam2* item = new SqMenuItem_BooleanParam2(module, Comp::ALT_LIMITER_PARAM);
    item->text = "Alt Limiter";
    theMenu->addChild(item);
}

void F2Widget::addLights(F2Module* module) {
    float xLED = 50;
    float yVol1 = 50 + 2;
    float dyPoles = 8;
    for (int i = 0; i < 4; ++i) {
        switch (i) {
            case 0:
            case 1:
                addChild(createLightCentered<SmallLight<GreenLight>>(
                    Vec(xLED, yVol1 + dyPoles * (3 - i)),
                    module,
                    Comp::VOL0_LIGHT + i));
                break;
            case 2:
                addChild(createLightCentered<SmallLight<YellowLight>>(
                    Vec(xLED, yVol1 + dyPoles * (3 - i)),
                    module,
                    Comp::VOL0_LIGHT + i));
                break;
            case 3:
                addChild(createLightCentered<SmallLight<RedLight>>(
                    Vec(xLED, yVol1 + dyPoles * (3 - i)),
                    module,
                    Comp::VOL0_LIGHT + i));
                break;
        }
    }
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(84, 57),
        module,
        Comp::LIMITER_LIGHT));
}

void F2Widget::addKnobs(F2Module* module, std::shared_ptr<IComposite> icomp) {
#ifdef _LAB
    addLabel(
        Vec(14 - 6, 166),
        "Fc");
#endif
    addParam(SqHelper::createParam<RoganSLBlue30>(
        icomp,
        Vec(7, 185),
        module, Comp::FC_PARAM));
    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(13, 228),
        module,
        Comp::FC_TRIM_PARAM));
#ifdef _LAB
    addLabel(
        Vec(55 - 8, 166),
        "Q");
#endif
    addParam(SqHelper::createParam<RoganSLBlue30>(
        icomp,
        Vec(45, 185),
        module, Comp::Q_PARAM));
    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(50.5, 228),
        module,
        Comp::Q_TRIM_PARAM));
#ifdef _LAB
    addLabel(
        Vec(95 - 8, 166),
        "R");
#endif
    addParam(SqHelper::createParam<RoganSLBlue30>(
        icomp,
        Vec(84, 185),
        module, Comp::R_PARAM));
    addParam(SqHelper::createParam<Trimpot>(
        icomp,
        Vec(90, 228),
        module,
        Comp::R_TRIM_PARAM));
#ifdef _LAB
    addLabel(
        Vec(10 - 8, 32),
        "Vol");
#endif
    addParam(SqHelper::createParam<RoganSLBlue30>(
        icomp,
        Vec(7, 50),
        module, Comp::VOL_PARAM));
#ifdef _LAB
    addLabel(
        Vec(83 - 10, 32),
        "Limit");
#endif
    addParam(SqHelper::createParam<CKSS>(
        icomp,
        Vec(93, 51),
        module, Comp::LIMITER_PARAM));

    PopupMenuParamWidget* p = SqHelper::createParam<PopupMenuParamWidget>(
        icomp,
        Vec(7, 97),
        module,
        Comp::MODE_PARAM);
    p->box.size.x = 104;  // width
    p->box.size.y = 22;
    p->text = "LP";
    p->setLabels({"LP", "BP", "HP", "N"});
    addParam(p);

    p = SqHelper::createParam<PopupMenuParamWidget>(
        icomp,
        Vec(7, 131),
        module,
        Comp::TOPOLOGY_PARAM);
    p->box.size.x = 104;  // width was 54
    p->box.size.y = 22;
    p->text = "12dB";
    p->setLabels({"12dB", "24dB", "Par", "Par -"});
    addParam(p);

    // just for test
#if 0
    addChild(createLight<MediumLight<GreenLight>>(
            Vec(40, 18),
            module,
            Comp::LIGHT_TEST));
#endif
}

void F2Widget::addJacks(F2Module* module, std::shared_ptr<IComposite> icomp) {
#ifdef _LAB
    addLabel(
        Vec(14 - 8, 258),
        "Fc");
#endif
    addInput(createInput<PJ301MPort>(
        Vec(10, 275),
        module,
        Comp::FC_INPUT));
#ifdef _LAB
    addLabel(
        Vec(55 - 8, 258),
        "Q");
#endif
    addInput(createInput<PJ301MPort>(
        Vec(48, 275),
        module,
        Comp::Q_INPUT));
#ifdef _LAB
    addLabel(
        Vec(95 - 8, 258),
        "R");
#endif
    addInput(createInput<PJ301MPort>(
        Vec(87, 274),
        module,
        Comp::R_INPUT));
#ifdef _LAB
    addLabel(
        Vec(15 - 8, 303),
        "In");
#endif
    addInput(createInput<PJ301MPort>(
        Vec(10, 320),
        module,
        Comp::AUDIO_INPUT));
#ifdef _LAB
    addLabel(
        Vec(87 - 8, 303),
        "Out");
#endif
    addOutput(createOutput<PJ301MPort>(
        Vec(87, 320),
        module,
        Comp::AUDIO_OUTPUT));
};

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */

F2Widget::F2Widget(F2Module* module) {
    setModule(module);
    SqHelper::setPanel(this, "res/f2-panel.svg");

#ifdef _LAB
    addLabel(Vec(50, 10), "F2");
#endif

    std::shared_ptr<IComposite> icomp = Comp::getDescription();
    addJacks(module, icomp);
    addKnobs(module, icomp);
    addLights(module);

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model* modelF2Module = createModel<F2Module, F2Widget>("squinkylabs-f2");
#endif
