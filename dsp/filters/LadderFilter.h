#pragma once

#include "AsymWaveShaper.h"
#include "IIRDecimator.h"
#include "IIRUpsampler.h"
#include "LookupTable.h"
#include "NonUniformLookupTable.h"
#include "ObjectCache.h"
#include "TrapezoidalLowpass.h"

#include <vector>
#include <string>

/**
 * Helper object to do the Edge lookup quickly
 */
class EdgeTables
{
public:
    EdgeTables();
    void lookup(bool is4PLP, float edge, float* gains);
private:
    LookupTableParams<float> tables4PLP[4];
    LookupTableParams<float> tablesOther[4];
};

inline EdgeTables::EdgeTables()
{

    for (int is4PLP = 0; is4PLP <= 1; ++is4PLP) {
        for (int stage = 0; stage < 4; ++stage) {
            LookupTableParams<float>* tables = (is4PLP) ? tables4PLP : tablesOther;
          
            LookupTable<float>::init(tables[stage], 20, 0, 1, [stage, is4PLP](double rawEdge) {
                float localStageGains[4];
                float k;
                if (rawEdge > .5) {
                    k = is4PLP ? .2f : .5f;
                } else {
                    k = is4PLP ? .6f : .8f;
                }

                const float edgeToUse = float(k + rawEdge * (1 - k) / .5f);
                AudioMath::distributeEvenly(localStageGains, 4, edgeToUse);
                return localStageGains[stage];
                });
        }
    }

}

inline void EdgeTables::lookup(bool is4PLP, float edge, float* gains)
{
    const LookupTableParams<float> * tables = (is4PLP) ? tables4PLP : tablesOther;
    for (int i = 0; i < 4; ++i) {
        float x = LookupTable<float>::lookup(tables[i], edge, false);
        gains[i] = x;
    }
}

/**
 * Moog-ish ladder filter, but lots of options
 */
template <typename T>
class LadderFilter
{
public:
    LadderFilter();
    enum class Types
    {
        _4PLP,
        _3PLP,
        _2PLP,
        _1PLP,
        _2PBP,
        _2HP1LP,
        _3HP1LP,
        _4PBP,
        _1LPNotch,
        _3AP1LP,
        _3PHP,
        _2PHP,
        _1PHP,
        _NOTCH,
        _PHASER,
        NUM_TYPES
    };

    enum class Voicing
    {
        Classic,
        Clip2,
        Fold,
        Fold2,
        Clean,
        NUM_VOICINGS
    };

    void run(T);
    T getOutput();

    /**
     * input range >0 to < .5
     */
    void setNormalizedFc(T);

    void setFeedback(T f);
    void setType(Types);
    void setVoicing(Voicing);
    void setGain(T);
    void setEdge(T);        // 0..1
    void setFreqSpread(T);
    void setBassMakeupGain(T);
    void setSlope(T);       // 0..3. only works in 4 pole
    void setVolume(T vol);  // 0..1

    float getLEDValue(int tapNumber) const;

    static std::vector<std::string> getTypeNames();
    static std::vector<std::string> getVoicingNames();

    // Only calibration routines will do this
    void disableQComp()
    {
        _disableQComp = true;
    }

    void _dump(const std::string&);
private:
    TrapezoidalLowpass<T> lpfs[4];
    EdgeTables edgeLookup;

    /**
     * Lowpass pole gain (base freq of filter converted)
     */
    T _g = .001f;

    /**
     * Individual _g values for each stage of filter.
     * normally all the same, but "caps" control separates them
     */
    T stageG[4] = {.001f, .001f, .001f, .001f};

   /**
    * Output mixer gain for each stage
    */
    T stageTaps[4] = {0, 0, 0, 1};

    T bassMakeupGain = 1;
    T mixedOutput = 0;
    T requestedFeedback = 0;
    T adjustedFeedback = 0;
    T gain = T(.3);
    T stageOutputs[4] = {0,0,0,0};
    T rawEdge = 0;
    T freqSpread = 0;
    T slope = 3;
    T volume = 0;

    float stageGain[4] = {1, 1, 1, 1};
    T stageFreqOffsets[4] = {1, 1, 1, 1};

    Types type = Types::_4PLP;
    Voicing voicing = Voicing::Classic;
    T lastNormalizedFc = T(.0001);
    T lastSlope = -1;
    T lastVolume = -1;
    T finalVolume = 1;

