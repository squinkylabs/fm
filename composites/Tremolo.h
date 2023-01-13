
#pragma once

#include <vector>

#include "AsymRampShaper.h"
#include "ClockMult.h"
#include "GateTrigger.h"
#include "IComposite.h"
#include "ObjectCache.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class TremoloDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

/**
 * CPU usage was 15
 * down to 7.2 with /4 subsample
 */
template <class TBase>
class Tremolo : public TBase {
public:
    Tremolo(Module* module) : TBase(module), gateTrigger(true) {
    }

    Tremolo() : TBase(), gateTrigger(true) {
    }

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<TremoloDescription<TBase>>();
    }

    void setSampleRate(float rate) {
        reciprocalSampleRate = 1 / rate;
    }

    // must be called after setSampleRate
    void init();

    enum ParamIds {
        LFO_RATE_PARAM,
        LFO_SHAPE_PARAM,
        LFO_SKEW_PARAM,
        LFO_PHASE_PARAM,
        MOD_DEPTH_PARAM,
        CLOCK_MULT_PARAM,

        LFO_SHAPE_TRIM_PARAM,
        LFO_SKEW_TRIM_PARAM,
        LFO_PHASE_TRIM_PARAM,
        MOD_DEPTH_TRIM_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_INPUT,
        CLOCK_INPUT,
        LFO_SHAPE_INPUT,
        LFO_SKEW_INPUT,
        LFO_PHASE_INPUT,
        MOD_DEPTH_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_OUTPUT,
        SAW_OUTPUT,
        LFO_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    /**
     * Main processing entry point. Called every sample
     */
    void step() override;

private:
    int inputSubSampleCounter = 1;
    const static int inputSubSample = 4;  // only look at knob/cv every 4
    float skew = .1f;
    float phase = 0;
    float shape = 0;
    float modDepth = 0;
    float shapeMul = 0;
    float gain = 0;

    void stepInput();

    ClockMult clock;
    std::shared_ptr<LookupTableParams<float>> tanhLookup;
    float reciprocalSampleRate = 0;

    AsymRampShaperParams rampShaper;
    std::shared_ptr<LookupTableParams<float>> exp2 = ObjectCache<float>::getExp2();

    // make some bootstrap scalers
    AudioMath::ScaleFun<float> scale_rate;
    AudioMath::ScaleFun<float> scale_skew;
    AudioMath::ScaleFun<float> scale_shape;
    AudioMath::ScaleFun<float> scale_depth;
    AudioMath::ScaleFun<float> scale_phase;

    GateTrigger gateTrigger;
};

template <class TBase>
inline void Tremolo<TBase>::init() {
    tanhLookup = ObjectCache<float>::getTanh5();
    clock.setMultiplier(0);

    scale_rate = AudioMath::makeLinearScaler(4.f, 9.f);  // log domain, 16 range
    scale_skew = AudioMath::makeLinearScaler(-.99f, .99f);
    scale_shape = AudioMath::makeLinearScaler(0.f, 1.f);
    scale_depth = AudioMath::makeLinearScaler(0.f, 1.f);
    scale_phase = AudioMath::makeLinearScaler(-1.f, 1.f);

    stepInput();  // call once to init
}

template <class TBase>
inline void Tremolo<TBase>::stepInput() {
    int clockMul = (int)round(TBase::params[CLOCK_MULT_PARAM].value);

    // UI is shifted
    clockMul++;
    if (clockMul > 4) {
        clockMul = 0;
    }

    clock.setMultiplier(clockMul);

    // second look at knowb and CV
    shape = scale_shape(
        TBase::inputs[LFO_SHAPE_INPUT].getVoltage(0),
        TBase::params[LFO_SHAPE_PARAM].value,
        TBase::params[LFO_SHAPE_TRIM_PARAM].value);

    skew = scale_skew(
        TBase::inputs[LFO_SKEW_INPUT].getVoltage(0),
        TBase::params[LFO_SKEW_PARAM].value,
        TBase::params[LFO_SKEW_TRIM_PARAM].value);

    phase = scale_phase(
        TBase::inputs[LFO_PHASE_INPUT].getVoltage(0),
        TBase::params[LFO_PHASE_PARAM].value,
        TBase::params[LFO_PHASE_TRIM_PARAM].value);

    modDepth = scale_depth(
        TBase::inputs[MOD_DEPTH_INPUT].getVoltage(0),
        TBase::params[MOD_DEPTH_PARAM].value,
        TBase::params[MOD_DEPTH_TRIM_PARAM].value);

    shapeMul = std::max(.25f, 10 * shape);
    gain = modDepth /
           LookupTable<float>::lookup(*tanhLookup.get(), (shapeMul / 2));

    // update internal clock from knob
    if (clockMul == 0)  // only calc rate for internal
    {
        const float logRate = scale_rate(
            0,
            TBase::params[LFO_RATE_PARAM].value,
            1);

        float rate = LookupTable<float>::lookup(*exp2, logRate);
        float scaledRate = rate * .06f;
        clock.setFreeRunFreq(scaledRate * reciprocalSampleRate);
    }

    AsymRampShaper::setup(rampShaper, skew, phase);
}

