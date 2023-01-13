
#pragma once

#include <assert.h>

#include <memory>

#include "CommChannels.h"
#include "Divider.h"
#include "IComposite.h"
#include "MixHelper.h"
#include "MultiLag.h"
#include "ObjectCache.h"
#include "SqMath.h"
#include "mixpolyhelper.h"

template <class TBase>
class Mix4Description : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

#ifndef _CLAMP
#define _CLAMP
namespace std {
inline float clamp(float v, float lo, float hi) {
    assert(lo < hi);
    return std::min(hi, std::max(v, lo));
}
}  // namespace std
#endif

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack

/**
 Perf: 10.4 before new features
    13.5 with all the features

 */
template <class TBase>
class Mix4 : public TBase {
public:
    template <typename Q>
    friend class MixHelper;
    template <typename Q>
    friend class MixPolyHelper;

    Mix4(::rack::engine::Module* module) : TBase(module) {
    }
    Mix4() : TBase() {
    }
    static const int numChannels = 4;
    static const int numGroups = 4;
    /**
    * Only needs to be called once.
    */
    void init();
    void setupFilters();

    enum ParamIds {
        GAIN0_PARAM,
        GAIN1_PARAM,
        GAIN2_PARAM,
        GAIN3_PARAM,

        PAN0_PARAM,
        PAN1_PARAM,
        PAN2_PARAM,
        PAN3_PARAM,

        MUTE0_PARAM,
        MUTE1_PARAM,
        MUTE2_PARAM,
        MUTE3_PARAM,

        SOLO0_PARAM,
        SOLO1_PARAM,
        SOLO2_PARAM,
        SOLO3_PARAM,

        ALL_CHANNELS_OFF_PARAM,  // when > .05, acts as if all channels muted.

        SEND0_PARAM,
        SEND1_PARAM,
        SEND2_PARAM,
        SEND3_PARAM,

        SENDb0_PARAM,
        SENDb1_PARAM,
        SENDb2_PARAM,
        SENDb3_PARAM,

        PRE_FADERa_PARAM,  // 0 = post, 1 = pre
        PRE_FADERb_PARAM,

        MUTE0_STATE_PARAM,
        MUTE1_STATE_PARAM,
        MUTE2_STATE_PARAM,
        MUTE3_STATE_PARAM,

        CV_MUTE_TOGGLE,

        NUM_PARAMS
    };

    enum InputIds {
        AUDIO0_INPUT,
        AUDIO1_INPUT,
        AUDIO2_INPUT,
        AUDIO3_INPUT,

        LEVEL0_INPUT,
        LEVEL1_INPUT,
        LEVEL2_INPUT,
        LEVEL3_INPUT,

        PAN0_INPUT,
        PAN1_INPUT,
        PAN2_INPUT,
        PAN3_INPUT,

        MUTE0_INPUT,
        MUTE1_INPUT,
        MUTE2_INPUT,
        MUTE3_INPUT,

        NUM_INPUTS
    };

    enum OutputIds {
        CHANNEL0_OUTPUT,
        CHANNEL1_OUTPUT,
        CHANNEL2_OUTPUT,
        CHANNEL3_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        SOLO0_LIGHT,
        SOLO1_LIGHT,
        SOLO2_LIGHT,
        SOLO3_LIGHT,

        MUTE0_LIGHT,
        MUTE1_LIGHT,
        MUTE2_LIGHT,
        MUTE3_LIGHT,
        NUM_LIGHTS
    };

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<Mix4Description<TBase>>();
    }

    void setExpansionInputs(const float*);
    void setExpansionOutputs(float*);

    /**
     * Main processing entry point. Called every sample
     */
    void step() override;
    void onSampleRateChange() override;

    void stepn(int steps);

    // These are all calculated in stepn from the
    // contents of filteredCV

    float buf_channelSendGainsALeft[numChannels] = {0};
    float buf_channelSendGainsARight[numChannels] = {0};
    float buf_channelSendGainsBLeft[numChannels] = {0};
    float buf_channelSendGainsBRight[numChannels] = {0};

    void _disableAntiPop();