    bool bypassFirstStage = false;
    bool _disableQComp = false;

    std::shared_ptr<NonUniformLookupTableParams<T>> fs2gLookup = makeTrapFilter_Lookup<T>();
    std::shared_ptr<NonUniformLookupTableParams<T>> feedbackAdjust;
    std::shared_ptr<LookupTableParams<T>> tanhLookup = ObjectCache<T>::getTanh5();
    std::shared_ptr<LookupTableParams<T>> expLookup = ObjectCache<T>::getExp2();

    static const int oversampleRate = 4;
    IIRUpsampler<float> up;
    IIRDecimator<float> down;

    AsymWaveShaper shaper;
    T _lastInput = 0;

    void runBufferClassic(float* buffer, int);
    void runBufferClip2(float* buffer, int);
    void runBufferFold(float* buffer, int);
    void runBufferFold2(float* buffer, int);
    void runBufferClean(float* buffer, int);

    void updateFilter();
    void updateSlope();
    void updateFeedback();
    void updateStageGains();
    T processFeedback(T fcNorm, T feedback) const;
    void initQLookup();


   // void dump(const char* p);
    T getGfromNormFreq(T nf) const;
};

template <typename T>
LadderFilter<T>::LadderFilter()
{
    initQLookup();
    // fix at 4X oversample
    up.setup(oversampleRate);
    down.setup(oversampleRate);
}

template <typename T>
inline void LadderFilter<T>::_dump(const std::string& s)
{
#if 0
    printf("\ndump %s\n", s.c_str());
    printf("norm freq = %.2f, @44 = %.2f\n", lastNormalizedFc, lastNormalizedFc * 44100.0f);
    printf("feedback=%.2f, gain=%.2f edge=%.2f slope=%.2f\n", adjustedFeedback, gain, rawEdge, slope);
    printf("filt:_g=%f,  bgain=%.2f bypassFirst=%d\n", _g, bassMakeupGain, bypassFirstStage);
    for (int i = 0; i < 4; ++i) {
        printf("stage[%d] tap=%.2f, gain=%.2f freqoff=%.2f filter_G %f\n", i,
            stageTaps[i],
            stageGain[i],
            stageFreqOffsets[i],
            stageG[i]);
    }
    printf("volume = %f %f fina: %f\n", volume, lastVolume, finalVolume);
    printf("mixedOutput =%f bassMakeupGain=%f\n", mixedOutput, bassMakeupGain);
    printf("last input = %f\n", _lastInput);
    fflush(stdout);
#endif
}

template <typename T>
inline float LadderFilter<T>::getLEDValue(int tapNumber) const
{
    T ret =  (type == Types::_4PLP) ? stageTaps[tapNumber] : 0;  
    return float(ret);
}

template <typename T>
inline void  LadderFilter<T>::updateSlope()
{
    if (type != Types::_4PLP) {
        return;
    }
   
  //  printf("\n update slope %f\n", slope);
    int iSlope = (int) std::floor(slope);
    for (int i = 0; i < 4; ++i) {
        if (i == iSlope) {
            stageTaps[i] = ((i + 1) - slope) * 1;
            if (i < 3) {
                stageTaps[i + 1] = (slope - i) * 1;
            }
        } else if (i != (iSlope + 1)) {
            stageTaps[i] = 0;
        }
    }
}

template <typename T>
inline void LadderFilter<T>::setSlope(T _slope)
{
    if (slope == lastSlope) {
        return;
    }
    slope = _slope;
    slope = std::min(slope, T(3));
    slope = std::max(slope, T(0));
    updateSlope();
}

template <typename T>
inline void LadderFilter<T>::setVolume(T vol)
{
    if (lastVolume == vol) {
        return;
    }
    lastVolume = vol;
    finalVolume = 4 * vol * vol;
}
  


template <typename T>
inline T LadderFilter<T>::getGfromNormFreq(T nf) const
{
    nf *= (1.0 / oversampleRate);
    const T g2 = NonUniformLookupTable<T>::lookup(*fs2gLookup, nf);
    return g2;
}

