
#include "asserts.h"
#include "MultiLag.h"
#include "LowpassFilter.h"
#include "Decimator.h"
#include "Analyzer.h"
#include "asserts.h"
#include "LFN.h"
#include "TestComposite.h"
#include "TrapezoidalLowpass.h"


template<typename T>
static void test0()
{
    LowpassFilterState<T> state;
    LowpassFilterParams<T> params;
    LowpassFilter<T>::setCutoff(params, T(.1));
    LowpassFilter<T>::setCutoffHP(params, T(.1));
    LowpassFilter<T>::run(0, state, params);
    
}

const float sampleRate = 44100;

// return first = fc, second = slope
template<typename T>
static std::pair<T, T> getLowpassStats(std::function<float(float)> filter, T FcExpected)
{
    const int numSamples = 16 * 1024;
    FFTDataCpx response(numSamples);
    Analyzer::getFreqResponse(response, filter);

   /**
    * 0 = low freq bin #
    * 1 = peak bin #
    * 2 = high bin#
    * dbAtten (typically -3
    */
    auto x = Analyzer::getMaxAndShoulders(response, -3);

   
    auto lowBin = std::get<0>(x);
    auto peakBin = std::get<1>(x);
    auto highBin = std::get<2>(x);
    const int maxx = numSamples / 2;
   

    const T cutoff = (T) FFT::bin2Freq(std::get<2>(x), sampleRate, numSamples);

    // Is the peak at zero? i.e. no ripple.
    if (std::get<1>(x) == 0) {
        assertEQ(std::get<0>(x), -1);   // no LF shoulder
    } else {
        // Some higher order filters have a tinny bit of ripple
        float peakMag = std::abs(response.get(std::get<1>(x)));
        float zeroMag = std::abs(response.get(0));
        assertClose(peakMag / zeroMag, 1, .0001);
    }

    //assertClose(cutoff, Fc, 3);    // 3db down at Fc

    T slope = Analyzer::getSlopeLowpass(response, (float) FcExpected * 2, sampleRate);
   // assertClose(slope, expectedSlope, 1);          // to get accurate we nee
    return std::make_pair(cutoff, slope);
}

template<typename T>
static void doLowpassTest(std::function<float(float)> filter, T Fc, T expectedSlope)
{
    auto stats = getLowpassStats<T>(filter, Fc);
    assertClose(stats.first, Fc, 3);    // 3db down at Fc

    //double slope = Analyzer::getSlope(response, (float) Fc * 2, sampleRate);
    assertClose(stats.second, expectedSlope, 1);          // to get accurate we need to go to higher freq, etc... this is fine
}

template<typename T>
static void testBasicLowpass100()
{
    const float Fc = 100;

    LowpassFilterState<T> state;
    LowpassFilterParams<T> params;
    LowpassFilter<T>::setCutoff(params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        auto y = LowpassFilter<T>::run(x, state, params);
        return float(y);
    };
    doLowpassTest<T>(filter, Fc, -6);
}

template<typename T>
static void testSuperBasicHP()
{
    const float Fc = 1;
    LowpassFilterState<T> state;
    LowpassFilterParams<T> params;
    LowpassFilter<T>::setCutoff(params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        auto y = LowpassFilter<T>::run(x, state, params);
        float ret = float(x - y);
       // printf("filter (%f) ret %f\n", x, ret);
        return ret;
    };
    printf("l = %f k=%f\n", params.l, params.k);

    float y0 = filter(0);
    assertEQ(y0, 0);
    float y = filter(1);
    assertClose(y, 1, .01);

    filter(1);
    filter(1);
    filter(1);
    filter(1);


#if 1
    for (int i = 0; i < 100000; ++i) {
        float y2 = filter(1);
      //  printf("in loop, y2=%f, y=%f i =%d\n", y2, y, i); fflush(stdout);
        assert(y2 < y);
      //  assert(i < 20);
        y = y2;
    }
#endif

    assertClose(y, 0, .0001);
}



template<typename T>
static void testTrap100()
{
    const float Fc = 100;

    const float g = .00717f;

    TrapezoidalLowpass<T> lpf;
    const T g2 = TrapezoidalLowpass<T>::legacyCalcG2(g);

    std::function<float(float)> filter = [&lpf, g2](float x) {
        auto y = lpf.run(x, g2);
        return float(y);
    };
    doLowpassTest<T>(filter, Fc, -6);
}

