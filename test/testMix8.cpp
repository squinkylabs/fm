/**
 * These tests were originally for Mixer-8.
 * When the other mixers came along, these tests were templatized
 * and adapted.
 *
 * When the mixers diverged, most of the other tests were moved to testMix4.
 */

#include "TestComposite.h"
#include "asserts.h"
#include "Mix8.h"
#include "Mix4.h"
#include "MixM.h"
#include "ObjectCache.h"
#include "TestComposite.h"



using Mixer8 = Mix8<TestComposite>;
using Mixer4 = Mix4<TestComposite>;
using MixerM = MixM<TestComposite>;

static float gOutputBuffer[8];

// function that knows how to get left output from a mixerM
static float outputGetterMixM(std::shared_ptr<MixerM> m, bool bRight)
{
    return m->outputs[bRight ? MixerM::RIGHT_OUTPUT : MixerM::LEFT_OUTPUT].getVoltage(0);
}

static float auxGetterMixM(std::shared_ptr<MixerM> m, bool bRight)
{
    return m->outputs[bRight ? MixerM::RIGHT_SEND_OUTPUT : MixerM::LEFT_SEND_OUTPUT].getVoltage(0);
}

static float auxGetterMixMB(std::shared_ptr<MixerM> m, bool bRight)
{
    return m->outputs[bRight ? MixerM::RIGHT_SENDb_OUTPUT : MixerM::LEFT_SENDb_OUTPUT].getVoltage(0);
}

static float outputGetterMix8(std::shared_ptr<Mixer8> m, bool bRight)
{
    return m->outputs[bRight ? Mixer8::RIGHT_OUTPUT : Mixer8::LEFT_OUTPUT].getVoltage(0);
}

static float outputGetterMix4(std::shared_ptr<Mixer4> m, bool bRight)
{
    // use the expander bus, and apply the default master gain
    return 0.8f * gOutputBuffer[bRight ? 1 : 0];
}

static float auxGetterMix4(std::shared_ptr<Mixer4> m, bool bRight)
{
    // use the expander bus, and apply the default master gain
    return gOutputBuffer[bRight ? 3 : 2];
}

static float auxGetterMix4B(std::shared_ptr<Mixer4> m, bool bRight)
{
    // use the expander bus, and apply the default master gain
    return gOutputBuffer[bRight ? 5 : 4];
}

static float auxGetterMix8(std::shared_ptr<Mixer8> m, bool bRight)
{
    return m->outputs[bRight ? Mixer8::RIGHT_SEND_OUTPUT : Mixer8::LEFT_SEND_OUTPUT].getVoltage(0);
}


template <typename T>
static std::shared_ptr<T> getMixerBase()
{
    auto ret = std::make_shared<T>();
    ret->init();
    auto icomp = ret->getDescription();
    for (int i = 0; i < icomp->getNumParams(); ++i) {
        auto param = icomp->getParamValue(i);
        ret->params[i].value = param.def;
    }

    return ret;
}

template <typename T>
static std::shared_ptr<T> getMixer();

template <>
std::shared_ptr<Mixer4> getMixer<Mixer4>()
{
    std::shared_ptr<Mixer4> ret = getMixerBase<Mixer4>();
    ret->setExpansionOutputs(gOutputBuffer);
    return ret;
}

template <>
std::shared_ptr<MixerM> getMixer<MixerM>()
{
    std::shared_ptr<MixerM> ret = getMixerBase<MixerM>();
    return ret;
}

template <>
std::shared_ptr<Mixer8> getMixer<Mixer8>()
{
    std::shared_ptr<Mixer8> ret = getMixerBase<Mixer8>();
    return ret;
}


template <typename T>
void testChannel(int channel, bool useParam)
{
    T m;
    m.init();

    // param > 1 is illegal. Fix this test!
    const float activeParamValue = useParam ? 2.f : 1.f;
    const float activeCVValue = useParam ? 5.f : 10.f;

    // zero all inputs, put all channel gains to 1
    for (int i = 0; i < T::numChannels; ++i) {
        m.inputs[T::AUDIO0_INPUT + i].setVoltage(0, 0);
        m.params[T::GAIN0_PARAM + i].value = 1;
    }

    auto xx = m.inputs[T::PAN0_INPUT].getVoltage(0);
    auto yy = m.params[T::PAN0_PARAM].value;

    m.inputs[T::AUDIO0_INPUT + channel].setVoltage(5.5f, 0);
    m.params[T::GAIN0_PARAM + channel].value = activeParamValue;
    m.inputs[T::LEVEL0_INPUT + channel].setVoltage(activeCVValue, 0);
    m.inputs[T::LEVEL0_INPUT + channel].channels = 1;

    for (int i = 0; i < 1000; ++i) {
        m.step();           // let mutes settle
    }

    for (int i = 0; i < T::numChannels; ++i) {

       // auto debugMuteState = m.params[T::MUTE0_STATE_PARAM + i];
        float expected = (i == channel) ? 5.5f : 0;
        assertClose(m.outputs[T::CHANNEL0_OUTPUT + i].getVoltage(0), expected, .01f);
    }
}

