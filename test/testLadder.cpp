
#include "asserts.h"
#include "TestComposite.h"
#include "Filt.h"
#include "LadderFilter.h"
#include "PeakDetector.h"
#include "TestComposite.h"


static void testLadderZero()
{
    LadderFilter<float> f;
    f.setNormalizedFc(.01f);
    for (int i = 0; i < 20; ++i) {
        f.run(0);
        float x = f.getOutput();
        assertEQ(x, 0);
    }
}


static void testLadderZero2()
{
    LadderFilter<double> filter;

    filter.setNormalizedFc(.1);
    filter.setVoicing(LadderFilter<double>::Voicing::Clean);
    filter.disableQComp();
    filter.setFeedback(3.9);

    for (int i = 0; i < 20; ++i) {
        filter.run(0);
        double x = filter.getOutput();
        assertEQ(x, 0);
    }
}

static void testLadderNotZero()
{
    LadderFilter<float> f;
    f.setNormalizedFc(.1f);
    for (int i = 0; i < 20; ++i) {
        f.run(1);
        float x = f.getOutput();
        assertGT(x, 0);
    }
}

template <typename T>
static void setupFilter(LadderFilter<T>& f)
{
    f.setType(LadderFilter<T>::Types::_3PHP);
    f.setNormalizedFc(1 / T(40));
    f.setBassMakeupGain(1);
    f.setVoicing(LadderFilter<T>::Voicing::Clean);
    f.setFeedback(0);

    f.setEdge(.5);
   
    f.setFeedback(0);
    f.setFreqSpread(0);
    f.setGain(1);
}

// test that highpass decays to zero
static void testLadderDCf(int repeats)
{
    LadderFilter<float> f;
    setupFilter(f);

    const float x = 3.78f;
   // const int repeats = 10;
    float y = 100;
    for (int i = 0; i < repeats; ++i) {
        f.run(x);
        y = f.getOutput();
      
    }
    y = f.getOutput();
    //printf("testLadderDCf rep = %d out=%e\n", repeats, y);
    if (repeats >= 1000) assertClose(y, 0, 3.3e-5);
}

static void testLadderDCd(int repeats)
{
    LadderFilter<double> f;
    setupFilter(f);

    const double x = 3.78;
    double y = 100;
    for (int i = 0; i < repeats; ++i) {
        f.run(x);

    }
    y = f.getOutput();
    //printf("testLadderDCd rep = %d out=%e\n", repeats, y);
   // .02 after 100+
    // 0 after 1000

    // better inits, 0 after 1000
    if (repeats >= 1000) assertClose(y, 0, 3e-6);
}

static void testLadderTypes()
{
    for (int i = 0; i < (int) LadderFilter<double>::Types::NUM_TYPES; ++i) {
        LadderFilter<double> f;
        f.setType(LadderFilter<double>::Types(i));
    }
}

// if not 4P LP, will all be zero
static void testLED0()
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_1PHP);
    f.setSlope(2);
    for (int i = 0; i < 4; ++i) {
        assertEQ(f.getLEDValue(i), 0);
    }
}

static void testLED1()
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_4PLP);
    f.setSlope(0);

    assertClose(f.getLEDValue(0), 1, .01);
    assertEQ(f.getLEDValue(1), 0);
    assertEQ(f.getLEDValue(2), 0);
    assertEQ(f.getLEDValue(3), 0); 
}

static void testLED2()
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_4PLP);
    f.setSlope(1);

    assertClose(f.getLEDValue(1), 1, .01);
    assertEQ(f.getLEDValue(0), 0);
    assertEQ(f.getLEDValue(2), 0);
    assertEQ(f.getLEDValue(3), 0);
}

static void testLED3()
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_4PLP);
    f.setSlope(2);

    assertClose(f.getLEDValue(2), 1, .01);
    assertEQ(f.getLEDValue(0), 0);
    assertEQ(f.getLEDValue(1), 0);
    assertEQ(f.getLEDValue(3), 0);
}

static void testLED4()
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_4PLP);
    f.setSlope(3);

    assertClose(f.getLEDValue(3), 1, .01);
    assertEQ(f.getLEDValue(0), 0);
    assertEQ(f.getLEDValue(2), 0);
    assertEQ(f.getLEDValue(1), 0);
}

static void testLED5()
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_4PLP);
    f.setSlope(2.5);

    assertClose(f.getLEDValue(3), .5, .01);
    assertEQ(f.getLEDValue(0), 0);
    assertClose(f.getLEDValue(2), .5, .01);
    assertEQ(f.getLEDValue(1), 0);
}

static void testFilt()
{
    Filt<TestComposite> f;

    assert(!f.getVoicingNames().empty());
    assert(!f.getTypeNames().empty());

    assertEQ(f.getVoicingNames().size(), (int)LadderFilter<float>::Voicing::NUM_VOICINGS);
    assertEQ(f.getTypeNames().size(), (int) LadderFilter<float>::Types::NUM_TYPES);

}