static void calibrateTrap()
{
    for (double g = 1.5; g > .000001; g /= 2) {
        TrapezoidalLowpass<double> lpf;
        double g2 = TrapezoidalLowpass<double>::legacyCalcG2(g);

        std::function<float(float)> filter = [&lpf, g2](float x) {
            auto y = lpf.run(x, g2);
            return float(y);
        };
        auto stats = getLowpassStats<double>(filter, 100);
        //printf("g = %f, g2 = %f, fC = %f\n", g, g2, stats.first);
        printf("f/fs = %f, g2=%f\n", stats.first / sampleRate, g2);
    }

}

template<typename T>
static void testTwoPoleButterworth100()
{
    const float Fc = 100;
    BiquadParams<T, 1> params;
    BiquadState<T, 1> state;

    ButterworthFilterDesigner<T>::designTwoPoleLowpass(
        params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        x = (float) BiquadFilter<T>::run(x, state, params);
        return x;
    };
    doLowpassTest<T>(filter, Fc, -12);
}

template<typename T>
static void testThreePoleButterworth100()
{
    const float Fc = 100;
    BiquadParams<T, 2> params;
    BiquadState<T, 2> state;

    ButterworthFilterDesigner<T>::designThreePoleLowpass(
        params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        x = (float) BiquadFilter<T>::run(x, state, params);
        return x;
    };
    doLowpassTest<T>(filter, Fc, -18);
}

template<typename T>
static void testFourPoleButterworth100()
{
    const float Fc = 100;
    BiquadParams<T, 2> params;
    BiquadState<T, 2> state;

    ButterworthFilterDesigner<T>::designFourPoleLowpass(
        params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        x = (float) BiquadFilter<T>::run(x, state, params);
        return x;
    };
    doLowpassTest<T>(filter, Fc, -24);
}

template<typename T>
static void testFivePoleButterworth100()
{
    const float Fc = 100;
    BiquadParams<T, 3> params;
    BiquadState<T, 3> state;

    ButterworthFilterDesigner<T>::designFivePoleLowpass(
        params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        x = (float) BiquadFilter<T>::run(x, state, params);
        return x;
    };
    doLowpassTest<T>(filter, Fc, -30);
}

template<typename T>
static void testSixPoleButterworth100()
{
    const float Fc = 100;
    BiquadParams<T, 3> params;
    BiquadState<T, 3> state;

    ButterworthFilterDesigner<T>::designSixPoleLowpass(
        params, Fc / sampleRate);

    std::function<float(float)> filter = [&state, &params](float x) {
        x = (float) BiquadFilter<T>::run(x, state, params);
        return x;
    };
    doLowpassTest<T>(filter, Fc, -36);
}

static double measureFeedthrough(
    std::function<float(float)> filter,
    std::function<void(void)> changeFunc)
{
    // run silence through it, settle
    float x = 0;
    for (int i = 0; i < 1000; ++i) {
        x = filter(0);      
    }
    for (int i = 0; i < 20; ++i) {
        x = filter(1);
    }

    // now back to zero
    for (int i = 0; i < 1000; ++i) {
        x = filter(0);
    }
   
    changeFunc();
    float jump = filter(1);
    return jump;
}

static void testCVFeedthroughSimple()
{
    const float Fc = 100;
    using T = float;

   
    LowpassFilterState<T> state;
    LowpassFilterParams<T> params;
    LowpassFilter<T>::setCutoff(params, Fc / sampleRate);

    auto filter = [&state, &params](float x) {
        auto y = LowpassFilter<T>::run(x, state, params);
        return float(y);
    };
    printf("here goes\n");

    auto change = [&params] {
        const float Fc = 400;
        LowpassFilter<T>::setCutoff(params, Fc / sampleRate);
    };

    const double jump = measureFeedthrough(filter, change);
    assertGT(jump, .05);
}


static void testCVFeedthroughTrap()
{
    const float Fc = 100;
    using T = float;

    TrapezoidalLowpass<T> lpf;
    float g2 = lpf.legacyCalcG2(.00717f);

    auto filter = [&lpf, g2](float x) {
        auto y = lpf.run(x, g2);
        return float(y);
    };

    auto change = [&lpf, &g2] {
        const float Fc = 400;
        g2 = lpf.legacyCalcG2(.0287f);
    };

    const double jump = measureFeedthrough(filter, change);
    assertLT(jump, .28);
}