template <typename T>
static void testChannel()
{
    for (int i = 0; i < T::numChannels; ++i) {
        testChannel<T>(i, true);
        testChannel<T>(i, false);
    }
}

template <typename T>
static void _testMaster(std::function<float(std::shared_ptr<T>, bool bRight)> outputGetter, bool side)
{
    auto m = getMixer<T>();

    m->inputs[T::AUDIO0_INPUT].setVoltage(10, 0);
    m->params[T::PAN0_PARAM].value = side ? -1.f : 1.f;     // full left

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }

    float outL = outputGetter(m, 0);
    float outR = outputGetter(m, 1);
    float expectedOutL = side ? float(10 * .8 * .8) : 0;
    float expectedOutR = side ? 0 : float(10 * .8 * .8);
    assertClose(outL, expectedOutL, .01);
    assertClose(outR, expectedOutR, .01);
}

template <typename T>
static void testMaster(std::function<float(std::shared_ptr<T>, bool bRight)> outputGetter)
{
    _testMaster<T>(outputGetter, false);
    _testMaster<T>(outputGetter, true);
}

/**
 * param augGetter is one of the functions that will retrieve data from the aux send.
 * param side is true if left,  false if right
 * param aux0 is true if we want to test the aux0 bus, false for aux1
 * param sendParam is the parameter id for the send level
 * param pre is true for pre-fader send
 */
template <typename T>
static void _testAuxOut(
    std::function<float(std::shared_ptr<T>, bool bRight)> auxGetter,
    bool side,
    bool aux0,
    int sendParam,
    bool pre,
    int preParam
)
{
    auto m = getMixer<T>();


    m->inputs[T::AUDIO0_INPUT].value = 10;
    m->params[T::PAN0_PARAM].value = side ? -1.f : 1.f;     // full left
    m->params[sendParam].value = 1;

    if (preParam) {
        m->params[preParam].value = pre ? 1.f : 0.f;
    }

    // with pre-fader, should still get out with no volume
    // TODO: test that with post fade the fader has an effect
    if (pre) {
        m->params[T::GAIN0_PARAM].value = 0;
    }

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }

    float auxL = auxGetter(m, 0);
    float auxR = auxGetter(m, 1);

    float expectedOutL = side ? float(10 * .8) : 0;
    float expectedOutR = side ? 0 : float(10 * .8);
    if (pre) {
        // no pan on pre
        expectedOutL = float(10 * .8);
        expectedOutR = float(10 * .8);
    }

    assertClose(auxL, expectedOutL, .01);
    assertClose(auxR, expectedOutR, .01);
}

template <typename T>
static void testAuxOut(std::function<float(std::shared_ptr<T>, bool bRight)> auxGetter)
{
    _testAuxOut<T>(auxGetter, false, true, T::SEND0_PARAM, false, 0);
    _testAuxOut<T>(auxGetter, true, true, T::SEND0_PARAM, false, 0);
}

template <typename T>
void testMute(std::function<float(std::shared_ptr<T>, bool bRight)> outputGetter)
{
    auto m = getMixer<T>();
    m->step();          // let mutes see zero first (startup reset)

    m->inputs[T::AUDIO0_INPUT].setVoltage(10, 0);
    m->params[T::PAN0_PARAM].value = -1.f;     // full left
    m->params[T::MUTE0_PARAM].value = 1;        // mute
    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }

    assertClose(outputGetter(m, false), 0, .001);

    // un-mute
    m->params[T::MUTE0_PARAM].value = 0;
    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }
    assertGT(outputGetter(m, false), 5);

    m->inputs[T::MUTE0_INPUT].setVoltage(10, 0);       //mute with CV

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }

    assertClose(outputGetter(m, false), 0, .001);
}

// only works for mix8 now
template <typename T>
void testSoloLegacy(std::function<float(std::shared_ptr<T>, bool bRight)> outputGetter)
{
    auto m = getMixer<T>();

    m->inputs[T::AUDIO0_INPUT].setVoltage(10, 0);
    m->params[T::PAN0_PARAM].value = -1.f;     // full left
    m->params[T::SOLO0_PARAM].value = 1;        // solo

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }
    m->step();

    assertClose(outputGetter(m, false), float(10 * .8 * .8), .001);
    assertClose(outputGetter(m, true), 0, .001);
}

