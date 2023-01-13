
#pragma once

#include <assert.h>

#include <memory>

#include "Cmprsr.h"
#include "Divider.h"
#include "IComposite.h"
#include "LookupTableFactory.h"
#include "ObjectCache.h"
#include "SqPort.h"

#define _CMP_SCHEMA2
#ifdef _CMP_SCHEMA2
#define MIN_ATTACK .05
#define MAX_ATTACK 350
#define MIN_RELEASE 5
#define MAX_RELEASE 1600
#else
#define MIN_ATTACK .05
#define MAX_ATTACK 30
#define MIN_RELEASE 100
#define MAX_RELEASE 1600
#endif

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class CompressorDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

/**
 * new splines - rebase
 *
 * 16 channel bypassed:         4.06    
 * 16 channel mono 4:1 soft     90.2    
 * 16 channel linked 4:1 soft   64.1 
 * 16 channel linked limited    27.8  
 * 
 * 
 * sidechain:
 * 
 * 16 channel bypassed:         4.4
 * 16 channel mono 4:1 soft     90.7
 * 16 channel linked 4:1 soft   64.3
 * 16 channel linked limited    28.2
 *  
 * new lookup tables, etc..
 * 
 * 16 channel bypassed:         4.4
 * 16 channel mono 4:1 soft     90.4
 * 16 channel linked 4:1 soft   64
 * 16 channel linked limited    27.1
 * 
 * first round optimization:
 *
 * 1 ch lim: 4.6
 * 1 cha curve 
 * 16 cha lim:  20
 * 16 ch curve: 124
 * comp2 16: 145
 * comp2 16 linked : 151
 * comp2 16 bypassed: 148
 *  
 * SQ:
 * 1 ch lim: 4.6
 * 1 cha curve 
 * 16 cha lim:  20
 * 16 ch curve: 124
 * comp2 16: 145
 * comp2 16 linked : 151
 * comp2 16 bypassed: 148
 * 
 * Starting on comp2, here's where we are
 * 1 ch lim: 4.6
 * 1 cha curve 
 * 16 cha lim:  15.9
 * 16 ch curve: 121
 * comp2 16: 149
 * 
 * expand range to 40db
 * 1 ch lim: 5
 * 1 cha curve 12.8
 * 16 ch lim 15.9
 * 16 ch curve:120
 *
 * aver 14..15:
 * 1 ch lim: 2.4
 * 1 cha curve 12.7
 * 16 ch lim 16.4
 * 16 ch curve: 119 
 * 
 * after zero attack:
 * 1 ch lim: 4.9
 * 1 cha curve 13.3
 * 16 ch lim 16.3
 * 16 ch curve: 122
 * 16 ch lim no dist  18
 * 
 * only calc on change:
 * 1 ch lim: 4.02
 * 1 cha curve 11.9
 * 16 ch lim 10.8
 * 16 ch curve: 106
 * 16 ch lim no dist 12.0
 * 
 *  * After poll 16 -> 32
 * 1 ch lim: 9.2
 * 1 ch curve: 17.7
 * 16 ch lim: 16.1
 * 16 ch curve: 111
 * 16h ch lim no dist:  17.9
 * 
 * After lookups for knobs:
 * 1 ch lim: 14.8
 * 1 ch curve: 23.1
 * 16 ch lim: 21.6
 * 16 ch curve: 117.5
 * 16h ch lim no dist:  23.9
 * 
 * 
 * after obvious perf:
 * 
 * 1 ch lim: 17.4
 * 1 ch curve: 25.5
 * 16 ch lim: 24.1
 * 16 ch curve: 119
 * 16h ch lim no dist: 25.6
 *  
 * 1 ch lim: 21
 * 1 ch curve: 42.9
 * 16 ch lim: 27.1
 * 16 ch curve: 123
 * 16h ch lim no dist: 28.6
 */