private:
    Divider divider;

    /**
     *      0..3 for smoothed input gain * channel mute
     *      4..7 for left pan
     *      8..11 for right pan
     *      12..15 for mute
     */

    static const int cvOffsetGain = 0;
    static const int cvOffsetPanLeft = 4;
    static const int cvOffsetPanRight = 8;
    static const int cvOffsetMute = 12;
    MultiLPF<16> filteredCV;

    std::shared_ptr<LookupTableParams<float>> panL = ObjectCache<float>::getMixerPanL();
    std::shared_ptr<LookupTableParams<float>> panR = ObjectCache<float>::getMixerPanR();

    const float* expansionInputs = nullptr;
    float* expansionOutputs = nullptr;

    MixHelper<Mix4<TBase>> helper;
    MixPolyHelper<Mix4<TBase>> polyHelper;

    std::shared_ptr<LookupTableParams<float>> taperLookupParam = ObjectCache<float>::getAudioTaper18();
};

template <class TBase>
inline void Mix4<TBase>::stepn(int div) {
    float unbufferedCV[cvOffsetMute + 4] = {0};

    const bool moduleIsMuted = TBase::params[ALL_CHANNELS_OFF_PARAM].value > .5f;
    const bool AisPreFader = TBase::params[PRE_FADERa_PARAM].value > .5;
    const bool BisPreFader = TBase::params[PRE_FADERb_PARAM].value > .5;

    helper.procMixInputs(this);
    polyHelper.updatePolyphony(this);

    // If the is an external solo, then mute all channels
    bool anySolo = false;
    for (int i = 0; i < numChannels; ++i) {
        if (TBase::params[i + SOLO0_PARAM].value > .5f) {
            anySolo = true;
            break;
        }
    }

    for (int i = 0; i < numChannels; ++i) {
        float channelGain = 0;

        // First let's round up the channel volume
        {
            const float rawSlider = TBase::params[i + GAIN0_PARAM].value;
            const float slider = LookupTable<float>::lookup(*taperLookupParam, rawSlider);

            const float rawCV = TBase::inputs[i + LEVEL0_INPUT].isConnected() ? TBase::inputs[i + LEVEL0_INPUT].getVoltage(0) : 10.f;
            const float cv = std::clamp(
                rawCV / 10.0f,
                0.0f,
                1.0f);
            channelGain = slider * cv;
        }

        // now round up the mutes
        float rawMuteValue = 0;  // assume muted
        if (moduleIsMuted) {
        } else if (anySolo) {
            // If any channels in this module are soloed, then
            // mute any channels that aren't soled
            rawMuteValue = TBase::params[i + SOLO0_PARAM].value;
        } else {
            // The pre-calculated state in :params[i + MUTE0_STATE_PARAM] will
            // be applicable if no solo
            rawMuteValue = TBase::params[i + MUTE0_STATE_PARAM].value > .5 ? 0.f : 1.f;
        }
        channelGain *= rawMuteValue;

        // now the raw channel gains are all computed
        unbufferedCV[cvOffsetGain + i] = channelGain;
        unbufferedCV[cvOffsetMute + i] = rawMuteValue;

        // now do the pan calculation
        {
            const float balance = TBase::params[i + PAN0_PARAM].value;
            const float cv = TBase::inputs[i + PAN0_INPUT].getVoltage(0);
            const float panValue = std::clamp(balance + cv / 5, -1, 1);
            unbufferedCV[cvOffsetPanLeft + i] = LookupTable<float>::lookup(*panL, panValue) * channelGain;
            unbufferedCV[cvOffsetPanRight + i] = LookupTable<float>::lookup(*panR, panValue) * channelGain;
        }

        // TODO: precalc all the send gains
        {
            const float muteValue = filteredCV.get(cvOffsetMute + i);
            const float sliderA = TBase::params[i + SEND0_PARAM].value;
            const float sliderB = TBase::params[i + SENDb0_PARAM].value;

            // TODO: we can do some main volume work ahead of time, just like the sends here
            if (!AisPreFader) {
                // post faster, gain sees mutes, faders,  pan, and send level
                buf_channelSendGainsALeft[i] = filteredCV.get(i + cvOffsetPanLeft) * sliderA;
                buf_channelSendGainsARight[i] = filteredCV.get(i + cvOffsetPanRight) * sliderA;
            } else {
                // pre-fader fader, gain sees mutes and send only
                buf_channelSendGainsALeft[i] = muteValue * sliderA * (1.f / sqrt(2.f));
                buf_channelSendGainsARight[i] = muteValue * sliderA * (1.f / sqrt(2.f));
            }

            if (!BisPreFader) {
                // post faster, gain sees mutes, faders,  pan, and send level
                buf_channelSendGainsBLeft[i] = filteredCV.get(i + cvOffsetPanLeft) * sliderB;
                buf_channelSendGainsBRight[i] = filteredCV.get(i + cvOffsetPanRight) * sliderB;
            } else {
                // pref fader, gain sees mutes and send only
                buf_channelSendGainsBLeft[i] = muteValue * sliderB * (1.f / sqrt(2.f));
                buf_channelSendGainsBRight[i] = muteValue * sliderB * (1.f / sqrt(2.f));
            }
        }

        // refresh the solo lights
        {
            const float soloValue = TBase::params[i + SOLO0_PARAM].value;
            TBase::lights[i + SOLO0_LIGHT].value = (soloValue > .5f) ? 10.f : 0.f;
        }
    }
    filteredCV.step(unbufferedCV);
}

