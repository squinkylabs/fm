#pragma once
#include <algorithm>
#include <cmath>

#include "AudioMath.h"
#include "FormantTables2.h"
#include "IComposite.h"
#include "LookupTable.h"
#include "LookupTableFactory.h"
#include "ObjectCache.h"
#include "StateVariableFilter.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class VocalDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

/**
 * original version CPU usage = 84
 * update filters less often => 28.4
 */
template <class TBase>
class VocalFilter : public TBase {
public:
    typedef float T;
    static const int numFilters = FormantTables2::numFormantBands;

    VocalFilter(Module* module) : TBase(module) {
    }
    VocalFilter() : TBase() {
    }

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<VocalDescription<TBase>>();
    }

    void setSampleRate(float rate) {
        reciprocalSampleRate = 1 / rate;
    }

    enum ParamIds {
        FILTER_Q_PARAM,
        FILTER_Q_TRIM_PARAM,
        FILTER_FC_PARAM,
        FILTER_FC_TRIM_PARAM,
        FILTER_VOWEL_PARAM,
        FILTER_VOWEL_TRIM_PARAM,
        FILTER_MODEL_SELECT_PARAM,
        FILTER_BRIGHTNESS_PARAM,
        FILTER_BRIGHTNESS_TRIM_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_INPUT,
        FILTER_Q_CV_INPUT,
        FILTER_FC_CV_INPUT,
        FILTER_VOWEL_CV_INPUT,
        FILTER_BRIGHTNESS_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        LED_A,
        LED_E,
        LED_I,
        LED_O,
        LED_U,
        NUM_LIGHTS
    };

    void init();
    void step() override;
    void stepFilters();

    float reciprocalSampleRate;

    // The frequency inputs to the filters, exposed for testing.

    T filterFrequencyLog[numFilters];

    StateVariableFilterState<T> filterStates[numFilters];
    StateVariableFilterParams<T> filterParams[numFilters];
    float m_gain[numFilters] = {0};

    FormantTables2 formantTables;
    std::shared_ptr<LookupTableParams<T>> expLookup;
    std::shared_ptr<LookupTableParams<T>> db2GainLookup;

    AudioMath::ScaleFun<T> scaleCV_to_formant;
    AudioMath::ScaleFun<T> scaleQ;
    AudioMath::ScaleFun<T> scaleFc;
    AudioMath::ScaleFun<T> scaleBrightness;

    int cycleCount = 1;
};

template <class TBase>
inline void VocalFilter<TBase>::init() {
    for (int i = 0; i < numFilters; ++i) {
        filterParams[i].setMode(StateVariableFilterParams<T>::Mode::BandPass);
        filterParams[i].setQ(15);  // or should it be 5?

        filterParams[i].setFreq(T(.1));
    }
    scaleCV_to_formant = AudioMath::makeLinearScaler<T>(0, formantTables.numVowels - 1);
    scaleFc = AudioMath::makeLinearScaler<T>(-2, 2);
    scaleBrightness = AudioMath::makeLinearScaler<T>(0, 1);

    AudioMath::ScaleFun<T> rawQKnob = AudioMath::makeLinearScaler<T>(-1, 1);
    scaleQ = [rawQKnob](T cv, T param, T trim) {
        T temp = rawQKnob(cv, param, trim);
        return (temp >= 0) ? 1 - 3 * temp / 4 : 1 - temp;
    };

    // get reference to table of 2 ** x
    expLookup = ObjectCache<T>::getExp2();
    db2GainLookup = ObjectCache<T>::getDb2Gain();
}