template <typename T>
inline void LadderFilter<T>::setNormalizedFc(T input)
{
    if (input == lastNormalizedFc) {
        return;
    }

    // temp debugging stuff
    #if 0
    {
        float f = input * 44100;
        static float lastf = 0;
        float delta = std::abs<float>(f - lastf);
        if (delta > 500) {
            lastf = f;
        } else {
            return;
        }
    }
    #endif


    lastNormalizedFc = input;
    _g = getGfromNormFreq(input);
    updateFilter();
    updateFeedback();
    _dump("setnormfc");
}

template <typename T>
void LadderFilter<T>::setBassMakeupGain(T g)
{
    assert(g >= 1);     // must be
    assert(g < 10);     // tends to be
    if (g != bassMakeupGain) {
        bassMakeupGain = g;
        _dump("setBassG");
    }
}

template <typename T>
void LadderFilter<T>::setGain(T g)
{
    if (g != gain) {
        gain = g;
        _dump("set gain");
    }
}

template <typename T>
void LadderFilter<T>::setFreqSpread(T s)
{
    if (s == freqSpread) {
        return;
    }
    freqSpread = s;

    s *= .5;        // cut it down
    assert(s <= 1 && s >= 0);

    T s2 = s + 1;       // 1..2
    AudioMath::distributeEvenly(stageFreqOffsets, 4, s2);

    updateFilter();
}

template <typename T>
void LadderFilter<T>::updateFilter()
{
    for (int i = 0; i < 4; ++i) {
        stageG[i] = _g * stageFreqOffsets[i];
    }

    if (bypassFirstStage) {
        stageG[0] = getGfromNormFreq(T(.9));
    }

    _dump("update");
}

template <typename T>
void LadderFilter<T>::setEdge(T e)
{
    if (e == rawEdge) {
        return;
    }
    rawEdge = e;
    assert(e <= 1 && e >= 0);

    updateStageGains();   
    _dump("set edge");
}

template <typename T>
void LadderFilter<T>::updateStageGains()
{
    edgeLookup.lookup((type == Types::_4PLP), float(rawEdge), stageGain);
}

#if 0
template <typename T>
void LadderFilter<T>::updateStageGains()
{
    T k;
    if (rawEdge > .5) {
        k =  (type == Types::_4PLP) ? .2f : .5f;
    } else {
        k =  (type == Types::_4PLP) ? .6f : .8f;
      //  k = .6;
    }

    const T edgeToUse = k + rawEdge * (1 - k) / .5f;
    AudioMath::distributeEvenly(stageGain, 4, edgeToUse);
}
#endif

template <typename T>
void LadderFilter<T>::setVoicing(Voicing v)
{
    voicing = v;
}

