#pragma once
#include <algorithm>

#include "AudioMath.h"
#include "IComposite.h"
#include "LookupTable.h"
#include "LookupTableFactory.h"
#include "MultiModOsc.h"
#include "ObjectCache.h"
#include "StateVariableFilter.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

#define _ANORM

template <class TBase>
class VocalAnimatorDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

/**
 * Version 2 - make the math sane.
 * was 46
 * with mod sub-sample 2 => 26
 */
template <class TBase>
class VocalAnimator : public TBase {
public:
    typedef float T;
    static const int numTriangle = 4;
    static const int numModOutputs = 3;
    static const int numFilters = 4;
    static const int modulationSubSample = 2;  // do at a fraction of the audio sample rate

    VocalAnimator(Module* module) : TBase(module) {
    }
    VocalAnimator() : TBase() {
    }

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<VocalAnimatorDescription<TBase>>();
    }

    void setSampleRate(float rate) {
        reciprocalSampleRate = 1 / rate;
        modulatorParams.setRateAndSpread(.5, .5, 0, reciprocalSampleRate);
    }

    enum ParamIds {
        LFO_RATE_PARAM,
        FILTER_Q_PARAM,
        FILTER_FC_PARAM,
        FILTER_MOD_DEPTH_PARAM,
        LFO_RATE_TRIM_PARAM,
        FILTER_Q_TRIM_PARAM,
        FILTER_FC_TRIM_PARAM,
        FILTER_MOD_DEPTH_TRIM_PARAM,
        BASS_EXP_PARAM,

        // tracking:
        //  0 = all 1v/octave, mod scaled, no on top
        //  1 = mod and cv scaled
        //  2 = 1, + top filter gets some mod
        TRACK_EXP_PARAM,

        // LFO mixing options
        // 0 = classic
        // 1 = option
        // 2 = lf sub
        LFO_MIX_PARAM,

        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_INPUT,
        LFO_RATE_CV_INPUT,
        FILTER_Q_CV_INPUT,
        FILTER_FC_CV_INPUT,
        FILTER_MOD_DEPTH_CV_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_OUTPUT,
        LFO0_OUTPUT,
        LFO1_OUTPUT,
        LFO2_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        LFO0_LIGHT,
        LFO1_LIGHT,
        LFO2_LIGHT,
        BASS_LIGHT,
        NUM_LIGHTS
    };

    void init();
    void step() override;
    void stepModulation();
    T modulatorOutput[numModOutputs];

    // The frequency inputs to the filters, exposed for testing.

    T filterFrequencyLog[numFilters];

    const T nominalFilterCenterHz[numFilters] = {522, 1340, 2570, 3700};
    const T nominalFilterCenterLog2[numFilters] = {
        std::log2(T(522)),
        std::log2(T(1340)),
        std::log2(T(2570)),
        std::log2(T(3700))};
    // 1, .937 .3125
    const T nominalModSensitivity[numFilters] = {T(1), T(.937), T(.3125), 0};

    // Following are for unit tests.
    T normalizedFilterFreq[numFilters];
    bool jamModForTest = false;
    T modValueForTest = 0;
    int modulationSubSampleCounter = 1;
    T filterNormalizedBandwidth = .1f;

    float reciprocalSampleRate;

    using osc = MultiModOsc<T, numTriangle, numModOutputs>;
    typename osc::State modulatorState;
    typename osc::Params modulatorParams;

    StateVariableFilterState<T> filterStates[numFilters];
    StateVariableFilterParams<T> filterParams[numFilters];

    std::shared_ptr<LookupTableParams<T>> expLookup;

    // We need a bunch of scalers to convert knob, CV, trim into the voltage
    // range each parameter needs.
    AudioMath::ScaleFun<T> scale0_1;
    AudioMath::ScaleFun<T> scalem2_2;
    AudioMath::ScaleFun<T> scaleQ;
    AudioMath::ScaleFun<T> scalen5_5;
};

template <class TBase>
inline void VocalAnimator<TBase>::init() {
    for (int i = 0; i < numFilters; ++i) {
        filterParams[i].setMode(StateVariableFilterParams<T>::Mode::BandPass);
        filterParams[i].setQ(15);  // or should it be 5?

        filterParams[i].setFreq(nominalFilterCenterHz[i] * reciprocalSampleRate);
        filterFrequencyLog[i] = nominalFilterCenterLog2[i];

        normalizedFilterFreq[i] = nominalFilterCenterHz[i] * reciprocalSampleRate;
    }
    scale0_1 = AudioMath::makeScalerWithBipolarAudioTrim(0, 1);    // full CV range -> 0..1
    scalem2_2 = AudioMath::makeScalerWithBipolarAudioTrim(-2, 2);  // full CV range -> -2..2
    scaleQ = AudioMath::makeScalerWithBipolarAudioTrim(.71f, 21);
    scalen5_5 = AudioMath::makeScalerWithBipolarAudioTrim(-5, 5);

    // make table of 2 ** x
    expLookup = ObjectCache<T>::getExp2();
}