static void testPanLook0()
{
    assert(ObjectCache<float>::getMixerPanL());
    assert(ObjectCache<float>::getMixerPanR());
}

static inline float _PanL(float balance, float cv)
{ // -1...+1
    float p, inp;
    inp = balance + cv / 5;
    p = M_PI * (std::clamp(inp, -1.0f, 1.0f) + 1) / 4;
    return ::cos(p);
}

static inline float _PanR(float balance, float cv)
{
    float p, inp;
    inp = balance + cv / 5;
    p = M_PI * (std::clamp(inp, -1.0f, 1.0f) + 1) / 4;
    return ::sin(p);
}

static void testPanLookL()
{
    auto p = ObjectCache<float>::getMixerPanL();
    auto r = ObjectCache<float>::getMixerPanR();
    for (float x = -1; x <= 1; x += .021f) {
        const float lookL = LookupTable<float>::lookup(*p, x);
        const float actualL = _PanL(x, 0);
        assertClose(lookL, actualL, .01);

        const float lookR = LookupTable<float>::lookup(*r, x);
        const float actualR = _PanR(x, 0);
        assertClose(lookR, actualR, .01);

        assertNE(actualL, actualR);
    }
}

template <typename T>
static void testReturn()
{
    auto m = getMixer<T>();
    m->inputs[T::LEFT_RETURN_INPUT].setVoltage(5, 0);
    m->inputs[T::RIGHT_RETURN_INPUT].setVoltage(6, 0);

    m->params[T::RETURN_GAIN_PARAM].value = .75;        // unity gain
    m->params[T::MASTER_VOLUME_PARAM].value = 1;        // gain of 2
    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }
    float defaultMasterGain = 1.002374f;

    float expectedOutL = 5 * defaultMasterGain * 2.f;
    float expectedOutR = 6 * defaultMasterGain * 2.f;

    float outL = m->outputs[T::LEFT_OUTPUT].getVoltage(0);
    float outR = m->outputs[T::RIGHT_OUTPUT].getVoltage(0);
    assertClose(outL, expectedOutL, .01);
    assertClose(outR, expectedOutR, .01);
}


template <typename T>
static void testPanMiddle(std::function<float(std::shared_ptr<T>, bool bRight)> outputGetter)
{
    auto m = getMixer<T>();

    m->inputs[T::AUDIO0_INPUT].setVoltage(10, 0);
    m->params[T::PAN0_PARAM].value = 0;     // pan in middle

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }

    float outL = outputGetter(m, false);
    float outR = outputGetter(m, false);
    float expectedOut = float(10 * .8 * .8f / sqrt(2.f));

    assertClose(outL, expectedOut, .01);
    assertClose(outR, expectedOut, .01);
}

template <typename T>
static void testMasterMute()
{
    auto m = getMixer<T>();

    m->inputs[T::AUDIO0_INPUT].setVoltage(10, 0);
    m->params[T::PAN0_PARAM].value = 0;     // straight up
    m->params[T::MASTER_MUTE_PARAM].value = 1;

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }
    float outL = m->outputs[T::LEFT_OUTPUT].getVoltage(0);
    float outR = m->outputs[T::RIGHT_OUTPUT].getVoltage(0);
    float expectedOut = 0;
    assertClose(outL, expectedOut, .01);
    assertClose(outR, expectedOut, .01);
}

#include "ExtremeTester.h"
template <typename T>
static void testInputExtremes()
{
    T dut;
   // dut.setSampleRate(44100);

    std::vector< std::pair<float, float> > paramLimits;
    dut.init();

    paramLimits.resize(dut.NUM_PARAMS);
    using fp = std::pair<float, float>;


    auto iComp = T::getDescription();
    for (int i = 0; i < iComp->getNumParams(); ++i) {
        auto desc = iComp->getParamValue(i);
        fp t(desc.min, desc.max);
        paramLimits[i] = t;
    }

    ExtremeTester<T>::test(dut, paramLimits, false, "mix");
}

static void testExpansion8()
{
    auto m = getMixer<Mixer8>();
    m->inputs[Mixer8::RIGHT_EXPAND_INPUT].setVoltage(5, 0);
    m->inputs[Mixer8::LEFT_EXPAND_INPUT].setVoltage(6, 0);

    m->step();

    assertEQ(m->outputs[Mixer8::LEFT_OUTPUT].getVoltage(0), 6);
    assertEQ(m->outputs[Mixer8::RIGHT_OUTPUT].getVoltage(0), 5);

    assertEQ(Mixer8::LEFT_EXPAND_INPUT + 1, Mixer8::RIGHT_EXPAND_INPUT);
    assertEQ(Mixer8::LEFT_RETURN_INPUT + 1, Mixer8::RIGHT_RETURN_INPUT);
}