template <typename T>
void LadderFilter<T>::setType(Types t)
{
    if (t == type)
        return;

    bypassFirstStage = false;
    type = t;
    switch (type) {
        case Types::_4PLP:
            stageTaps[3] = 1;
            stageTaps[2] = 0;
            stageTaps[1] = 0;
            stageTaps[0] = 0;
            break;
        case Types::_3PLP:
            stageTaps[3] = 0;
            stageTaps[2] = 1;
            stageTaps[1] = 0;
            stageTaps[0] = 0;
            break;
        case Types::_2PLP:
            stageTaps[3] = 0;
            stageTaps[2] = 0;
            stageTaps[1] = 1;
            stageTaps[0] = 0;
            break;
        case Types::_1PLP:
            stageTaps[3] = 0;
            stageTaps[2] = 0;
            stageTaps[1] = 0;
            stageTaps[0] = 1;
            break;
        case Types::_2PBP:
            stageTaps[3] = 0;
            stageTaps[2] = 0;
            stageTaps[1] = T(-.68) * 2;
            stageTaps[0] = T(.68) * 2;
            break;
        case Types::_2HP1LP:
            stageTaps[3] = 0;
            stageTaps[2] = T(.68) * 2;
            stageTaps[1] = T(-1.36) * 2;
            stageTaps[0] = T(.68) * 2;
            break;
        case Types::_3HP1LP:
            stageTaps[3] = T(-.68) * 4;
            stageTaps[2] = T(2.05) * 4;
            stageTaps[1] = T(-2.05) * 4;
            stageTaps[0] = T(.68) * 4;
            break;
        case Types::_3PHP:
            bypassFirstStage = true;
            stageTaps[3] = -1;
            stageTaps[2] = 3;
            stageTaps[1] = -3;
            stageTaps[0] = 1;
            break;
        case Types::_2PHP:
            bypassFirstStage = true;
            stageTaps[3] = 0;
            stageTaps[2] = 1;
            stageTaps[1] = -2;
            stageTaps[0] = 1;
            break;
        case Types::_1PHP:
            bypassFirstStage = true;
            stageTaps[3] = 0;
            stageTaps[2] = 0;
            stageTaps[1] = -1;
            stageTaps[0] = 1;
            break;
        case Types::_4PBP:
            stageTaps[3] = T(-.68) * 4;
            stageTaps[2] = T(1.36) * 4;
            stageTaps[1] = T(-.68) * 4;
            stageTaps[0] = 0;
            break;
        case Types::_1LPNotch:
            stageTaps[3] = 0;
            stageTaps[2] = T(1.36);
            stageTaps[1] = T(-1.36);
            stageTaps[0] = T(.68);
            break;
        case Types::_3AP1LP:
            stageTaps[3] = T(-2.73);
            stageTaps[2] = T(4.12);
            stageTaps[1] = T(-2.05);
            stageTaps[0] = T(.68);
            break;
        case Types::_NOTCH:
            bypassFirstStage = true;
            stageTaps[3] = 0;
            stageTaps[2] = 2;
            stageTaps[1] = -2;
            stageTaps[0] = 1;
            break;
        case Types::_PHASER:
            bypassFirstStage = true;
            stageTaps[3] = -4;
            stageTaps[2] = 6;
            stageTaps[1] = -3;
            stageTaps[0] = 1;
            break;
        default:
            assert(false);
    }
    updateFilter();
    updateSlope();
    updateStageGains();         // many filter types turn off the edge
    _dump("set type");
}

template <typename T>
inline T LadderFilter<T>::getOutput()
{
    return mixedOutput * 5 * bassMakeupGain;
}

template <typename T>
inline void LadderFilter<T>::setFeedback(T f)
{
    if (f == requestedFeedback) {
        return;
    }
    requestedFeedback = f;
    updateFeedback();
    _dump("feedback");
}

template <typename T>
inline void LadderFilter<T>::updateFeedback()
{
    if (!_disableQComp) {
#if 0
        double maxFeedback = 4;
        double fNorm = lastNormalizedFc;

        // Becuase this filter isn't zero delay, it can get unstable at high freq.
        // So limite the feedback up there.
        if (fNorm <= .002) {
            maxFeedback = 3.99;
        } else if (fNorm <= .008) {
            maxFeedback = 3.9;
        } else if (fNorm <= .032) {
            maxFeedback = 3.8;
        } else if (fNorm <= .064) {
            maxFeedback = 3.6;
        } else if (fNorm <= .128) {
            maxFeedback = 2.95;
        } else if (fNorm <= .25) {
            maxFeedback = 2.85;
        } else if (fNorm <= .3) {
          //  maxFeedback = 2.30;
          // experiment 2.5 too low.
          // 2.7 slightly low?
          // 2.8 ever so lightly high
          // 2.85 too high
            maxFeedback = 2.75;
        } else if (fNorm <= .4) {
            maxFeedback = 2.5;
        } else {
            maxFeedback = 2.3;
        }
        
        assert(requestedFeedback <= 4 && adjustedFeedback >= 0);

        adjustedFeedback = std::min(requestedFeedback, (T) maxFeedback);
        adjustedFeedback = std::max(adjustedFeedback, T(0));
#endif
        adjustedFeedback = processFeedback(lastNormalizedFc, requestedFeedback);
        assert(adjustedFeedback >= 0 && adjustedFeedback < 4);
       // printf("in updateFeedback, f= %.2f (%.2f) max = %.2f\n", fNorm * 44100, fNorm, maxFeedback);
       // printf("  reqF=%.2f adj = %.2f \n", requestedFeedback, adjustedFeedback);
    } else {
        adjustedFeedback = requestedFeedback;
    }
}




