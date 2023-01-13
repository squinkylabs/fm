
#pragma once

#include <random>

#include "BiquadFilter.h"
#include "BiquadParams.h"
#include "BiquadState.h"
#include "ButterworthFilterDesigner.h"
#include "Decimator.h"
#include "GraphicEq.h"
#include "IComposite.h"
#include "LowpassFilter.h"
#include "ObjectCache.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

/**
 * Noise generator feeding a graphic equalizer.
 * Calculated at very low sample rate, then re-sampled
 * up to audio rate.
 *
 * Below assumes 44k SR. TODO: other rates.
 *
 * We first design the EQ around bands of 100, 200, 400, 800,
 * 1600. EQ gets noise.
 *
 * Then output of EQ is re-sampled up by a factor of 100
 * to bring the first band down to 1hz.
 * or : decimation factor = 100 * (fs) / 44100.
 *
 * A butterworth lowpass then removes the re-sampling artifacts.
 * Otherwise these images bring in high frequencies that we
 * don't want.
 *
 * Cutoff for the filter can be as low as the top of the eq,
 * which is 3.2khz. 44k/3.2k is about 10,
 * so fc/fs can be 1/1000.
 *
 * or :   fc = (fs / 44100) / 1000;
 *
 * (had been using  fc/fs = float(1.0 / (44 * 100.0)));)
 *
 * Design for R = root freq (was 1 Hz, above)
 * EQ first band at E (was 100 Hz, above)
 *
 * Decimation divider = E / R
 *
 * Imaging filter fc = 3.2khz / decimation-divider
 * fc/fs = 3200 * (reciprocal sr) / decimation-divider.
 *
 * Experiment: let's use those values and compare to what we had been using.
 * result: not too far off.
 *
 * make a range/base control. map -5 to +5 into 1/10 Hz to 2 Hz rate. Can use regular
 * functions, since we won't calc that often.
 */

template <class TBase>
class LFNDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

template <class TBase>
class LFN : public TBase {
public:
    LFN(Module* module) : TBase(module) {
    }
    LFN() : TBase() {
    }

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<LFNDescription<TBase>>();
    }

    void setSampleTime(float time) {
        reciprocalSampleRate = time;
        updateLPF();
    }

    /**
    * re-calc everything that changes with sample
    * rate. Also everything that depends on baseFrequency.
    *
    * Only needs to be called once.
    */
    void init();

    enum ParamIds {
        EQ0_PARAM,
        EQ1_PARAM,
        EQ2_PARAM,
        EQ3_PARAM,
        EQ4_PARAM,
        FREQ_RANGE_PARAM,
        XLFN_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        EQ0_INPUT,
        EQ1_INPUT,
        EQ2_INPUT,
        EQ3_INPUT,
        EQ4_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    /**
     * Main processing entry point. Called every sample
     */
    void step() override;

    float getBaseFrequency() const {
        return baseFrequency;
    }

    bool isXLFN() const {
        return TBase::params[XLFN_PARAM].value > .5;
    }

    /**
     * This lets the butterworth get re-calculated on the UI thread.
     * We can't do it on the audio thread, because it calls malloc.
     */
    void pollForChangeOnUIThread();

    void setUniPolar(bool);

    bool uniPolar = false;

private:
    float reciprocalSampleRate = 0;


    ::Decimator decimator;

    GraphicEq2<5> geq;

    /**
     * Template type for butterworth reconstruction filter
     * Tried double for best low frequency performance. It's
     * probably overkill, but calculates plenty fast.
     */
    using TButter = double;
    BiquadParams<TButter, 2> lpfParams;
    BiquadState<TButter, 2> lpfState;

    /**
     * Frequency, in Hz, of the lowest band in the graphic EQ
     */
    float baseFrequency = 1;

    /**
    * The last value baked by the LPF filter calculation
    * done on the UI thread.
    */
    float lastBaseFrequencyParamValue = -100;
    float lastXLFMParamValue = -1;

    std::default_random_engine generator{57};
    std::normal_distribution<double> distribution{-1.0, 1.0};

    float noise() {
        return (float)distribution(generator);
    }

    int controlUpdateCount = 0;

    /**
     * Must be called after baseFrequency is updated.
     * re-calculates the butterworth lowpass.
     */
    void updateLPF();

    /**
     * scaling function for the range / base frequency knob
     * map knob range from .1 Hz to 2.0 Hz
     */
    std::function<double(double)> rangeFunc =
        {AudioMath::makeFunc_Exp(-5, 5, .1, 2)};

    /**
 * Audio taper for the EQ gains. Arbitrary max value selected
 * to give "good" output level.
 */
    AudioMath::SimpleScaleFun<float> gainScale =
        {AudioMath::makeSimpleScalerAudioTaper(0, 35)};
};