template <class TBase>
class Compressor : public TBase {
public:
    Compressor(Module* module) : TBase(module) {
        compressorsL[0].setIsPolyCV(false);
        compressorsL[1].setIsPolyCV(false);
        compressorsL[2].setIsPolyCV(false);
        compressorsL[3].setIsPolyCV(false);
        compressorsR[0].setIsPolyCV(false);
        compressorsR[1].setIsPolyCV(false);
        compressorsR[2].setIsPolyCV(false);
        compressorsR[3].setIsPolyCV(false);
    }

    Compressor() : TBase() {
        compressorsL[0].setIsPolyCV(false);
        compressorsL[1].setIsPolyCV(false);
        compressorsL[2].setIsPolyCV(false);
        compressorsL[3].setIsPolyCV(false);
        compressorsR[0].setIsPolyCV(false);
        compressorsR[1].setIsPolyCV(false);
        compressorsR[2].setIsPolyCV(false);
        compressorsR[3].setIsPolyCV(false);
    }

    /**
    * re-calc everything that changes with sample
    * rate. Also everything that depends on baseFrequency.
    *
    * Only needs to be called once.
    */
    void init();

    enum ParamIds {
        ATTACK_PARAM,
        RELEASE_PARAM,
        THRESHOLD_PARAM,
        RATIO_PARAM,
        MAKEUPGAIN_PARAM,
        NOTBYPASS_PARAM,
        WETDRY_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        LAUDIO_INPUT,
        RAUDIO_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        LAUDIO_OUTPUT,
        RAUDIO_OUTPUT,
        DEBUG_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<CompressorDescription<TBase>>();
    }

    /**
     * Main processing entry point. Called every sample
     */
    //void step() override;
    void process(const typename TBase::ProcessArgs& args) override;

    void onSampleRateChange() override;

    float getGainReductionDb() const;
    static const std::vector<std::string>& ratios();
    static const std::vector<std::string>& ratiosLong();
    static std::function<double(double)> getSlowAttackFunction_1() {
        return AudioMath::makeFunc_Exp(0, 1, .05, 30);
    }

    static std::function<double(double)> getSlowReleaseFunction_1() {
        return AudioMath::makeFunc_Exp(0, 1, 100, 1600);
    }
    static std::function<double(double)> getSlowThresholdFunction() {
        return AudioMath::makeFunc_Exp(0, 10, .1, 10);
    }
#ifdef _CMP_SCHEMA2
    static std::function<double(double)> getSlowAttackFunction_2() {
        return AudioMath::makeFunc_Exp(0, 1, MIN_ATTACK, MAX_ATTACK);
    }
    static std::function<double(double)> getSlowAntiAttackFunction_2() {
        return AudioMath::makeFunc_InverseExp(0, 1, MIN_ATTACK, MAX_ATTACK);
    }

    static std::function<double(double)> getSlowReleaseFunction_2() {
        return AudioMath::makeFunc_Exp(0, 1, MIN_RELEASE, MAX_RELEASE);
    }
    static std::function<double(double)> getSlowAntiReleaseFunction_2() {
        return AudioMath::makeFunc_InverseExp(0, 1, MIN_RELEASE, MAX_RELEASE);
    }

   
#endif
    void onNewPatch(int schema);
   

private:
    Cmprsr compressorsL[4];
    Cmprsr compressorsR[4];
    void setupLimiter();
    void stepn();
    void pollAttackRelease();

    int numChannelsL_m = 0;
    int numBanksL_m = 0;
    int numChannelsR_m = 0;
    int numBanksR_m = 0;

    float_4 wetLevel = 0;
    float_4 dryLevel = 0;
    float_4 makeupGain_m = 1;
    Divider divn;

    // we could unify this stuff with the ui stuff, above.
    LookupTableParams<float> attackFunctionParams;
    LookupTableParams<float> releaseFunctionParams;
    LookupTableParams<float> thresholdFunctionParams;

    std::shared_ptr<LookupTableParams<float>> panL = ObjectCache<float>::getMixerPanL();
    std::shared_ptr<LookupTableParams<float>> panR = ObjectCache<float>::getMixerPanR();