template <class TBase>
inline void VocalFilter<TBase>::stepFilters() {
    int model = 0;
    const T switchVal = TBase::params[FILTER_MODEL_SELECT_PARAM].value;
    if (switchVal < .5) {
        model = 0;
        assert(switchVal > -.5);
    } else if (switchVal < 1.5) {
        model = 1;
    } else if (switchVal < 2.5) {
        model = 2;
    } else if (switchVal < 3.5) {
        model = 3;
    } else {
        model = 4;
        assert(switchVal < 4.5);
    }

    const T fVowel = scaleCV_to_formant(
        TBase::inputs[FILTER_VOWEL_CV_INPUT].getVoltage(0),
        TBase::params[FILTER_VOWEL_PARAM].value,
        TBase::params[FILTER_VOWEL_TRIM_PARAM].value);

    int iVowel = (int)std::floor(fVowel);

    assert(iVowel >= 0);
    if (iVowel >= formantTables.numVowels) {
        printf("formant overflow %f\n", fVowel);
        iVowel = formantTables.numVowels - 1;
    }

    for (int i = LED_A; i <= LED_U; ++i) {
        if (i == iVowel) {
            TBase::lights[i].value = ((i + 1) - fVowel) * 1;
            TBase::lights[i + 1].value = (fVowel - i) * 1;
        } else if (i != (iVowel + 1)) {
            TBase::lights[i].value = 0;
        }
    }

    const T bwMultiplier = scaleQ(
        TBase::inputs[FILTER_Q_CV_INPUT].getVoltage(0),
        TBase::params[FILTER_Q_PARAM].value,
        TBase::params[FILTER_Q_TRIM_PARAM].value);

    const T fPara = scaleFc(
        TBase::inputs[FILTER_FC_CV_INPUT].getVoltage(0),
        TBase::params[FILTER_FC_PARAM].value,
        TBase::params[FILTER_FC_TRIM_PARAM].value);
    // fNow -5..5, log

    const T brightness = scaleBrightness(
        TBase::inputs[FILTER_BRIGHTNESS_INPUT].getVoltage(0),
        TBase::params[FILTER_BRIGHTNESS_PARAM].value,
        TBase::params[FILTER_BRIGHTNESS_TRIM_PARAM].value);

    //   T filterMix = 0;
    for (int i = 0; i < numFilters; ++i) {
        const T fcLog = formantTables.getLogFrequency(model, i, fVowel);
        const T normalizedBw = bwMultiplier * formantTables.getNormalizedBandwidth(model, i, fVowel);

        // Get the filter gain from the table, but scale by BW to counteract the filters
        // gain that tracks Q
        T gainDB = formantTables.getGain(model, i, fVowel);

        // blend the table with full gain depending on brightness
        T modifiedGainDB = (1 - gainDB) * brightness + gainDB;

        // TODO: why is normalizedBW in this equation?
        m_gain[i] = LookupTable<T>::lookup(*db2GainLookup, modifiedGainDB) * normalizedBw;

        T fcFinalLog = fcLog + fPara;
        T fcFinal = LookupTable<T>::lookup(*expLookup, fcFinalLog);

        filterParams[i].setFreq(fcFinal * reciprocalSampleRate);
        filterParams[i].setNormalizedBandwidth(normalizedBw);
        //  filterMix += gain * StateVariableFilter<T>::run(input, filterStates[i], filterParams[i]);
    }
    // TBase::outputs[AUDIO_OUTPUT].value = 3 * filterMix;
}

template <class TBase>
inline void VocalFilter<TBase>::step() {
    if (--cycleCount < 0) {
        cycleCount = 3;
    }

    if (cycleCount == 0) {
        stepFilters();
    }

    T filterMix = 0;
    const T input = TBase::inputs[AUDIO_INPUT].getVoltage(0);
    for (int i = 0; i < numFilters; ++i) {
        filterMix += m_gain[i] * StateVariableFilter<T>::run(input, filterStates[i], filterParams[i]);
    }
    TBase::outputs[AUDIO_OUTPUT].setVoltage(3 * filterMix, 0);
}

template <class TBase>
int VocalDescription<TBase>::getNumParams() {
    return VocalFilter<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config VocalDescription<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case VocalFilter<TBase>::FILTER_VOWEL_PARAM:
            ret = {-5.0, 5.0, 0.0, "Vowel"};
            break;
        case VocalFilter<TBase>::FILTER_VOWEL_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "Vowel Trim"};
            break;
        case VocalFilter<TBase>::FILTER_FC_PARAM:
            ret = {-5.0, 5.0, 0.0, "Filter Cutoff"};
            break;
        case VocalFilter<TBase>::FILTER_FC_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "CV Trim"};
            break;
        case VocalFilter<TBase>::FILTER_Q_PARAM:
            ret = {-5.0, 5.0, 0.0, "Q / Resonance"};
            break;
        case VocalFilter<TBase>::FILTER_Q_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "Q CV Trim"};
            break;
        case VocalFilter<TBase>::FILTER_BRIGHTNESS_PARAM:
            ret = {-5.0, 5.0, 0.0, "Brightness"};
            break;
        case VocalFilter<TBase>::FILTER_BRIGHTNESS_TRIM_PARAM:
            ret = {-1.0, 1.0, 1.0, "Brightness CV Trim"};
            break;
        case VocalFilter<TBase>::FILTER_MODEL_SELECT_PARAM:
            ret = {0.0f, 4.0f, 2.0f, "Filter Model"};
            break;
        default:
            assert(false);
    }
    return ret;
}