static void testFilt2()
{
    Filt<TestComposite> f;
    auto x = f.getTypeNames();
    for (auto t : x) {
        assert(!t.empty());
    }

    auto y = f.getVoicingNames();
    for (auto t : y) {
        assert(!t.empty());
    }
}

static bool _testFiltStability(double fNorm, double feedback)
{
    LadderFilter<double> f;
    f.setType(LadderFilter<double>::Types::_4PLP);
    f.setVoicing(LadderFilter<double>::Voicing::Clean);
    f.setGain(1);
    f.setEdge(.5);
    f.setFreqSpread(0);
    f.setBassMakeupGain(1);
    f.setNormalizedFc(fNorm);     // (.05) at .01 is stable at 3.99  .1 stable at 3.4
    f.setSlope(4);
    f.setFeedback(feedback);       // 5 unstable at 200, 4 unstable at 800 3.9 unstable at 1200
                                // 3.75 unstable at 6000
                                // 3.7 stable

   // AudioMath::RandomUniformFunc random = AudioMath::random();
#ifdef _TEXTEX
    const int  reps = 100000;
#else
    const int  reps = 10000;
#endif

    double a = 0, b = 0;
    for (int i = 0; i < reps; ++i) {
     //   const double noise = .1 * (random() - .5);
        const double noise = -.01;
        f.run(noise);
        double x = f.getOutput();
        if ((x < -1) || (x > 1)) {
         //   printf("over at i = %d\n", i);
            f.getOutput();
            f.run(noise);
            return false;
        }
        a = std::min(a, x);
        b = std::max(b, x);
        assert(x < 1);
        assert(x > -1);
       // printf("output = %f\n", x);
    }
  //  printf("LADDER extremes were %f, %f\n", a, b);
    return true;
}


// This is now baked into the ladder
static double getFeedForTest(double fNorm)
{
    double ret = 3.99;

    if (fNorm <= .002) {
        ret = 3.99;
    } else if (fNorm <= .008) {
        ret = 3.9;
    } else if (fNorm <= .032) {
        ret = 3.8;
    } else if (fNorm <= .064) {
        ret = 3.6;
    } else if (fNorm <= .128) {
        ret = 2.95;
    } else if (fNorm <= .25) {
        ret = 2.85;
    } else {
        ret = 2.30;
    }
    return ret;

}

static void testFiltStability()
{
#ifdef _TESTEX   // full test
    for (double f = .001; f < .5; f *= 2) {
     //   const double feed = getFeedForTest(f);
        const double feed = 4;
        bool stable = _testFiltStability(f, feed);
        //printf("freq %f feed = %f stable=%d\n", f, feed, stable);
        assert(stable);

    }

    //printf("\n");
    for (double f = .25; f < .7; f *= 1.1) {
        //const double feed = getFeedForTest(f);
        const double feed = 4;
        bool stable = _testFiltStability(f, feed);
        //printf("freq %f feed = %f stable=%d\n", f, feed, stable);
        assert(stable);

    }
#endif

    assert(_testFiltStability(.01, 4));
    assert(_testFiltStability(.05, 4));
}


static void testPeak0()
{
    PeakDetector p;
    p.step(0);
    assertEQ(p.get(), 0);
    p.step(4.4f);
    assertEQ(p.get(), 4.4f);

    p.step(8.4f);
    assertEQ(p.get(), 8.4f);
}

static void testPeak4_0()
{
    PeakDetector4 p;
    p.step(0);
    assertEQ(p.get(), 0);
    p.step(4.4f);

    p.decay(.1f);
    assertEQ(p.get(), 4.4f);

    p.step(8.4f);
    p.decay(.1f);
    assertEQ(p.get(), 8.4f);
}

static void testPeak1()
{
    PeakDetector p;
    p.step(5);
  //  p.decay(.1f);
    assertEQ(p.get(), 5);
    p.step(4.4f);
 //   p.decay(.1f);
    assertEQ(p.get(), 5);
}

static void testPeak4_1()
{
    PeakDetector4 p;
    p.step(5);
    p.decay(.1f);
    assertEQ(p.get(), 5);
    p.step(4.4f);
    p.decay(.1f);
    assertEQ(p.get(), 5);
}

static void testPeak2()
{
    PeakDetector p;
    p.step(5);
    p.decay(4.f / 44000);
    assertLT(p.get(), 5);
}

static void testPeak4_2()
{
    PeakDetector4 p;
    p.setDecay(4);
    p.step(5);
    p.decay(4.f / 44000);
    assertLT(p.get(), 5);
}