    float lastRawMakeupGain = -1;
    float lastRawMix = -1;
    float lastRawA = -1;
    float lastRawR = -1;
    float lastThreshold = -1;
    float lastRatio = -1;
    int lastNumChannelsL = -1;
    int lastNumChannelsR = -1;
    bool bypassed = false;
    std::shared_ptr<IComposite> myDescription;
};

template <class TBase>
inline const std::vector<std::string>& Compressor<TBase>::ratios() {
    return Cmprsr::ratios();
}

template <class TBase>
inline const std::vector<std::string>& Compressor<TBase>::ratiosLong() {
    return Cmprsr::ratiosLong();
}

template <class TBase>
inline void Compressor<TBase>::init() {
    setupLimiter();
    divn.setup(32, [this]() {
        this->stepn();
    });

    LookupTableFactory<float>::makeGenericExpTaper(64, attackFunctionParams, 0, 1, .05, 30);
    LookupTableFactory<float>::makeGenericExpTaper(64, releaseFunctionParams, 0, 1, 100, 1600);
    LookupTableFactory<float>::makeGenericExpTaper(64, thresholdFunctionParams, 0, 10, .1, 10);
    myDescription = Compressor<TBase>::getDescription();
}


template <class TBase>
inline void Compressor<TBase>::onNewPatch(int knownSchema) {
#ifdef _CMP_SCHEMA2
    bool needsUpdate = false;
    if (knownSchema == 0) {
        // If we don't know the schema, then it is either a 1.0 patch loaded, or
        // it is a new instances. But if it's a new instance it won't have the same
        // values as the old 1.0 defaults. So the only way this heuristic fails is if
        // it were truly a 1.0 patch, but happened to have all defaults, and A and R edited
        // to exactly the 2.0 defaults - not likely.
        needsUpdate = true;
        for (int i = 0; i < myDescription->getNumParams(); ++i) {
            const float def = myDescription->getParamValue(i).def;
            const float cur = Compressor<TBase>::params[i].value;
            const bool same = (def == cur);
            //SQINFO("p=%d def=%f cur=%f same = %d", i, def, cur, same);
            if (!same) {
                needsUpdate = false;
            }
        }
    }
    if (needsUpdate) {
        float storedAttackParam = Compressor<TBase>::params[ATTACK_PARAM].value;
        float attackTime = getSlowAttackFunction_1()(storedAttackParam);
        float newAttackParam = getSlowAntiAttackFunction_2()(attackTime);
        Compressor<TBase>::params[ATTACK_PARAM].value = newAttackParam;

        float storedReleaseParam = Compressor<TBase>::params[ATTACK_PARAM].value;
        float releaseTime = getSlowReleaseFunction_1()(storedReleaseParam);
        float newReleaseParam = getSlowAntiReleaseFunction_2()(releaseTime);
        Compressor<TBase>::params[ATTACK_PARAM].value = newReleaseParam;
    }
#endif
}


template <class TBase>
inline float Compressor<TBase>::getGainReductionDb() const {
    float_4 minGain_4 = 1;
    if (bypassed) {
        return 0;
    }

    for (int bank = 0; bank < numBanksL_m; ++bank) {
        minGain_4 = SimdBlocks::min(minGain_4, compressorsL[bank].getGain());
    }
    for (int bank = 0; bank < numBanksR_m; ++bank) {
        minGain_4 = SimdBlocks::min(minGain_4, compressorsR[bank].getGain());
    }

    float minGain = minGain_4[0];
    minGain = std::min(minGain, minGain_4[1]);
    minGain = std::min(minGain, minGain_4[2]);
    minGain = std::min(minGain, minGain_4[3]);
    auto r = AudioMath::db(minGain);
    return -r;
}