template <class TBase>
inline void Mix4<TBase>::init() {
    const int divRate = 4;
    divider.setup(divRate, [this, divRate] {
        this->stepn(divRate);
    });
    setupFilters();
}

template <class TBase>
inline void Mix4<TBase>::onSampleRateChange() {
    setupFilters();
}

template <class TBase>
inline void Mix4<TBase>::_disableAntiPop() {
    filteredCV.setCutoff(0.49f);  // set it super fast
}

template <class TBase>
inline void Mix4<TBase>::setupFilters() {
    const float x = TBase::engineGetSampleTime() * 44100.f / 100.f;
    filteredCV.setCutoff(x);
}

template <class TBase>
inline void Mix4<TBase>::step() {
    divider.step();

    float left = 0, right = 0;  // these variables will be summed up over all channels
    float lSend = 0, rSend = 0;
    float lSendb = 0, rSendb = 0;

    if (expansionInputs) {
        left = expansionInputs[0];
        right = expansionInputs[1];
        lSend = expansionInputs[2];
        rSend = expansionInputs[3];
        lSendb = expansionInputs[4];
        rSendb = expansionInputs[5];
    }

    for (int i = 0; i < numChannels; ++i) {
        const float channelInput = polyHelper.getNormalizedInputSum(this, i);

        // sum the channel output to the masters
        left += channelInput * filteredCV.get(i + cvOffsetPanLeft);
        right += channelInput * filteredCV.get(i + cvOffsetPanRight);

        // TODO: aux sends
        lSend += channelInput * buf_channelSendGainsALeft[i];
        lSendb += channelInput * buf_channelSendGainsBLeft[i];
        rSend += channelInput * buf_channelSendGainsARight[i];
        rSendb += channelInput * buf_channelSendGainsBRight[i];

        TBase::outputs[i + CHANNEL0_OUTPUT].setVoltage(channelInput * filteredCV.get(i + cvOffsetGain), 0);
    }

    // output the buses to the expansion port
    if (expansionOutputs) {
        expansionOutputs[0] = left;
        expansionOutputs[1] = right;
        expansionOutputs[2] = lSend;
        expansionOutputs[3] = rSend;
        expansionOutputs[4] = lSendb;
        expansionOutputs[5] = rSendb;
    }
}

template <class TBase>
inline void Mix4<TBase>::setExpansionInputs(const float* p) {
    expansionInputs = p;
}

template <class TBase>
inline void Mix4<TBase>::setExpansionOutputs(float* p) {
    expansionOutputs = p;
}

