
#pragma once
#include "AudioMath.h"
#include "FFT.h"
#include "FFTData.h"

#include <functional>
#include <vector>


/**
 * some utils for modules, some for testing...
 */
class FFTUtils
{
public:
    class Stats {
    public:
        double averagePhaseJump = 0;
    };
    using Generator = std::function<double()>;
    static void getStats(Stats&, const FFTDataCpx& a, const FFTDataCpx& b, const FFTDataCpx& c);
    static void getStats2(Stats&, const FFTDataCpx& a, const FFTDataCpx& b, const FFTDataCpx& c);


    static std::vector<FFTDataRealPtr> generateData(int numSamples, int frameSize, Generator generator);
    static std::vector<FFTDataCpxPtr> generateFFTs(int numSamples, int frameSize, Generator generator);
 
    #if 0 // move to test geneartors
    static Generator makeSinGenerator(double periodInSamples, double initialPhase);

    /**
     * xxx Discontinuity 0..1, where 1 is two pi
     * NO - discontinuity is in radians
     */
    static Generator makeSinGeneratorPhaseJump(double periodInSamples, double initialPhase, int delay, double discontinuityRadians);
    #endif


};

/**
 * this helper should know everything about phase that goes from -pi to pi
 * let's let the include pi, but not -pi
 */
class PhaseAngleUtil
{
public:
    static bool isNormalized(double phase) {
        return phase > -AudioMath::Pi && phase <= AudioMath::Pi;
    }
    static double normalize(double phase, bool print = false) {
        if (print) printf("normalize (%f) called\n", phase);
        while(phase <= -AudioMath::Pi) {
            phase += AudioMath::_2Pi;
        }
        while(phase > AudioMath::Pi) {
            phase -= AudioMath::_2Pi;
        }
        assert(isNormalized(phase));
        if (print) printf("norm returning %f\n", phase);
        return phase;
    }
    static double distance(double to, double from, bool print = false) {
        if (print) printf("distance(%f, %f) called\n", to, from);
        double ret = normalize(to - from, print);
        if (print) printf("distance(%f, %f) returning %.2f\n", to, from, ret);
        return ret;
    }

};