template <class TBase>
int LFNDescription<TBase>::getNumParams() {
    return LFN<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config LFNDescription<TBase>::getParamValue(int i) {
    const float gmin = -5;
    const float gmax = 5;
    const float gdef = 0;
    Config ret(0, 1, 0, "");
    switch (i) {
        case LFN<TBase>::EQ0_PARAM:
            ret = {gmin, gmax, gdef, "Low freq mix"};
            break;
        case LFN<TBase>::EQ1_PARAM:
            ret = {gmin, gmax, gdef, "Mid-low freq fix"};
            break;
        case LFN<TBase>::EQ2_PARAM:
            ret = {gmin, gmax, gdef, "Mid freq mix"};
            break;
        case LFN<TBase>::EQ3_PARAM:
            ret = {gmin, gmax, gdef, "Mid-high freq mix"};
            break;
        case LFN<TBase>::EQ4_PARAM:
            ret = {gmin, gmax, gdef, "High freq mix"};
            break;
        case LFN<TBase>::FREQ_RANGE_PARAM:
            ret = {-5, 5, 0, "Base frequency"};
            break;
        case LFN<TBase>::XLFN_PARAM:
            ret = {0, 1, 0, "Extra low frequency"};
            break;
        default:
            assert(false);
    }
    return ret;
}

template <class TBase>
inline void LFN<TBase>::pollForChangeOnUIThread() {
    if ((lastBaseFrequencyParamValue != TBase::params[FREQ_RANGE_PARAM].value) ||
        (lastXLFMParamValue != TBase::params[XLFN_PARAM].value)) {
        lastBaseFrequencyParamValue = TBase::params[FREQ_RANGE_PARAM].value;
        lastXLFMParamValue = TBase::params[XLFN_PARAM].value;

        baseFrequency = float(rangeFunc(lastBaseFrequencyParamValue));
        if (TBase::params[XLFN_PARAM].value > .5f) {
            baseFrequency /= 10.f;
        }

        updateLPF();  // now get the filters updated
    }
}

template <class TBase>
void LFN<TBase>::setUniPolar(bool uni) {
    uniPolar = uni;
}

template <class TBase>
inline void LFN<TBase>::init() {
    updateLPF();
}

template <class TBase>
inline void LFN<TBase>::updateLPF() {
    assert(reciprocalSampleRate > 0);
    // decimation must be 100hz (what our EQ is designed at)
    // divided by base.
    float decimationDivider = float(100.0 / baseFrequency);

    decimator.setDecimationRate(decimationDivider);

    // calculate lpFc ( Fc / sr)
    // Imaging filter fc = 3.2khz / decimation-divider
    // fc/fs = 3200 * (reciprocal sr) / decimation-divider.
    const float lpFc = 3200 * reciprocalSampleRate / decimationDivider;
    ButterworthFilterDesigner<TButter>::designThreePoleLowpass(
        lpfParams, lpFc);
}

template <class TBase>
inline void LFN<TBase>::step() {
    // Let's only check the inputs every 4 samples. Still plenty fast, but
    // get the CPU usage down really far.
    if (controlUpdateCount++ > 4) {
        controlUpdateCount = 0;
        const int numEqStages = geq.getNumStages();
        for (int i = 0; i < numEqStages; ++i) {
            auto paramNum = i + EQ0_PARAM;
            auto cvNum = i + EQ0_INPUT;
            const float gainParamKnob = TBase::params[paramNum].value;
            const float gainParamCV = TBase::inputs[cvNum].getVoltage(0);
            const float gain = gainScale(gainParamKnob, gainParamCV);
            geq.setGain(i, gain);
        }
    }

    bool needsData;
    TButter x = decimator.clock(needsData);
    x = BiquadFilter<TButter>::run(x, lpfState, lpfParams);
    if (needsData) {
        const float z = geq.run(noise());
        decimator.acceptData(z);
    }

    if (uniPolar)
        TBase::outputs[OUTPUT].setVoltage((float)x+5, 0); 
    else  
        TBase::outputs[OUTPUT].setVoltage((float)x, 0);
}