template <class TBase>
inline void VocalAnimator<TBase>::step() {
    // printf("step %d\n", modulationSubSampleCounter);
    if (--modulationSubSampleCounter <= 0) {
        modulationSubSampleCounter = modulationSubSample;
        stepModulation();
    }

    // Now run the filters
    T filterMix = 0;  // Sum the folder outputs here
    const T input = TBase::inputs[AUDIO_INPUT].getVoltage(0);

    for (int i = 0; i < numFilters; ++i) {
        filterMix += StateVariableFilter<T>::run(input, filterStates[i], filterParams[i]);
    }
#ifdef _ANORM
    filterMix *= filterNormalizedBandwidth * 2;
#else
    filterMix *= T(.3);  // attenuate to avoid clip
#endif
    TBase::outputs[AUDIO_OUTPUT].setVoltage(filterMix, 0);
}

template <class TBase>
inline void VocalAnimator<TBase>::stepModulation() {
    // printf("step mod\n");
    const bool bass = TBase::params[BASS_EXP_PARAM].value > .5;
    const auto mode = bass ? StateVariableFilterParams<T>::Mode::LowPass : StateVariableFilterParams<T>::Mode::BandPass;

    for (int i = 0; i < numFilters + 1 - 1; ++i) {
        filterParams[i].setMode(mode);
    }

    // Run the modulators, hold onto their output.
    // Raw Modulator outputs put in modulatorOutputs[].
    osc::run(modulatorOutput, modulatorState, modulatorParams);

    static const OutputIds LEDOutputs[] = {
        LFO0_OUTPUT,
        LFO1_OUTPUT,
        LFO2_OUTPUT,
    };
    // Light up the LEDs with the unscaled Modulator outputs.
    for (int i = LFO0_LIGHT; i <= LFO2_LIGHT; ++i) {
        TBase::outputs[LEDOutputs[i]].setVoltage(modulatorOutput[i], 0);
        TBase::lights[i].value = (modulatorOutput[i]) * .3f;
        TBase::outputs[LEDOutputs[i]].setVoltage(modulatorOutput[i], 0);
    }

    // Normalize all the parameters out here
    const T qFinal = scaleQ(
        TBase::inputs[FILTER_Q_CV_INPUT].getVoltage(0),
        TBase::params[FILTER_Q_PARAM].value,
        TBase::params[FILTER_Q_TRIM_PARAM].value);

    const T fc = scalen5_5(
        TBase::inputs[FILTER_FC_CV_INPUT].getVoltage(0),
        TBase::params[FILTER_FC_PARAM].value,
        TBase::params[FILTER_FC_TRIM_PARAM].value);

    // put together a mod depth parameter from all the inputs
    // range is 0..1

    // cv, knob, trim
    const T baseModDepth = scale0_1(
        TBase::inputs[FILTER_MOD_DEPTH_CV_INPUT].getVoltage(0),
        TBase::params[FILTER_MOD_DEPTH_PARAM].value,
        TBase::params[FILTER_MOD_DEPTH_TRIM_PARAM].value);

    // tracking:
    //  0 = all 1v/octave, mod scaled, no on top
    //  1 = mod and cv scaled
    //  2 = 1, + top filter gets some mod
    int cvScaleMode = 0;
    const float cvScaleParam = TBase::params[TRACK_EXP_PARAM].value;
    if (cvScaleParam < .5) {
        cvScaleMode = 0;
    } else if (cvScaleParam < 1.5) {
        cvScaleMode = 1;
    } else {
        cvScaleMode = 2;
        assert(cvScaleParam < 2.5);
    }

    // Just do the Q division once, in the outer loop
    filterNormalizedBandwidth = T(1) / qFinal;
    for (int i = 0; i < numFilters; ++i) {
        T logFreq = nominalFilterCenterLog2[i];

        switch (cvScaleMode) {
            case 1:
                // In this mode (1) CV comes straight through at 1V/8
                // Even on the top (fixed) filter
                logFreq += fc;  // add without attenuation for 1V/octave
                break;
            case 0:
                // In mode (0) CV gets scaled per filter, as in the original design.
                // Since nominalModSensitivity[3] == 0, top doesn't track
                logFreq += fc * nominalModSensitivity[i];
                break;
            case 2:
                if (fc < 0) {
                    // Normal scaling for Fc less than nominal
                    logFreq += fc * nominalModSensitivity[i];
                } else {
                    // above nominal, they all track the high one (so they don't cross)
                    logFreq += fc * nominalModSensitivity[2];
                }

                break;
            default:
                assert(false);
        }

        // First three filters always modulated,
        // (wanted to try modulating 4, but don't have an LFO (yet
        const bool modulateThisFilter = (i < 3);
        if (modulateThisFilter) {
            logFreq += modulatorOutput[i] *
                       baseModDepth *
                       nominalModSensitivity[i];
        }

        logFreq += ((i < 3) ? modulatorOutput[i] : 0) *
                   baseModDepth *
                   nominalModSensitivity[i];

        filterFrequencyLog[i] = logFreq;

        // tell lookup not to assert - we know we can go slightly out of range.
        T normFreq = LookupTable<T>::lookup(*expLookup, logFreq, true) * reciprocalSampleRate;
        normFreq = std::min(normFreq, T(.2));

        normalizedFilterFreq[i] = normFreq;
        filterParams[i].setFreq(normFreq);

        filterParams[i].setNormalizedBandwidth(filterNormalizedBandwidth);
    }

    int matrixMode;
    float mmParam = TBase::params[LFO_MIX_PARAM].value;
    if (mmParam < .5) {
        matrixMode = 0;
    } else if (mmParam < 1.5) {
        matrixMode = 1;
    } else {
        matrixMode = 2;
        assert(mmParam < 2.5);
    }

    const T spread = T(1.0);

    // scale by sub-sample rate to lfo rate sounds right.
    const float modRate = modulationSubSample * scalem2_2(
                                                    TBase::inputs[LFO_RATE_CV_INPUT].getVoltage(0),
                                                    TBase::params[LFO_RATE_PARAM].value,
                                                    TBase::params[LFO_RATE_TRIM_PARAM].value);
    modulatorParams.setRateAndSpread(
        modRate,
        spread,
        matrixMode,
        reciprocalSampleRate);
}