template <typename T>
inline void LadderFilter<T>::run(T input)
{
    _lastInput = input;
    input *= gain;
    float buffer[oversampleRate];
    up.process(buffer, (float) input);

    switch (voicing) {
        case Voicing::Classic:
            runBufferClassic(buffer, oversampleRate);
            break;
        case Voicing::Clip2:
            runBufferClip2(buffer, oversampleRate);
            break;
        case Voicing::Fold:
            runBufferFold(buffer, oversampleRate);
            break;
        case Voicing::Fold2:
            runBufferFold2(buffer, oversampleRate);
            break;
        case Voicing::Clean:
            runBufferClean(buffer, oversampleRate);
            break;
        default:
            assert(false);
    }
    mixedOutput = down.process(buffer) * finalVolume;
}

/**************************************************************************************
 *
 * A set of macros for (ugh) building up process functions for different distortion functions
 */
#define PROC_PREAMBLE(name) template <typename T> \
    inline void  LadderFilter<T>::name(float* buffer, int numSamples) { \
        for (int i = 0; i < numSamples; ++i) { \
            const T input = buffer[i]; \
            T temp = input - adjustedFeedback * stageOutputs[3]; \
            temp = std::max(T(-3), temp); \
            temp = std::min(T(3), temp);

#define PROC_END \
        temp = 0; \
        for (int i = 0; i < 4; ++i) { \
            temp += stageOutputs[i] * stageTaps[i]; \
        } \
        temp = std::max(T(-1.7), temp); \
        temp = std::min(T(1.7), temp); \
        buffer[i] = float(temp); \
     } \
}

#define ONETAP(func, index) \
    temp *= stageGain[index]; \
    func(); \
    temp = lpfs[index].run(temp, stageG[index]); \
    stageOutputs[index] = temp;

#define BODY( func0, func1, func2, func3) \
    ONETAP(func0, 0) \
    ONETAP(func1, 1) \
    ONETAP(func2, 2) \
    ONETAP(func3, 3)

#define TANH() temp = T(2) * LookupTable<T>::lookup(*tanhLookup.get(), T(.5) * temp, true)

#define CLIP() temp = std::max<T>(temp, -1.f); temp = std::min<T>(temp, 1.f)
#define CLIP_TOP()  temp = std::min<T>(temp, 1.f)
#define CLIP_BOTTOM()  temp = std::max<T>(temp, -1.f)

#define FOLD() temp = AudioMath::fold(float(temp))
#define FOLD_ATTEN() temp = AudioMath::fold(float(temp) * .5f)
//#define FOLD_ATTEN() temp = AudioMath::fold(float(temp) * 1)
#define FOLD_BOOST() temp = 2 * AudioMath::fold(float(temp))

#define FOLD_TOP() temp = (temp > 0) ? (T) AudioMath::fold(float(temp)) : temp
#define FOLD_BOTTOM() temp = (temp < 0) ? (T) AudioMath::fold(float(temp)) : temp
#define NOPROC()

//#define TRIODE1() temp = T(1.4) * shaper.lookup(float(temp * .4f), 7)
//#define TRIODE2_ATTEN() temp = T(.1) * shaper.lookup(float(temp), 5)
//#define TRIODE2() temp = 1.1 * shaper.lookup(float(temp * .4f), 5)
//#define TRIODE2g() temp = 3 * shaper.lookup(float(temp * .4f), 5)
//#define TRIODE2b() temp = 1.1 * -shaper.lookup(float(-temp * .4f), 5)

PROC_PREAMBLE(runBufferClassic)
BODY(TANH, TANH, TANH, TANH)
PROC_END

PROC_PREAMBLE(runBufferClip2)
BODY(CLIP_TOP, CLIP_BOTTOM, CLIP_TOP, CLIP_BOTTOM)
PROC_END

PROC_PREAMBLE(runBufferFold)
BODY(FOLD_ATTEN, FOLD, FOLD, FOLD)
PROC_END

PROC_PREAMBLE(runBufferFold2)
BODY(FOLD_TOP, FOLD_BOTTOM, FOLD_TOP, FOLD_BOTTOM)
PROC_END

#if 1
PROC_PREAMBLE(runBufferClean)
BODY(NOPROC, NOPROC, NOPROC, NOPROC)
PROC_END
#endif

template <typename T>
inline  std::vector<std::string> LadderFilter<T>::getTypeNames()
{
    return {
        "4P LP",
        "3P LP",
        "2P LP",
        "1P LP",
        "2P BP",
        "2HP+1LP",
        "3HP+1LP",
        "4P BP",
        "LP+Notch",
        "3AP+1LP",
        "3P HP",
        "2P HP",
        "1P HP",
        "Notch",
        "Phaser"
    };
}