template <class TBase>
inline void Tremolo<TBase>::step() {
    if (--inputSubSampleCounter <= 0) {
        inputSubSampleCounter = inputSubSample;
        stepInput();
    }

    // First: external clock proc
    gateTrigger.go(TBase::inputs[CLOCK_INPUT].getVoltage(0));
    if (gateTrigger.trigger()) {
        clock.refClock();
    }

    // ------------ now generate the lfo waveform
    clock.sampleClock();
    float mod = clock.getSaw();
    mod = AsymRampShaper::proc_1(rampShaper, mod);
    mod -= 0.5f;
    // now we have a skewed saw -.5 to .5
    TBase::outputs[SAW_OUTPUT].setVoltage(10 * mod, 0);

    // TODO: don't scale twice - just get it right the first time
    //  const float shapeMul = std::max(.25f, 10 * shape);
    mod *= shapeMul;
    mod = LookupTable<float>::lookup(*tanhLookup.get(), mod);
    TBase::outputs[LFO_OUTPUT].setVoltage(5 * mod, 0);

    const float finalMod = gain * mod + 1;  // TODO: this offset by 1 is pretty good, but we
                                            // could add an offset control to make it really "chop" off

    TBase::outputs[AUDIO_OUTPUT].setVoltage(TBase::inputs[AUDIO_INPUT].getVoltage(0) * finalMod, 0);
}

/*


old plug proc loop.

// Step 1: generate a saw
// range is 0..1
SawOsc<vec_t>::gen_v(*sawState, *sawParams, tempBuffer, sampleFrames);

// step 2: apply skew and phase shift
// range still 0..1
AsymRampShaper<vec_t>::proc_v(*shaperParams, tempBuffer, tempBuffer, sampleFrames);

// step 3: shift down to be centered at zero,
// max excursion +-5 at shape "most square"
// min is +/- .25  TODO: put the .25 into the control range itself

// range = +/- (5 * shape)
//
f_t shapeMul = std::max(.25, 10 * controlValues.lfoShape);
VecBasic<vec_t>::add_mul_c_imp(tempBuffer, sampleFrames, shapeMul, -.5f);

// now tanh,
// output contered around zero,
// max is tanh(.25) to tanh(5), depending on shape value
// rang = +/- tanh(5 * shape)
LookupUniform<vec_t>::lookup_clip_v(*tanhParams, tempBuffer, tempBuffer, sampleFrames);

    // so: makeup gain of 1/tanh(shapeMul) will get us to +1/-1
    // then multiply by depth to get contered around zero with correct depth
    // the add one to get back to trem range!
    f_t gain = controlValues.modDepth / tanh(shapeMul/2);
    VecBasic<vec_t>::mul_add_c_imp(tempBuffer, sampleFrames, gain, 1);
// scale then add constant
    // input = a * input + b
    static void mul_add_c_imp(f_t * inout, int size, f_t a, f_t b) {
        assert_size(size);

    // now range = +/- tanh(5*shape) * depth / tanh(10 * shape)
*/

template <class TBase>
int TremoloDescription<TBase>::getNumParams() {
    return Tremolo<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config TremoloDescription<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case Tremolo<TBase>::LFO_RATE_PARAM:
            ret = {-5.0, 5.0, 0.0, "LFO rate"};
            break;
        case Tremolo<TBase>::LFO_SHAPE_PARAM:
            ret = {-5.0, 5.0, 0.0, "LFO shape"};
            break;
        case Tremolo<TBase>::LFO_SKEW_PARAM:
            ret = {-5.0, 5.0, 0.0, "LFO skew"};
            break;
        case Tremolo<TBase>::LFO_PHASE_PARAM:
            ret = {-5.0, 5.0, 0.0, "LFO phase"};
            break;
        case Tremolo<TBase>::MOD_DEPTH_PARAM:
            ret = {-5.0, 5.0, 0.0, "Mod depth"};
            break;
        case Tremolo<TBase>::CLOCK_MULT_PARAM:
            ret = {0.0f, 4.0f, 4.0f, "Clock multiplier"};
            break;
        case Tremolo<TBase>::LFO_SHAPE_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "LFO shape trim"};
            break;
        case Tremolo<TBase>::LFO_SKEW_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "LFO skew trim"};
            break;
        case Tremolo<TBase>::LFO_PHASE_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "LFO phase trim"};
            break;
        case Tremolo<TBase>::MOD_DEPTH_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "Mod depth trim"};
            break;
        default:
            assert(false);
    }
    return ret;
}