template <class TBase>
int VocalAnimatorDescription<TBase>::getNumParams() {
    return VocalAnimator<TBase>::NUM_PARAMS;
}

/*
enum ParamIds
{
    LFO_RATE_PARAM,
    FILTER_Q_PARAM,
    FILTER_FC_PARAM,
    FILTER_MOD_DEPTH_PARAM,
    LFO_RATE_TRIM_PARAM,
    FILTER_Q_TRIM_PARAM,
    FILTER_FC_TRIM_PARAM,
    FILTER_MOD_DEPTH_TRIM_PARAM,
    BASS_EXP_PARAM,

    // tracking:
    //  0 = all 1v/octave, mod scaled, no on top
    //  1 = mod and cv scaled
    //  2 = 1, + top filter gets some mod
    TRACK_EXP_PARAM,

    // LFO mixing options
    // 0 = classic
    // 1 = option
    // 2 = lf sub
    LFO_MIX_PARAM,

    NUM_PARAMS
};
*/
template <class TBase>
inline IComposite::Config VocalAnimatorDescription<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case VocalAnimator<TBase>::LFO_RATE_PARAM:
            ret = {-5.0, 5.0, 0.0, "LFO rate"};
            break;
        case VocalAnimator<TBase>::FILTER_Q_PARAM:
            ret = {-5.0, 5.0, 0.0, "Filter Q"};
            break;
        case VocalAnimator<TBase>::FILTER_FC_PARAM:
            ret = {-5.0, 5.0, 0.0, "Filter cutoff"};
            break;
        case VocalAnimator<TBase>::FILTER_MOD_DEPTH_PARAM:
            ret = {-5.0, 5.0, 0.0, "Filter mod depth"};
            break;
        case VocalAnimator<TBase>::LFO_RATE_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "LFO rate trim"};
            break;
        case VocalAnimator<TBase>::FILTER_Q_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "FilterQ trim"};
            break;
        case VocalAnimator<TBase>::FILTER_FC_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "Filter cutoff trim"};
            break;
        case VocalAnimator<TBase>::FILTER_MOD_DEPTH_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "Filter mod depth trim"};
            break;
        case VocalAnimator<TBase>::BASS_EXP_PARAM:
            ret = {0.0f, 1.0f, 0.0f, "Bass expand"};
            break;
        case VocalAnimator<TBase>::TRACK_EXP_PARAM:
            ret = {0, 2, 0, "Track ExP"};  // TODO: we don't use this - what is the default?
            ret.active = false;
            break;
        case VocalAnimator<TBase>::LFO_MIX_PARAM:
            ret = {0.0f, 2.0f, 0.0f, "LFO mix select"};
            break;
        default:
            assert(false);
            break;
    }
    return ret;
}