static void testTrapDCf()
{
    TrapezoidalLowpass<float> lpf;
    const float g2 = lpf.legacyCalcG2(.00717f);
    const float x = 3.41f;
    float y;

    const int times = 1000;
    for (int i = 0; i < times; ++i) {
        y = lpf.run(x, g2);
    }
    // 100 iter: error = .8
    // 1000 and up:  .000016
    //printf("testTrapDCf: repeat %d: output = %f, error = %f\n", times,  y, x - y);

    assertClose(x - y, 0, .00002);
}

static void testTrapDCd()
{
    TrapezoidalLowpass<double> lpf;
    const double g2 = lpf.legacyCalcG2(.00717f);
    const double x = 3.41;
    double y;
    const int times = 10000;
    for (int i = 0; i < times; ++i) {
        y = lpf.run(x, g2);
    }
    // 100 iter: error = .8
    // 1000 .000002
    // 10000: 0
   // printf("testTrapDCd: rep %d output = %f, error = %f\n", times, y, x - y);
    const double error = x - y;
    const double exp = 4.1e-14;
    assertClose(error, 0, exp);
}
/******************************************************************************************************/

#if 0 // not ready for prime time
template<typename T>
static void doEllipticTest(std::function<float(float)> filter, T Fc, T expectedSlope)
{
    const int numSamples = 16 * 1024;
   //const int numSamples = 1024;
    FFTDataCpx response(numSamples);
    Analyzer::getFreqResponse(response, filter);

    auto x = Analyzer::getMaxAndShoulders(response, -3);

    const float cutoff = FFT::bin2Freq(std::get<2>(x), sampleRate, numSamples);


    // Some higher order filters have a tinny bit of ripple
    float peakMag = std::abs(response.get(std::get<1>(x)));
    float zeroMag = std::abs(response.get(0));
    printf("mag at zero hz = %f, peak mag = %f, -3db at %f\n ",
        zeroMag, peakMag, cutoff);

    Analyzer::getAndPrintFeatures(response, 1, sampleRate);
    for (int i = 0; i < numSamples; ++i) {

    }
    //assertClose(peakMag / zeroMag, 1, .0001);


  //  assertClose(cutoff, Fc, 3);    // 3db down at Fc


    double slope = Analyzer::getSlope(response, (float) Fc * 2, sampleRate);
  //  assertClose(slope, expectedSlope, 1);          // to get accurate we need to go to higher freq, etc... this is fine

}

template<typename T>
static void testElip1()
{
    const float Fc = 100;
    BiquadParams<T, 4> params;
    BiquadState<T, 4> state;

    T rippleDb = 3;
    T attenDb = 100000;
    T ripple = (T) AudioMath::gainFromDb(1);
    ButterworthFilterDesigner<T>::designEightPoleElliptic(params, Fc / sampleRate, rippleDb, attenDb);

    std::function<float(float)> filter = [&state, &params](float x) {
        x = (float) BiquadFilter<T>::run(x, state, params);
        return x;
    };
    doEllipticTest<T>(filter, Fc, -36);

}
#endif

template<typename T>
void _testLowpassFilter()
{
    test0<T>();
    testBasicLowpass100<T>();
   
    // testBasicLowpassHP100<T>();
    testTrap100<T>();
    testTwoPoleButterworth100<T>();
    testThreePoleButterworth100<T>();
    testFourPoleButterworth100<T>();
    testFivePoleButterworth100<T>();
    testSixPoleButterworth100<T>();
}


/*********************************************************************************
**
**              also test decimator here
**
**********************************************************************************/

static void decimate0()
{
    Decimator d;
    d.setDecimationRate(2);
    d.acceptData(5);
    bool b = true;
    auto x = d.clock(b);
    assert(!b);
    assertEQ(x, 5);
}


static void decimate1()
{
    Decimator d;
    d.setDecimationRate(2);
    d.acceptData(5);
    bool b = true;
    auto x = d.clock(b);
    assert(!b);
    assertEQ(x, 5);

    x = d.clock(b);
    assert(b);
    assertEQ(x, 5);
}

static void testCVFeedthrough()
{
    testCVFeedthroughSimple();
    testCVFeedthroughTrap();
}