static void testPeak4_3()
{
    PeakDetector4 p;
    p.step( float_4(1,2,3,4));
    p.decay(.1f);
    assertEQ(p.get(), 4);

    p.step(float_4(3, 4, 5, 6));
    p.decay(.1f);
    assertEQ(p.get(), 6);
}

static void testEdgeInMiddleUnity(bool is4PLP)
{
    EdgeTables t;
    float buf[5] = {0};
  
    t.lookup(is4PLP, .5, buf);
    for (int i = 0; i < 4; ++i) {
        assertClose(buf[i], 1, .0001);  // float passed at .02
    }
    assertEQ(buf[4], 0);
}


static void testEdge1(bool is4PLP)
{
    EdgeTables t;
    float buf[5] = {0};

    t.lookup(is4PLP, 1, buf);

    assertGT(buf[1], buf[0]);
    assertGT(buf[2], buf[1]);
    assertGT(buf[3], buf[2]);

    if (is4PLP) {
        assertClose(buf[3], 2.42, .1);
    } else {
        assertClose(buf[3], 1.84, .1);
    }
}


static void testEdge0(bool is4PLP)
{
    EdgeTables t;
    float buf[5] = {0};

    t.lookup(is4PLP, 0, buf);

    assertLT(buf[1], buf[0]);
    assertLT(buf[2], buf[1]);
    assertLT(buf[3], buf[2]);

    if (is4PLP) {
        assertClose(buf[3], .46, .1);
    } else {
        assertClose(buf[3], .71, .1);
    }
}


static void testFiltOutputPoly()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 6;

    for (int i = 0; i < 6; ++i) {
        f.inputs[F::L_AUDIO_INPUT].setVoltage(10, i);
    }

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;          // I think this is the initial patched state
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    assertEQ(f.outputs[F::L_AUDIO_OUTPUT].channels, 6);
  //  assertEQ(f.outputs[F::R_AUDIO_OUTPUT].channels, 6);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 0);

    // should be passing DC already
   // for (int i = 0; i < 6; ++i) {
    for (int i = 5; i >= 0; --i) {
        f._dump(i, "test");
        assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(i), 1);
    }


    // disconnect the inputs
    f.inputs[F::L_AUDIO_INPUT].channels = 0;
    for (int i = 0; i < 8; ++i) {
        f.step();
    }

    // disconnected should go to zero.
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 0);
    assertEQ(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 0);
}

static void testFiltOutputStereo()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 1;
    f.inputs[F::R_AUDIO_INPUT].channels = 1;


    f.inputs[F::L_AUDIO_INPUT].setVoltage(10);
    f.inputs[F::R_AUDIO_INPUT].setVoltage(10);

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;          // I think this is the initial patched state
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    assertEQ(f.outputs[F::L_AUDIO_OUTPUT].channels, 1);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].channels, 1);
    assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
    assertGT(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 1);

    // bug in stereo mode - number of channels is unstable
    for (int i = 0; i < 20; ++i) {
        f.step();
        assertEQ(f.outputs[F::L_AUDIO_OUTPUT].channels, 1);
        assertEQ(f.outputs[F::R_AUDIO_OUTPUT].channels, 1);
        assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
        assertGT(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 1);
    }
}

static void testFiltOutputLeftOnly()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 1;
    f.inputs[F::R_AUDIO_INPUT].channels = 0;


    f.inputs[F::L_AUDIO_INPUT].setVoltage(10);
   // f.inputs[F::R_AUDIO_INPUT].setVoltage(10);

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;          // I think this is the initial patched state
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    assertEQ(f.outputs[F::L_AUDIO_OUTPUT].channels, 1);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].channels, 1);
    assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
    assertGT(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 1);
}

static void testFiltOutputRightOnly()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 0;
    f.inputs[F::R_AUDIO_INPUT].channels = 1;


    f.inputs[F::R_AUDIO_INPUT].setVoltage(10);
  

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;          // I think this is the initial patched state
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    assertEQ(f.outputs[F::L_AUDIO_OUTPUT].channels, 1);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].channels, 1);
    assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
    assertGT(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 1);
}


static void testFiltOutputsDisconnect()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 1;
    f.inputs[F::R_AUDIO_INPUT].channels = 1;
    f.inputs[F::L_AUDIO_INPUT].setVoltage(10, 0);
    f.inputs[F::R_AUDIO_INPUT].setVoltage(10, 0);
    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    // should be passing DC already
    assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
    assertGT(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 1);

    // disconnect the inputs
    f.inputs[F::L_AUDIO_INPUT].channels = 0;
    f.inputs[F::R_AUDIO_INPUT].channels = 0;

    for (int i = 0; i < 8; ++i) {
        f.step();
    }

    // disconnected should go to zero.
    assertEQ(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 0);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), 0);

}


