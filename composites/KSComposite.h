#pragma once

#include "FunVCO3.h"

template <class TBase>
class KSComposite : public TBase {
public:
    KSComposite() {
        init();
    }
    KSComposite(Module* module) : TBase(module) {
        init();
    }

    enum ParamIds {
        OCTAVE_PARAM,
        SEMI_PARAM,
        FINE_PARAM,
        FM_PARAM,
        PW_PARAM,
        PWM_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        PITCH_INPUT,
        FM_INPUT,
        SYNC_INPUT,
        PW_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        SIN_OUTPUT,
        TRI_OUTPUT,
        SAW_OUTPUT,
        SQR_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    void step() override;
    void init() {
        oscillator.init();
    }
#if 0
    void setSampleRate(float rate)
    {
        oscillator.sampleTime = 1.f / rate;
    }
#endif

private:
    KSOscillator<16, 16> oscillator;
};

template <class TBase>
inline void KSComposite<TBase>::step() {
    // TODO: tune these

    /* from functional
    float pitchFine = 3.0f * quadraticBipolar(TBase::params[FINE_PARAM].value);
    float pitchCv = 12.0f * TBase::inputs[PITCH_INPUT].value;
    if (TBase::inputs[FM_INPUT].active) {
        pitchCv += quadraticBipolar(TBase::params[FM_PARAM].value) * 12.0f * TBase::inputs[FM_INPUT].value;
    }
    */

    // const float cv = getInput(osc, CV1_INPUT, CV2_INPUT, CV3_INPUT);
    const float cv = TBase::inputs[PITCH_INPUT].getVoltage(0);
    const float finePitch = TBase::params[FINE_PARAM].value / 12.0f;
    const float semiPitch = TBase::params[SEMI_PARAM].value / 12.0f;
    // const float fm = getInput(osc, FM1_INPUT, FM2_INPUT, FM3_INPUT);

    float pitch = 1.0f + roundf(TBase::params[OCTAVE_PARAM].value) +
                  semiPitch +
                  finePitch;
    pitch += cv;

    oscillator.setPitch(pitch);

    oscillator.setPulseWidth(TBase::params[PW_PARAM].value + TBase::params[PWM_PARAM].value * TBase::inputs[PW_INPUT].getVoltage(0) / 10.0f);
    oscillator.syncEnabled = TBase::inputs[SYNC_INPUT].isConnected();

    oscillator.sawEnabled = TBase::outputs[SAW_OUTPUT].isConnected();
    oscillator.sinEnabled = TBase::outputs[SIN_OUTPUT].isConnected();
    oscillator.sqEnabled = TBase::outputs[SQR_OUTPUT].isConnected();
    oscillator.triEnabled = TBase::outputs[TRI_OUTPUT].isConnected();

    oscillator.process(TBase::engineGetSampleTime(), TBase::inputs[SYNC_INPUT].getVoltage(0), TBase::engineGetSampleTime());
    // Set output
    if (TBase::outputs[SIN_OUTPUT].isConnected())
        TBase::outputs[SIN_OUTPUT].setVoltage(5.0f * oscillator.sin(), 0);
    if (TBase::outputs[TRI_OUTPUT].isConnected())
        TBase::outputs[TRI_OUTPUT].setVoltage(5.0f * oscillator.tri(), 0);
    if (TBase::outputs[SAW_OUTPUT].isConnected())
        TBase::outputs[SAW_OUTPUT].setVoltage(5.0f * oscillator.saw(), 0);
    if (TBase::outputs[SQR_OUTPUT].isConnected())
        TBase::outputs[SQR_OUTPUT].setVoltage(5.0f * oscillator.sqr(), 0);
}