template <class TBase>
int Mix4Description<TBase>::getNumParams() {
    return Mix4<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config Mix4Description<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case Mix4<TBase>::GAIN0_PARAM:
            ret = {0, 1, 0, "Level 1"};
            break;
        case Mix4<TBase>::GAIN1_PARAM:
            ret = {0, 1, 0, "Level 2"};
            break;
        case Mix4<TBase>::GAIN2_PARAM:
            ret = {0, 1, 0, "Level 3"};
            break;
        case Mix4<TBase>::GAIN3_PARAM:
            ret = {0, 1, 0, "Level 4"};
            break;

        case Mix4<TBase>::PAN0_PARAM:
            ret = {-1.0f, 1.0f, 0.0f, "Pan 1"};
            break;
        case Mix4<TBase>::PAN1_PARAM:
            ret = {-1.0f, 1.0f, 0.0f, "Pan 2"};
            break;
        case Mix4<TBase>::PAN2_PARAM:
            ret = {-1.0f, 1.0f, 0.0f, "Pan 3"};
            break;
        case Mix4<TBase>::PAN3_PARAM:
            ret = {-1.0f, 1.0f, 0.0f, "Pan 4"};
            break;

        case Mix4<TBase>::MUTE0_PARAM:
            ret = {0, 1.0f, 0, "Mute  1"};
            break;
        case Mix4<TBase>::MUTE1_PARAM:
            ret = {0, 1.0f, 0, "Mute  2"};
            break;
        case Mix4<TBase>::MUTE2_PARAM:
            ret = {0, 1.0f, 0, "Mute  3"};
            break;
        case Mix4<TBase>::MUTE3_PARAM:
            ret = {0, 1.0f, 0, "Mute  4"};
            break;
        case Mix4<TBase>::SOLO0_PARAM:
            ret = {0, 1.0f, 0, "Solo  1"};
            break;
        case Mix4<TBase>::SOLO1_PARAM:
            ret = {0, 1.0f, 0, "Solo  2"};
            break;
        case Mix4<TBase>::SOLO2_PARAM:
            ret = {0, 1.0f, 0, "Solo  3"};
            break;
        case Mix4<TBase>::SOLO3_PARAM:
            ret = {0, 1.0f, 0, "Solo  4"};
            break;
        case Mix4<TBase>::SEND0_PARAM:
            ret = {0, 1.0f, 0, "Send 1"};
            break;
        case Mix4<TBase>::SEND1_PARAM:
            ret = {0, 1.0f, 0, "Send 2"};
            break;
        case Mix4<TBase>::SEND2_PARAM:
            ret = {0, 1.0f, 0, "Send 3"};
            break;
        case Mix4<TBase>::SEND3_PARAM:
            ret = {0, 1.0f, 0, "Send 4"};
            break;
        case Mix4<TBase>::SENDb0_PARAM:
            ret = {0, 1.0f, 0, "Send 1b"};
            break;
        case Mix4<TBase>::SENDb1_PARAM:
            ret = {0, 1.0f, 0, "Send 2b"};
            break;
        case Mix4<TBase>::SENDb2_PARAM:
            ret = {0, 1.0f, 0, "Send 3b"};
            break;
        case Mix4<TBase>::SENDb3_PARAM:
            ret = {0, 1.0f, 0, "Send 4b"};
            break;
        case Mix4<TBase>::ALL_CHANNELS_OFF_PARAM:
            ret = {0, 1.0f, 0, "All Off"};
            break;
        case Mix4<TBase>::PRE_FADERa_PARAM:  // 0 = post, 1 = pre
            ret = {0, 1.0f, 0, "Pre Fader A"};
            break;
        case Mix4<TBase>::PRE_FADERb_PARAM:
            ret = {0, 1.0f, 0, "Pre Fader B"};
            break;
        case Mix4<TBase>::MUTE0_STATE_PARAM:
            ret = {0, 1, 0, "MSX0"};  // not user visible
            break;
        case Mix4<TBase>::MUTE1_STATE_PARAM:
            ret = {0, 1, 0, "MSX1"};
            break;
        case Mix4<TBase>::MUTE2_STATE_PARAM:
            ret = {0, 1, 0, "MSX2"};
            break;
        case Mix4<TBase>::MUTE3_STATE_PARAM:
            ret = {0, 1, 0, "MSX3"};
            break;
        case Mix4<TBase>::CV_MUTE_TOGGLE:
            ret = {0, 1, 0, "VCTM"};
            break;
        default:
            assert(false);
    }
    return ret;
}