template <class TBase>
inline void Compressor<TBase>::stepn() {
    SqInput& inPortL = TBase::inputs[LAUDIO_INPUT];
    SqOutput& outPortL = TBase::outputs[LAUDIO_OUTPUT];
    SqInput& inPortR = TBase::inputs[RAUDIO_INPUT];
    SqOutput& outPortR = TBase::outputs[RAUDIO_OUTPUT];

    numChannelsL_m = inPortL.channels;
    numChannelsR_m = inPortR.channels;
    outPortL.setChannels(numChannelsL_m);
    outPortR.setChannels(numChannelsR_m);

    numBanksL_m = (numChannelsL_m / 4) + ((numChannelsL_m % 4) ? 1 : 0);
    numBanksR_m = (numChannelsR_m / 4) + ((numChannelsR_m % 4) ? 1 : 0);
    // printf("\n****** after stepm banks = %d numch=%d\n", numBanks_m, numChannels_m);

    pollAttackRelease();

    const float rawWetDry = Compressor<TBase>::params[WETDRY_PARAM].value;
    if (rawWetDry != lastRawMix) {
        lastRawMix = rawWetDry;
        wetLevel = LookupTable<float>::lookup(*panR, rawWetDry, true);
        dryLevel = LookupTable<float>::lookup(*panL, rawWetDry, true);
        wetLevel *= wetLevel;
        dryLevel *= dryLevel;
    }

    const float rawMakeupGain = Compressor<TBase>::params[MAKEUPGAIN_PARAM].value;
    if (lastRawMakeupGain != rawMakeupGain) {
        lastRawMakeupGain = rawMakeupGain;
        makeupGain_m = float(AudioMath::gainFromDb(rawMakeupGain));
    }

    const float threshold = LookupTable<float>::lookup(thresholdFunctionParams, Compressor<TBase>::params[THRESHOLD_PARAM].value);
    const float rawRatio = Compressor<TBase>::params[RATIO_PARAM].value;
    if (lastThreshold != threshold ||
        lastRatio != rawRatio ||
        lastNumChannelsL != numChannelsL_m ||
        lastNumChannelsR != numChannelsR_m

    ) {
        lastThreshold = threshold;
        lastRatio = rawRatio;
        lastNumChannelsL = numChannelsL_m;
        lastNumChannelsR = numChannelsR_m;
        Cmprsr::Ratios ratio = Cmprsr::Ratios(int(std::round(rawRatio)));
        for (int i = 0; i < 4; ++i) {
            compressorsL[i].setThreshold(threshold);
            compressorsR[i].setThreshold(threshold);
            compressorsL[i].setCurve(ratio);
            compressorsR[i].setCurve(ratio);

            if (i < numBanksL_m) {
                const int baseChannel = i * 4;
                const int chanThisBankL = std::min(4, numChannelsL_m - baseChannel);
                compressorsL[i].setNumChannels(chanThisBankL);
            }
            if (i < numBanksR_m) {
                const int baseChannel = i * 4;
                const int chanThisBankR = std::min(4, numChannelsR_m - baseChannel);
                compressorsR[i].setNumChannels(chanThisBankR);
            }
        }
    }

    bypassed = !bool(std::round(Compressor<TBase>::params[NOTBYPASS_PARAM].value));
    // printf("notbypass value = %f, bypassed = %d\n", Compressor<TBase>::params[NOTBYPASS_PARAM].value, bypassed); fflush(stdout);
}

template <class TBase>
inline void Compressor<TBase>::pollAttackRelease() {
    const float rawAttack = Compressor<TBase>::params[ATTACK_PARAM].value;
    const float rawRelease = Compressor<TBase>::params[RELEASE_PARAM].value;

    if (rawAttack != lastRawA || rawRelease != lastRawR) {
        lastRawA = rawAttack;
        lastRawR = rawRelease;

        const float attack = LookupTable<float>::lookup(attackFunctionParams, rawAttack);
        const float release = LookupTable<float>::lookup(releaseFunctionParams, rawRelease);

        for (int i = 0; i < 4; ++i) {
            compressorsL[i].setTimes(attack, release, TBase::engineGetSampleTime());
            compressorsR[i].setTimes(attack, release, TBase::engineGetSampleTime());
        }
    }
}