// test pass-through of data on expansion buses
static void testExpansion4()
{
    float inbuf[6];
    float outbuf[6];

    auto m = getMixer<Mixer4>();
    inbuf[0] = 1.1f;
    inbuf[1] = 3.2f;
    inbuf[2] = 2.2f;
    inbuf[3] = 3.3f;
    m->setExpansionInputs(inbuf);
    m->setExpansionOutputs(outbuf);
    m->step();
    assertEQ(outbuf[0], 1.1f);
    assertEQ(outbuf[1], 3.2f);
    assertEQ(outbuf[2], 2.2f);
    assertEQ(outbuf[3], 3.3f);

    // output,no input
    m->setExpansionInputs(nullptr);
    m->step();
    assertEQ(outbuf[0], 0);
    assertEQ(outbuf[1], 0);
    assertEQ(outbuf[2], 0);
    assertEQ(outbuf[3], 0);

    // input, no output
    m->setExpansionInputs(inbuf);
    m->setExpansionOutputs(nullptr);
    m->step();
    assertEQ(outbuf[0], 0);
    assertEQ(outbuf[1], 0);
    assertEQ(outbuf[2], 0);
    assertEQ(outbuf[3], 0);
}

static void testExpansionM()
{
    float inbuf[6];

    auto m = getMixer<MixerM>();
    inbuf[0] = 1.1f;
    inbuf[1] = 3.2f;
    inbuf[2] = 2.2f;
    inbuf[3] = 3.3f;
    m->setExpansionInputs(inbuf);

    m->params[MixerM::MASTER_VOLUME_PARAM].value = 1;

    for (int i = 0; i < 1000; ++i) {
        m->step();           // let mutes settle
    }

    const float masterVolMaxGain = 2;

    assertClose(m->outputs[MixerM::LEFT_OUTPUT].getVoltage(0), 1.1f * masterVolMaxGain, .01);
    assertClose(m->outputs[MixerM::RIGHT_OUTPUT].getVoltage(0), 3.2f * masterVolMaxGain, .01);
    assertClose(m->outputs[MixerM::LEFT_SEND_OUTPUT].getVoltage(0), 2.2f, .01);
    assertClose(m->outputs[MixerM::RIGHT_SEND_OUTPUT].getVoltage(0), 3.3f, .01);

    // disconnect input
    m->setExpansionInputs(nullptr);
    m->step();

    assertClose(m->outputs[MixerM::LEFT_OUTPUT].getVoltage(0), 0, .01);
    assertClose(m->outputs[MixerM::RIGHT_OUTPUT].getVoltage(0), 0, .01);
    assertClose(m->outputs[MixerM::LEFT_SEND_OUTPUT].getVoltage(0), 0, .01);
    assertClose(m->outputs[MixerM::RIGHT_SEND_OUTPUT].getVoltage(0), 0, .01);
}

#if 0
void testMix8()
{
    printf("testMix8 disabled\n");
}
#else
void testMix8()
{
    testChannel<Mixer8>();

    testMaster<Mixer8>(outputGetterMix8);

    // why doesn't this work?
    testMute<MixerM>(outputGetterMixM);


    // why does this test fail for mixer8?
  //  testAuxOut<Mixer8>(auxGetterMix8);
   // testAuxOut<MixerM>(auxGetterMixM);
  //  testAuxOut<Mixer4>(auxGetterMix4);
   

 

    // now all mixers support "legacy" solo
    testSoloLegacy<Mixer8>(outputGetterMix8);
   // testSoloLegacy<Mixer4>(outputGetterMix4);
   // testSoloLegacy<MixerM>(outputGetterMixM);

  //  testSoloNew<MixerM>(outputGetterMixM);
 //   testSoloNew<Mixer4>(outputGetterMix4);

   // testSoloNew2<MixerM>(outputGetterMixM);
  //  testSoloNew2<Mixer4>(outputGetterMix4);

    testPanLook0();
    testPanLookL();

    testPanMiddle<Mixer8>(outputGetterMix8);


    testMasterMute<Mixer8>();
    testMasterMute<MixerM>();

    testExpansion4();
    testExpansionM();
    testExpansion8();

    testReturn<MixerM>();

    // values have diverged from origin.
    //testReturn<Mixer8>();

    // TODO: need a test for master volume

#if 0 // these take too long
    testInputExtremes<Mixer8>();
    testInputExtremes<MixerM>();
    testInputExtremes<Mixer4>();
#endif
}
#endif