static void testFiltOutputsRightDisconnect()
{
    assert(false);          // this may no longer be valid
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 1;
    f.inputs[F::R_AUDIO_INPUT].channels = 0;
    f.inputs[F::L_AUDIO_INPUT].setVoltage(10, 0);
    f.inputs[F::R_AUDIO_INPUT].setVoltage(0, 0);
    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    // should be passing DC already
    assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), (f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0)));

}


static void testFiltOutputsLeftDisconnect()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 0;
    f.inputs[F::R_AUDIO_INPUT].channels = 1;
    f.inputs[F::L_AUDIO_INPUT].setVoltage(0,0);
    f.inputs[F::R_AUDIO_INPUT].setVoltage(10, 0);
    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    f.params[F::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 50; ++i) {
        f.step();
    }

    // should be passing DC already
    assertGT(f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0), 1);
    assertEQ(f.outputs[F::R_AUDIO_OUTPUT].getVoltage(0), (f.outputs[F::L_AUDIO_OUTPUT].getVoltage(0)));

}



static void stepF(Filt<TestComposite>& f) {
    for (int i = 0; i < 60; ++i) {
        f.step();
    }
}

//#include "LadderFilterBank.h"

static void testProcVarsStereo()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 1;
    f.inputs[F::R_AUDIO_INPUT].channels = 1;

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    stepF(f);

    const Filt<TestComposite>::ProcessingVars& x = f._getProcVars();
    assert(x.mode == LadderFilterBank<double>::Modes::stereo);
    assert(x.inputForChannel0 == nullptr);
    assert(x.inputForChannel1 != nullptr);
    assertEQ(x.leftOutputChannels, 1);
    assertEQ(x.numFiltersActive , 2);
    assertEQ(x.rightOutputChannels, 1);
}

static void testProcVarsPoly()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 12;

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    stepF(f);

    const Filt<TestComposite>::ProcessingVars& x = f._getProcVars();
    assert(x.mode == LadderFilterBank<double>::Modes::normal);
    assert(x.inputForChannel0 == nullptr);
    assert(x.inputForChannel1 == nullptr);
    assertEQ(x.leftOutputChannels, 12);
    assertEQ(x.numFiltersActive, 12);
    assertEQ(x.rightOutputChannels, 1);
}

static void testProcVarsL()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 1;
    f.inputs[F::R_AUDIO_INPUT].channels = 0;

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    stepF(f);

    const Filt<TestComposite>::ProcessingVars& x = f._getProcVars();
    assert(x.mode == LadderFilterBank<double>::Modes::leftOnly);
    assert(x.inputForChannel0 == nullptr);
    assert(x.inputForChannel1 == nullptr);
    assertEQ(x.leftOutputChannels, 1);
    assertEQ(x.numFiltersActive, 1);
    assertEQ(x.rightOutputChannels, 1);
}

static void testProcVarsR()
{
    using F = Filt<TestComposite>;
    F f;
    f.init();
    f.inputs[F::L_AUDIO_INPUT].channels = 0;
    f.inputs[F::R_AUDIO_INPUT].channels = 1;

    f.outputs[F::L_AUDIO_OUTPUT].channels = 1;
    f.outputs[F::R_AUDIO_OUTPUT].channels = 1;

    stepF(f);

    const Filt<TestComposite>::ProcessingVars& x = f._getProcVars();
    assert(x.mode == LadderFilterBank<double>::Modes::rightOnly);
    assert(x.inputForChannel0 != nullptr);
    assert(x.inputForChannel1 == nullptr);
    assertEQ(x.leftOutputChannels, 1);
    assertEQ(x.numFiltersActive, 1);
    assertEQ(x.rightOutputChannels, 1);
}

void testLadder()
{
    testEdgeInMiddleUnity(true);
    testEdgeInMiddleUnity(false);
    testEdge1(true);
    testEdge1(false);
    testEdge0(true);
    testEdge0(false);
    printf("in test ladder\n");
    testLadderZero();
    testLadderZero2();
    testLadderNotZero();
    testLadderDCf(1000);
    testLadderDCd(1000);
    testLadderTypes();
    testLED0();
    testLED1();
    testLED2();
    testLED3();
    testLED4();
    testLED5();
    testFilt();
    testFilt2();

    testFiltStability();
    testPeak0();
    testPeak1();
    testPeak2();
    testPeak4_0();
    testPeak4_1();
    testPeak4_2();
    testPeak4_3();

    testProcVarsPoly();
    testProcVarsStereo();
    testProcVarsL();
    testProcVarsR();

    testFiltOutputPoly();
    testFiltOutputStereo();
    testFiltOutputLeftOnly();
    testFiltOutputRightOnly();

    // the following are bad tests
#if 0
    testFiltOutputsDisconnect();
    testFiltOutputsRightDisconnect();
    testFiltOutputsLeftDisconnect();
#endif
}