template <class TBase>
inline void Compressor<TBase>::process(const typename TBase::ProcessArgs& args) {
    divn.step();

    SqInput& inPortL = TBase::inputs[LAUDIO_INPUT];
    SqOutput& outPortL = TBase::outputs[LAUDIO_OUTPUT];
    SqInput& inPortR = TBase::inputs[RAUDIO_INPUT];
    SqOutput& outPortR = TBase::outputs[RAUDIO_OUTPUT];

    if (bypassed) {
        for (int bank = 0; bank < numBanksL_m; ++bank) {
            const int baseChannel = bank * 4;
            const float_4 input = inPortL.getPolyVoltageSimd<float_4>(baseChannel);
            outPortL.setVoltageSimd(input, baseChannel);
        }
        for (int bank = 0; bank < numBanksR_m; ++bank) {
            const int baseChannel = bank * 4;
            const float_4 input = inPortR.getPolyVoltageSimd<float_4>(baseChannel);
            outPortR.setVoltageSimd(input, baseChannel);
        }
        return;
    }

    for (int bank = 0; bank < numBanksL_m; ++bank) {
        const int baseChannel = bank * 4;
        const float_4 input = inPortL.getPolyVoltageSimd<float_4>(baseChannel);
        const float_4 wetOutput = compressorsL[bank].step(input) * makeupGain_m;
        const float_4 mixedOutput = wetOutput * wetLevel + input * dryLevel;

        outPortL.setVoltageSimd(mixedOutput, baseChannel);
    }
    for (int bank = 0; bank < numBanksR_m; ++bank) {
        const int baseChannel = bank * 4;
        const float_4 input = inPortR.getPolyVoltageSimd<float_4>(baseChannel);
        const float_4 wetOutput = compressorsR[bank].step(input) * makeupGain_m;
        const float_4 mixedOutput = wetOutput * wetLevel + input * dryLevel;

        outPortR.setVoltageSimd(mixedOutput, baseChannel);
    }
}

// TODO: do we still need this old init function? combine with other?
template <class TBase>
inline void Compressor<TBase>::setupLimiter() {
    for (int i = 0; i < 4; ++i) {
        compressorsL[i].setTimes(1, 100, TBase::engineGetSampleTime());
        compressorsR[i].setTimes(1, 100, TBase::engineGetSampleTime());
    }
}

template <class TBase>
inline void Compressor<TBase>::onSampleRateChange() {
    setupLimiter();
}

template <class TBase>
int CompressorDescription<TBase>::getNumParams() {
    return Compressor<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config CompressorDescription<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {

#ifdef _CMP_SCHEMA2
        // these new values calculated with "maths" to give same times as before
        case Compressor<TBase>::ATTACK_PARAM:
            ret = {0, 1, .58336f, "Attack time"};
            break;
            // 200ms
        case Compressor<TBase>::RELEASE_PARAM:
            ret = {0, 1, .6395f, "Release time"};
            break;

#else
        case Compressor<TBase>::ATTACK_PARAM:
            // .8073 too low .8075 too much
            ret = {0, 1, .8074f, "Attack time"};
            break;
        case Compressor<TBase>::RELEASE_PARAM:
            ret = {0, 1, .25f, "Release time"};
            break;
#endif
        case Compressor<TBase>::THRESHOLD_PARAM:
            ret = {0, 10, 10, "Threshold"};
            break;
        case Compressor<TBase>::RATIO_PARAM:
            ret = {0, 8, 3, "Compression ratio"};
            break;
        case Compressor<TBase>::MAKEUPGAIN_PARAM:
            ret = {0, 40, 0, "Makeup gain"};
            break;
        case Compressor<TBase>::NOTBYPASS_PARAM:
            ret = {0, 1, 1, "Effect bypass"};
            break;
        case Compressor<TBase>::WETDRY_PARAM:
            ret = {-1, 1, 1, "dry/wet mix"};
            break;
        default:
            assert(false);
    }
    return ret;
}