void testLowpassFilter()
{
  //  _testLowpassFilter<float>();
  //  _testLowpassFilter<double>();

    testSuperBasicHP<double>();
  //  testCVFeedthrough();
  
  //  testTrapDCf();
  //  testTrapDCd();
  //  decimate0();
   // decimate1();
}

/*********************************************************************************
**
**              also test MultiLag here
**
**********************************************************************************/

template <class T>
static void _testMultiLag0(int size)
{
    T l;
    for (int i = 0; i < size; ++i) {
        assertClose(l.get(i), 0, .0001);
    }
}

static void testMultiLag0()
{
    _testMultiLag0<MultiLPF<8>>(8);
    _testMultiLag0<MultiLag<8>>(8);
}


// test that output eventually matches input
template <class T>
static void _testMultiLag1(int size, T& dut)
{

    float input[100];
    for (int n = 0; n < size; ++n) {
        assert(n < 100);
        for (int i = 0; i < size; ++i) {
            input[i] = (i == n) ? 1.f : 0.f;
        }
        for (int i = 0; i < 10; ++i) {
            dut.step(input);
        }
        for (int i = 0; i < 8; ++i) {
            const float expected = (i == n) ? 1.f : 0.f;
            assertClose(dut.get(i), expected, .0001);
        }
    }
}

static void testMultiLag1()
{
    MultiLPF<8> f;
    f.setCutoff(.4f);
    _testMultiLag1(8, f);

    MultiLag<8> l;
    l.setAttack(.4f);
    l.setRelease(.4f);
    _testMultiLag1(8, l);
}


// test response
template <class T>
static void _testMultiLag2(int size, T& dut, float f)
{
    for (int n = 0; n < size; ++n) {
        float input[100] = {0};
        std::function<float(float)> filter = [&input, n, &dut](float x) {

            input[n] = x;
            dut.step(input);
            auto y = dut.get(n);
            return float(y);
        };
        doLowpassTest<float>(filter, f, -6);
    }
}

static void testMultiLag2()
{
    MultiLPF<8> f;
    float fC = 10.f;
    f.setCutoff(fC / sampleRate);
    _testMultiLag2(8, f, fC);

    MultiLag<8> l;
    l.setAttack(fC / sampleRate);
    l.setRelease(fC / sampleRate);
    _testMultiLag2(8, l, fC);
}

static void testMultiLagDisable()
{
    MultiLag<8> f;
    float fC = 10.f;
    f.setAttack(fC / sampleRate);
    f.setRelease(fC / sampleRate);

    // when enabled, should lag
    const float buffer[8] = {1,1,1,1,1,1,1,1};
    f.step(buffer);
    for (int i = 0; i < 8; ++i) {
        assertNE(f.get(0), 1);
    }

    f.setEnable(false);
    f.step(buffer);
    for (int i = 0; i < 8; ++i) {
        assertEQ(f.get(0), 1);
    }

}


template <typename T>
static void tlp()
{
    auto params = makeLPFilterL_Lookup<T>();
    assert(params->size() == 6);
}

static void testLowpassLookup()
{
    tlp<float>();
    tlp<double>();
}

static void testLowpassLookup2()
{
    auto params = makeLPFilterL_Lookup<float>();

    for (float f = .1f; f < 100; f *= 1.1f) {
        float fs = f / 44100;
        float l = LowpassFilter<float>::computeLfromFs(fs);
        float l1 = NonUniformLookupTable<float>::lookup(*params, fs);

        float r = l1 / l;
        assertClose(r, 1, .01);
    }

    assert(true);
}

static void testDirectLookup()
{
    auto p = makeLPFDirectFilterLookup<float>(1.f / 44100.f);
    assert(p->numBins_i > 0);
    makeLPFDirectFilterLookup<double>(1.f / 44100.f);
}

static void testDirectLookup2()
{
    auto p = makeLPFDirectFilterLookup<float>(1.f / 44100.f);

    float y = LookupTable<float>::lookup(*p, 0);
    assertEQ(y, 1 - .4f);

    y = LookupTable<float>::lookup(*p, 1);
    assertEQ(y, 1 - .00002f);
}

void testMultiLag()
{
    testLowpassLookup();
    testLowpassLookup2();
    testDirectLookup();
    testDirectLookup2();

    testMultiLag0();
    testMultiLag1();
    testMultiLag2();
    testMultiLagDisable();
}