template <typename T>
inline  std::vector<std::string> LadderFilter<T>::getVoicingNames()
{
    return {
        "Transistor",
        "Asym Clip",
        "Fold",
        "Asym Fold",
        "Clean"
    };
}
  
#if 0
template <typename T>
inline void LadderFilter<T>::runBufferClean(float* buffer, int numSamples)
{

    for (int i = 0; i < numSamples; ++i) {
        const T input = buffer[i];

        T temp = input - adjustedFeedback * stageOutputs[3];
        temp = std::max(T(-10), temp); 
        temp = std::min(T(10), temp);

        temp = lpfs[0].run(temp, stageG[0]);
        stageOutputs[0] = temp;

        temp = lpfs[1].run(temp, stageG[1]);
        stageOutputs[1] = temp;

        temp = lpfs[2].run(temp, stageG[2]);
        stageOutputs[2] = temp;

        temp = lpfs[3].run(temp, stageG[3]);

        temp = std::max(T(-10), temp);
        temp = std::min(T(10), temp);
        stageOutputs[3] = temp;

        if (type != Types::_4PLP) {
            temp = 0;
            for (int i = 0; i < 4; ++i) {
                temp += stageOutputs[i] * stageTaps[i];
            }
        }
        buffer[i] = float(temp);
    }
}
#endif

#if 0
template <typename T>
inline void LadderFilter<T>::runBufferClassic(T* buffer, int numSamples)
{
    const float k = 1.f / 5.f;
    const float j = 1.f / k;
    for (int i = 0; i < numSamples; ++i) {
        const T input = buffer[i];
        T temp = input - feedback * stageOutputs[3];
        temp *= stageGain[0];
        temp = j * LookupTable<float>::lookup(*tanhLookup.get(), k * temp, true);
        temp = lpfs[0].run(temp, stageG[0]);
        stageOutputs[0] = temp;

        temp *= stageGain[1];
        temp = j * LookupTable<float>::lookup(*tanhLookup.get(), k * temp, true);
        temp = lpfs[1].run(temp, stageG[1]);
        stageOutputs[1] = temp;

        temp *= stageGain[2];
        temp = j * LookupTable<float>::lookup(*tanhLookup.get(), k * temp, true);
        temp = lpfs[2].run(temp, stageG[2]);
        stageOutputs[2] = temp;

        temp *= stageGain[3];
        temp = j * LookupTable<float>::lookup(*tanhLookup.get(), k * temp, true);
        temp = lpfs[3].run(temp, stageG[3]);
        stageOutputs[3] = temp;

        if (type != Types::_4PLP) {
            temp = 0;
            for (int i = 0; i < 4; ++i) {
                temp += stageOutputs[i] * stageTaps[i];
            }
        }
        buffer[i] = temp;
    }

   // mixedOutput = temp;
   // return temp;
}
#endif


#if 0 // old manual way
template <typename T>
inline T LadderFilter<T>::processFeedback(T fcNorm, T feedback) const
{
    printf("doing old feedback\n");
   // double fNorm = lastNormalizedFc;
    double maxFeedback = 0;
       // Becuase this filter isn't zero delay, it can get unstable at high freq.
       // So limite the feedback up there.
    if (fcNorm <= .002) {
        maxFeedback = 3.99;
    } else if (fcNorm <= .008) {
        maxFeedback = 3.9;
    } else if (fcNorm <= .032) {
        maxFeedback = 3.8;
    } else if (fcNorm <= .064) {
        maxFeedback = 3.6;
    } else if (fcNorm <= .128) {
        maxFeedback = 2.95;
    } else if (fcNorm <= .25) {
        maxFeedback = 2.85;
    } else if (fcNorm <= .3) {
      //  maxFeedback = 2.30;
      // experiment 2.5 too low.
      // 2.7 slightly low?
      // 2.8 ever so lightly high
      // 2.85 too high
        maxFeedback = 2.75;
    } else if (fcNorm <= .4) {
        maxFeedback = 2.5;
    } else {
        maxFeedback = 2.3;
    }

    assert(requestedFeedback <= 4 && adjustedFeedback >= 0);
    T ret = std::min(feedback, (T) maxFeedback);
    ret = std::max(feedback, T(0));
    return ret;
}
#else

