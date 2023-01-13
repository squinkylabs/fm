
#pragma once

#include <assert.h>

#include <memory>

#include "Divider.h"
#include "IComposite.h"
#include "MultiLag.h"
#include "ObjectCache.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class Slew4Description : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

/**
 * perf, initial build. 11.35%
 * with all normals, now 13.5%
 * 
 */
template <class TBase>
class Slew4 : public TBase {
public:
    Slew4(Module* module) : TBase(module) {
    }

    Slew4() : TBase() {
    }

    /**
    * re-calc everything that changes with sample
    * rate. Also everything that depends on baseFrequency.
    *
    * Only needs to be called once.
    */
    void init();

    enum ParamIds {
        PARAM_RISE,
        PARAM_FALL,
        PARAM_LEVEL,
        NUM_PARAMS
    };

    enum InputIds {
        INPUT_TRIGGER0,
        INPUT_TRIGGER1,
        INPUT_TRIGGER2,
        INPUT_TRIGGER3,
        INPUT_TRIGGER4,
        INPUT_TRIGGER5,
        INPUT_TRIGGER6,
        INPUT_TRIGGER7,
        INPUT_AUDIO0,
        INPUT_AUDIO1,
        INPUT_AUDIO2,
        INPUT_AUDIO3,
        INPUT_AUDIO4,
        INPUT_AUDIO5,
        INPUT_AUDIO6,
        INPUT_AUDIO7,
        INPUT_RISE,
        INPUT_FALL,
        NUM_INPUTS
    };

    enum OutputIds {
        OUTPUT0,
        OUTPUT1,
        OUTPUT2,
        OUTPUT3,
        OUTPUT4,
        OUTPUT5,
        OUTPUT6,
        OUTPUT7,
        OUTPUT_MIX0,
        OUTPUT_MIX1,
        OUTPUT_MIX2,
        OUTPUT_MIX3,
        OUTPUT_MIX4,
        OUTPUT_MIX5,
        OUTPUT_MIX6,
        OUTPUT_MIX7,
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<Slew4Description<TBase>>();
    }

    /**
     * Main processing entry point. Called every sample
     */
    void step() override;

    void onSampleRateChange() override {
        knobToFilterL = makeLPFDirectFilterLookup<float>(this->engineGetSampleTime(), 4);
    }

private:
    MultiLag<8> lag;
    std::shared_ptr<LookupTableParams<float>> knobToFilterL;
    Divider divider;

    void updateKnobs();

    AudioMath::ScaleFun<float> lin = AudioMath::makeLinearScaler<float>(0, 1);
    std::shared_ptr<LookupTableParams<float>> audioTaper =
        ObjectCache<float>::getAudioTaper();
    float _outputLevel = 0;
};

template <class TBase>
inline void Slew4<TBase>::init() {
    divider.setup(4, [this]() {
        updateKnobs();
    });

    onSampleRateChange();
    lag.setAttack(.1f);
    lag.setRelease(.0001f);
}

template <class TBase>
inline void Slew4<TBase>::updateKnobs() {
    const float combinedA = lin(
        TBase::inputs[INPUT_RISE].getVoltage(0),
        TBase::params[PARAM_RISE].value,
        1);

    const float combinedR = lin(
        TBase::inputs[INPUT_FALL].getVoltage(0),
        TBase::params[PARAM_FALL].value,
        1);

    lag.setEnable(true);
#if 0
    // TODO: does this unit really want to disable ever?
    if (combinedA < .1 && combinedR < .1) {
        lag.setEnable(false);
    } else {
        lag.setEnable(true);
#endif

    const float lA = LookupTable<float>::lookup(*knobToFilterL, combinedA);
    lag.setAttackL(lA);
    const float lR = LookupTable<float>::lookup(*knobToFilterL, combinedR);
    lag.setReleaseL(lR);

    const float knob = TBase::params[PARAM_LEVEL].value;
    _outputLevel = LookupTable<float>::lookup(*audioTaper, knob, false);
}

template <class TBase>
inline void Slew4<TBase>::step() {
    divider.step();
    // get input to slews
    float slewInput[8];
    float triggerIn = 0;
    for (int i = 0; i < 8; ++i) {
        // if input is patched, it becomes the new normaled input;
        const bool bPatched = TBase::inputs[i + INPUT_TRIGGER0].isConnected();
        if (bPatched) {
            triggerIn = TBase::inputs[i + INPUT_TRIGGER0].getVoltage(0);
        }

        slewInput[i] = triggerIn;
    }

    // clock the slew
    lag.step(slewInput);

    // send slew to output
    float sum = 0;
    for (int i = 0; i < 8; ++i) {
        //if audio in hooked up, then output[n] = input[n] * lag
        // else output = lag
        float inputValue = 10.f;

        if (TBase::inputs[i + INPUT_AUDIO0].isConnected()) {
            inputValue = TBase::inputs[i + INPUT_AUDIO0].getVoltage(0);
        }
        TBase::outputs[i + OUTPUT0].setVoltage(lag.get(i) * inputValue * .1f, 0);
        sum += TBase::outputs[i + OUTPUT0].getVoltage(0);

        // normaled output logic: patched outputs get the sum of the un-patched above them.
        if (TBase::outputs[i + OUTPUT_MIX0].isConnected()) {
            TBase::outputs[i + OUTPUT_MIX0].setVoltage(sum * _outputLevel, 0);
            sum = 0;
        }
    }
}

template <class TBase>
int Slew4Description<TBase>::getNumParams() {
    return Slew4<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config Slew4Description<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case Slew4<TBase>::PARAM_RISE:
            ret = {-5.0f, 5.0f, 0, "Rise time"};
            break;
        case Slew4<TBase>::PARAM_FALL:
            ret = {-5.0f, 5.0f, 0, "Fall time"};
            break;
        case Slew4<TBase>::PARAM_LEVEL:
            ret = {0.f, 1.f, .5, "Level"};
            break;
        default:
            assert(false);
    }
    return ret;
}