template <typename T>
inline T LadderFilter<T>::processFeedback(T fcNorm, T feedback) const
{
    const T x = feedback * T(.25);         // range 0..1
    const T y = x * (2 - x);            // still 0..1, but now a smooshed parabola
  //  const t z = 4 * y;                  // now 0..4 again, but still squared and inverted

    const T maxFeedback = NonUniformLookupTable<T>::lookup(*feedbackAdjust, fcNorm);

  //  printf("proc f. raw f = %f, x = %f, x = %f, will ret %f\n", feedback, x, y, y * maxFeedback);
    return y * maxFeedback;
}

#endif



template <typename T>
void LadderFilter<T>::initQLookup()
{
    std::shared_ptr<NonUniformLookupTableParams<T>> ret =
        std::make_shared<NonUniformLookupTableParams<T>>();

    /** this derived by CalQ with:
    const double desiredGain = 5;
    const double toleranceDb = 1;
    double toleranceDbInterp = 2;
    */
    NonUniformLookupTable<T>::addPoint(*ret, T(0.001134), T(3.531250));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.003933), T(3.625000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.006732), T(3.625000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.012330), T(3.625000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.023526), T(3.625000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.045918), T(3.625000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.090703), T(3.250000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.092971), T(3.250000));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.295918), T(2.687500));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.498866), T(2.390137));
    
    NonUniformLookupTable<T>::finalize(*ret);
    feedbackAdjust = ret;
}
#if 0 // gain=40 = too much
template <typename T>
void LadderFilter<T>::initQLookup()
{
    std::shared_ptr<NonUniformLookupTableParams<T>> ret = 
        std::make_shared<NonUniformLookupTableParams<T>>(); 
   
    NonUniformLookupTable<T>::addPoint(*ret, T(0.001134), T(3.964844));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.003933), T(3.966309));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.006732), T(3.953125));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.012330), T(3.923096));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.023526), T(3.858643));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.034722), T(3.796021));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.045918), T(3.736328));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.057115), T(3.677734));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.062713), T(3.649902));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.068311), T(3.622803));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.079507), T(3.569702));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.090703), T(3.517700));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.092971), T(3.507355));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.105655), T(3.450043));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.118339), T(3.394150));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.131023), T(3.345032));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.143707), T(3.297035));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.150050), T(3.273415));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.156392), T(3.250080));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.162734), T(3.226952));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.169076), T(3.204063));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.175418), T(3.181427));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.181760), T(3.159042));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.188102), T(3.136887));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.194444), T(3.114960));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.200787), T(3.093296));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.207129), T(3.071838));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.213471), T(3.050587));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.219813), T(3.029575));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.226155), T(3.008770));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.232497), T(2.988182));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.234083), T(2.983158));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.235668), T(2.978855));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.238839), T(2.970261));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.245181), T(2.953175));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.251524), T(2.936214));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.257866), T(2.919426));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.264208), T(2.902786));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.270550), T(2.886272));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.276892), T(2.869907));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.283234), T(2.853668));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.289576), T(2.837578));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.292747), T(2.829578));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.295918), T(2.821659));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.302260), T(2.805792));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.308603), T(2.790108));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.314945), T(2.774555));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.321287), T(2.759140));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.327629), T(2.743839));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.333971), T(2.728664));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.340313), T(2.713633));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.346655), T(2.698704));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.352997), T(2.683895));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.359340), T(2.669224));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.365682), T(2.654684));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.372024), T(2.640224));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.378366), T(2.625925));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.384708), T(2.611706));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.391050), T(2.597647));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.397392), T(2.583662));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.403734), T(2.569792));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.410077), T(2.556053));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.416419), T(2.542418));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.422761), T(2.528876));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.429103), T(2.515444));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.435445), T(2.502129));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.441787), T(2.488908));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.448129), T(2.475807));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.451300), T(2.469341));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.454471), T(2.463985));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.460813), T(2.453320));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.473498), T(2.432211));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.486182), T(2.411366));
    NonUniformLookupTable<T>::addPoint(*ret, T(0.498866), T(2.390795));

    NonUniformLookupTable<T>::finalize(*ret);
    feedbackAdjust = ret;
}
#endif


