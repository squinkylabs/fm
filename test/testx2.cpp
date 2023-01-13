#include <set>

#include "AudioMath.h"
#include "CompiledInstrument.h"
#include "CompiledRegion.h"
#include "FilePath.h"
#include "InstrumentInfo.h"
#include "SInstrument.h"
#include "Sampler4vx.h"
#include "SamplerSchema.h"
#include "SqLog.h"
#include "WaveLoader.h"
#include "asserts.h"
#include "samplerTests.h"

static const char* tinnyPiano = R"foo(D:\samples\UprightPianoKW-small-SFZ-20190703\UprightPianoKW-small-20190703.sfz)foo";
const char* tinnyPianoRoot = R"foo(D:\samples\UprightPianoKW-small-SFZ-20190703\)foo";
static const char* smallPiano = R"foo(D:\samples\K18-Upright-Piano\K18-Upright-Piano.sfz)foo";

static void testWaveLoader0() {
    WaveLoader w;
    w.addNextSample(FilePath("fake file name"));
    auto b = w.loadNextFile();
    assertEQ(int(b), int(WaveLoader::LoaderState::Error));
    assertEQ(w.getProgressPercent(), 0);
}

static void testWaveLoader1Wav() {
    WaveLoader w;
    w.addNextSample(FilePath("D:\\samples\\UprightPianoKW-small-SFZ-20190703\\samples\\A3vH.wav"));
    auto b = w.loadNextFile();
    assertEQ(int(b), int(WaveLoader::LoaderState::Done));
    auto x = w.getInfo(1);
    assert(x->isValid());
    x = w.getInfo(0);
    assert(!x);

    assertEQ(w.getProgressPercent(), 100);
}

static void testWaveLoader1Flac() {
    WaveLoader w;
    w.addNextSample(FilePath("D:\\samples\\test\\flac\\mono24.flac"));
    auto b = w.loadNextFile();
    assertEQ(int(b), int(WaveLoader::LoaderState::Done));
    auto x = w.getInfo(1);
    assert(x->isValid());
    x = w.getInfo(0);
    assert(!x);

    assertEQ(w.getProgressPercent(), 100);
}

static void testWaveLoader2() {
    WaveLoader w;
    w.addNextSample(FilePath("D:/samples/UprightPianoKW-small-SFZ-20190703/samples/A3vH.wav"));
    auto b = w.loadNextFile();
    assertEQ(int(b), int(WaveLoader::LoaderState::Done));
    auto x = w.getInfo(1);
    assert(x->isValid());
    x = w.getInfo(0);
    assert(!x);
}

static void testWaveLoaderNot44() {
    WaveLoader w;
    w.addNextSample(FilePath("D:\\samples\\K18-Upright-Piano\\K18\\A0.f.wav"));

    auto b = w.loadNextFile();
    assertEQ(int(b), int(WaveLoader::LoaderState::Done));

    auto x = w.getInfo(1);
    assert(x->isValid());
    assertEQ(x->getSampleRate(), 48000);  // we keep the input sample rate
}

static void testWaveLoaderNotMono() {
    assert(false);
}

static void testPlayInfo() {
    VoicePlayInfo info;
    assertEQ(info.valid, false);
}

static void testPlayInfo(const char* patch, const std::vector<int>& velRanges) {
    SInstrumentPtr inst = std::make_shared<SInstrument>();

    auto err = SParse::goFile(FilePath(patch), inst);
    assert(err.empty());

    SamplerErrorContext errc;
    CompiledInstrumentPtr cinst = CompiledInstrument::make(errc, inst);
    // assert(errc.empty());
    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 60;
    params.midiVelocity = 60;
    cinst->play(info, params, nullptr, 44100);
    assert(info.valid);
    int minSampleIndex = 200;
    int maxSampleIndex = -200;
    for (int pitch = 21; pitch <= 108; ++pitch) {
        for (auto vel : velRanges) {
            info.valid = false;
            params.midiPitch = pitch;
            params.midiVelocity = vel;
            cinst->play(info, params, nullptr, 44100);
            assert(info.valid);
            assert(info.canPlay());
            minSampleIndex = std::min(minSampleIndex, info.sampleIndex);
            maxSampleIndex = std::max(maxSampleIndex, info.sampleIndex);
        }
    }

    // VoicePlayParameter params;
    params.midiPitch = 20;
    params.midiVelocity = 60;
    cinst->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 109;
    cinst->play(info, params, nullptr, 44100);
    assert(!info.valid);

    assert(minSampleIndex == 1);
    assert(maxSampleIndex > 4);
}

void testPlayInfoTinnyPiano() {
    testPlayInfo(tinnyPiano, {64});
}

void testPlayInfoSmallPiano() {
    printf("\n----- testPlayInfoSmallPiano\n");
    testPlayInfo(smallPiano, {1, 23, 44, 65, 80, 107});
}

static void testLoadWavesPiano() {
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    //  const char* p = R"foo(D:\samples\UprightPianoKW-small-SFZ-20190703\UprightPianoKW-small-20190703.sfz)foo";
    auto err = SParse::goFile(FilePath(tinnyPiano), inst);
    assert(err.empty());

    SamplerErrorContext errc;
    CompiledInstrumentPtr cinst = CompiledInstrument::make(errc, inst);
    WaveLoaderPtr loader = std::make_shared<WaveLoader>();

    // const char* pRoot = R"foo(D:\samples\UprightPianoKW-small-SFZ-20190703\)foo";
    cinst->setWaves(loader, FilePath(tinnyPianoRoot));
    loader->loadNextFile();
    // assert(false);
}

static void testCIKeysAndValues(const std::string& pitch, int expectedPitch) {

    SKeyValuePairPtr p = std::make_shared<SKeyValuePair>("hikey", pitch);
    SKeyValueList l = {p};

    SamplerErrorContext errc;
    auto output = SamplerSchema::compile(errc, l);
    assert(errc.empty());
    assertEQ(output->_size(), 1);
    SamplerSchema::ValuePtr vp = output->get(SamplerSchema::Opcode::HI_KEY);
    assert(vp);
    assertEQ(vp->numericInt, expectedPitch);
}

static void testCIKeysAndValues() {
    testCIKeysAndValues("12", 12);
}

static void testCIKeysAndValuesNotesLC() {
    // c0 = 12
    // c6 - 84
    testCIKeysAndValues("c6", 12 * (6 + 1));
    testCIKeysAndValues("d6", 2 + 12 * (6 + 1));
    testCIKeysAndValues("e6", 4 + 12 * (6 + 1));
    testCIKeysAndValues("f6", 5 + 12 * (6 + 1));
    testCIKeysAndValues("g6", 7 + 12 * (6 + 1));
    testCIKeysAndValues("a6", 9 + 12 * (6 + 1));

    testCIKeysAndValues("b5", 12 * (6 + 1) - 1);
}

static void testCIKeysAndValuesNotesUC() {
    // c0 = 12
    // c6 - 84
    testCIKeysAndValues("C6", 12 * (6 + 1));
    testCIKeysAndValues("D6", 2 + 12 * (6 + 1));
    testCIKeysAndValues("E6", 4 + 12 * (6 + 1));
    testCIKeysAndValues("F6", 5 + 12 * (6 + 1));
    testCIKeysAndValues("G6", 7 + 12 * (6 + 1));
    testCIKeysAndValues("A6", 9 + 12 * (6 + 1));

    testCIKeysAndValues("B5", 12 * (6 + 1) - 1);

    testCIKeysAndValues("C5", 12 * (5 + 1));
    testCIKeysAndValues("C4", 12 * (4 + 1));
    testCIKeysAndValues("C3", 12 * (3 + 1));
    testCIKeysAndValues("C2", 12 * (2 + 1));
    testCIKeysAndValues("C1", 12 * (1 + 1));
    testCIKeysAndValues("C0", 12 * (0 + 1));
    testCIKeysAndValues("C-1", 12 * (-1 + 1));
    testCIKeysAndValues("C-2", 12 * (-2 + 1));
}

static void testCIKeysAndValuesNotesSharp() {
    // c c# d d# = 3 semis
    int expectedPitch = 12 * (4 + 1) + 3;
    testCIKeysAndValues("d#4", expectedPitch);
}

static void testParseHeadingGlobalAndRegionCompiled() {
    printf("start test parse global\n");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go("<global><region>", inst);

    assert(err.empty());
    SamplerErrorContext errc;
    CompiledInstrument::expandAllKV(errc, inst);
    assert(errc.empty());

    assertEQ(inst->headings.size(), 2);
    assertEQ((int)inst->headings[0]->type, (int)SHeading::Type::Global);
    assertEQ((int)inst->headings[1]->type, (int)SHeading::Type::Region);

    assert(inst->headings[0]->compiledValues);
    assertEQ(inst->headings[0]->compiledValues->_size(), 0);

    assert(inst->headings[1]->compiledValues);
    assertEQ(inst->headings[1]->compiledValues->_size(), 0);
}

static void testParseHeadingGlobalWithKVAndRegionCompiled() {
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go("<global>hikey=57<region>", inst);

    assert(err.empty());
    SamplerErrorContext errc;
    CompiledInstrument::expandAllKV(errc, inst);
    assert(errc.empty());

    assertEQ(inst->headings.size(), 2);
    assertEQ((int)inst->headings[0]->type, (int)SHeading::Type::Global);
    assertEQ((int)inst->headings[1]->type, (int)SHeading::Type::Region);

    assert(inst->headings[0]->compiledValues);
    assertEQ(inst->headings[0]->compiledValues->_size(), 1);
    auto val = inst->headings[0]->compiledValues->get(SamplerSchema::Opcode::HI_KEY);
    assertEQ(val->numericInt, 57);

    //  SGroupPtr group = inst->groups[0];
    //  assert(group);
    assert(inst->headings[1]->compiledValues);
    assertEQ(inst->headings[1]->compiledValues->_size(), 0);
}

static void testParseHeadingGlobalWitRegionKVCompiled() {
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go("<global><region><region>lokey=57<region>", inst);

    assert(err.empty());
    SamplerErrorContext errc;
    CompiledInstrument::expandAllKV(errc, inst);
    assert(errc.empty());

    assertEQ(inst->headings.size(), 4);
    assertEQ((int)inst->headings[0]->type, (int)SHeading::Type::Global);
    assertEQ((int)inst->headings[1]->type, (int)SHeading::Type::Region);
    assertEQ((int)inst->headings[2]->type, (int)SHeading::Type::Region);
    assertEQ((int)inst->headings[3]->type, (int)SHeading::Type::Region);

    assert(inst->headings[0]->compiledValues);
    assert(inst->headings[1]->compiledValues);
    assert(inst->headings[2]->compiledValues);
    assert(inst->headings[3]->compiledValues);

    assertEQ(inst->headings[0]->compiledValues->_size(), 0);
    assertEQ(inst->headings[1]->compiledValues->_size(), 0);
    assertEQ(inst->headings[2]->compiledValues->_size(), 1);
    assertEQ(inst->headings[3]->compiledValues->_size(), 0);

    //  assertEQ(inst->headings[2]->compiledValues->get(SamplerSchema::Opcode::LO_KEY), 57);

    auto val = inst->headings[2]->compiledValues->get(SamplerSchema::Opcode::LO_KEY);
    assertEQ(val->numericInt, 57);
}

static void testParseHeadingControl() {
    const char* data = R"foo(<control>
        default_path=Woodwinds\Bassoon\stac\
        <global>ampeg_attack=0.001 ampeg_release=3 ampeg_dynamic=1 volume=0
        <group> //Begin Group 1
        // lorand=0.0 hirand=0.5 
        group_label=gr_1
        <region>sample=PSBassoon_A1_v1_rr1.wav lokey=43 hikey=46 pitch_keycenter=45 lovel=1 hivel=62 volume=12
        )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);
    assert(err.empty());

    assertEQ(inst->headings.size(), 4);
    assertEQ((int)inst->headings[0]->type, (int)SHeading::Type::Control);
    assertEQ((int)inst->headings[1]->type, (int)SHeading::Type::Global);
    assertEQ((int)inst->headings[2]->type, (int)SHeading::Type::Group);
    assertEQ((int)inst->headings[3]->type, (int)SHeading::Type::Region);

    SamplerErrorContext errc;
    CompiledInstrumentPtr cinst = CompiledInstrument::make(errc, inst);

    std::vector<CompiledRegionPtr> regions;
    cinst->_pool()._getAllRegions(regions);
    assertEQ(regions.size(), 1);
    CompiledRegionPtr creg = regions[0];
    assertEQ(creg->lokey, 43);
    // TODO: does this test work?
}

static void testParseInclude() {
    //SQINFO("------ testParseInclude");
    const char* data = R"foo(<global>bend_up=1200
        bend_down=-1200
        #include "vc_arco_sus_map.sfz")foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    // for now, include give us parse errors.
    assert(!err.empty());
}

static void testParseLabel() {
    const char* data = R"foo(<region>sw_label=abd def ghi)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);
    assert(err.empty());
}

static void testCompiledRegion() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>sample=K18\C7.pp.wav lovel=1 hivel=22 lokey=95 hikey=97 pitch_keycenter=96 offset=200 end=1234)foo");
    assertEQ(cr->keycenter, 96);
    assertEQ(cr->lovel, 1);
    assertEQ(cr->hivel, 22);
    assertEQ(cr->lokey, 95);
    assertEQ(cr->hikey, 97);
    std::string expected = std::string("K18") + FilePath::nativeSeparator() + std::string("C7.pp.wav");
    assertEQ(cr->sampleFile.toString(), expected);
    assertEQ(cr->loopData.offset, 200);
    assertEQ(cr->loopData.end, 1234);

    // test a few defaults
    assertEQ(cr->volume, 0);
    assertEQ(cr->tune, 0);
}

static void testCompiledRegionAddedOpcodes() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>sample=a key=64 tune=11 volume=-13)foo");

    assertEQ(cr->tune, 11);
    assertEQ(cr->volume, -13);
}

static void testCompiledRegionInherit() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<group>sample=K18\C7.pp.wav lovel=1 hivel=22 lokey=95 hikey=97 pitch_keycenter=96 tune=10 offset=200<region>)foo");
    assertEQ(cr->keycenter, 96);
    assertEQ(cr->lovel, 1);
    assertEQ(cr->hivel, 22);
    assertEQ(cr->lokey, 95);
    assertEQ(cr->hikey, 97);
    assertEQ(cr->sampleFile.toString(), "K18\\C7.pp.wav");
    assertEQ(cr->loopData.offset, 200);
}

static void testCompiledRegionKey() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>key=32)foo");
    assertEQ(cr->lokey, 32);
    assertEQ(cr->hikey, 32);
    assertEQ(cr->keycenter, 32);
}

static void testCompiledRegionVel() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>hivel=12)foo");
    assertEQ(cr->lovel, 1);
    assertEQ(cr->hivel, 12);
}

static void testCompiledRegionVel2() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>lovel=71)foo");
    assertEQ(cr->lovel, 71);
    assertEQ(cr->hivel, 127);
}

static void testCompiledRegionVel3() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>hivel=59 lovel=29)foo");
    assertEQ(cr->lovel, 29);
    assertEQ(cr->hivel, 59);
}

static void testCompiledRegionsRand() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>hirand=.7 lorand=.29)foo");
    assertEQ(cr->hirand, .7f);
    assertEQ(cr->lorand, .29f);
}

static void testCompiledRegionSeqIndex1() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<region>seq_position=11)foo");
    assertEQ(cr->sequencePosition, 11);
}

static void testCompiledRegionSeqIndex2() {
    CompiledRegionPtr cr = st::makeRegion(R"foo(<group>seq_position=11<region>)foo");
    assertEQ(cr->sequencePosition, 11);
}

static void testCompileInst0() {
    printf("\n-- test comp inst 1\n");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go("<global><region>lokey=50 hikey=50", inst);
    assert(err.empty());

    SamplerErrorContext errc;
    CompiledInstrumentPtr i = CompiledInstrument::make(errc, inst);
    assert(errc.empty());

    VoicePlayInfo info;
    VoicePlayParameter params;
    info.sampleIndex = 0;
    assert(!info.valid);
    params.midiPitch = 50;
    params.midiVelocity = 60;
    i->play(info, params, nullptr, 44100);
    assert(info.valid);  // this will fail until we implement a real compiler
    assertNE(info.sampleIndex, 0);
}

#include <stdio.h>
#include <sys/stat.h>

static void testCompileInstLinNumbers() {
    printf("\n-- testCompileInstLinNumbers\n");

    SInstrumentPtr inst = std::make_shared<SInstrument>();
#if 0

#if 0
    // this is wrong, gives region on line 2, but shoudl be 3
    const char* content = R"foo(<control>
 hint_ram_based=1
<region>
 lokey=60 hikey=64
 loop_mode=loop_continuous)foo";
#endif
const char* content = R"foo(<region>
lokey=60 hikey=64
loop_mode=loop_continuous)foo";

    auto err = SParse::go(content, inst);
#else
    FilePath fp("d:\\samples\\warren_b\\U20 Electric Grand sf2\\U20 E Grand.sfz");
    auto err = SParse::goFile(fp, inst);

#endif
    //SQINFO("err: %s", err.c_str());
    assert(err.empty());
   


    SamplerErrorContext errc;
    CompiledInstrumentPtr i = CompiledInstrument::make(errc, inst);
    // assert(errc.empty());

    auto pool = i->_pool();
    pool._dump(0);
  //  assertEQ(pool.size(), 2);
    assert(false);

}

static void testCompileInst1() {
    printf("\n-- test comp inst 1\n");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go("<global><region>lokey=50\nhikey=50\nsample=foo<region>lokey=60\nhikey=60\nsample=bar<region>lokey=70\nhikey=70\nsample=baz", inst);
    assert(err.empty());

    SamplerErrorContext errc;
    CompiledInstrumentPtr i = CompiledInstrument::make(errc, inst);
    assert(errc.empty());

    VoicePlayInfo info;
    info.sampleIndex = 0;
    assert(!info.valid);
    VoicePlayParameter params;
    params.midiPitch = 60;
    params.midiVelocity = 60;
    i->play(info, params, nullptr, 44100);
    assert(info.valid);  // this will fail until we implement a real compiler
    assertNE(info.sampleIndex, 0);
}

static void testCompileOverlap() {
    printf("\n-- test comp overlap\n");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(R"foo(<global>
        <region>lokey=0 hikey=127 sample=foo
        <region>lokey=60 hikey=60 pitch_keycenter=60 sample=bar)foo",
                          inst);
    assert(err.empty());

    SamplerErrorContext errc;
    CompiledInstrumentPtr ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());
    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 60;
    params.midiVelocity = 2;

    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertNE(info.sampleIndex, 0);
    assertEQ(info.needsTranspose, false);

    params.midiPitch = 61;
    params.midiVelocity = 100;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);
    params.midiPitch = 59;
    params.midiVelocity = 12;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);
}
static void testCompileTranspose1() {
    printf("\nstarting on transpose 1\n");
    auto inst = std::make_shared<SInstrument>();
    auto err = SParse::go(R"foo(<region> sample=K18\D#1.pp.wav lovel=1 hivel=65 lokey=26 hikey=28 pitch_keycenter=27)foo", inst);
    assert(err.empty());
    SamplerErrorContext errc;
    auto cinst = CompiledInstrument::make(errc, inst);
    assert(errc.empty());
    VoicePlayInfo info;
    printf("about to fetch ifo for key = 26\n");

    // figure the expected transpose for pitch 26
    int semiOffset = -1;

    VoicePlayParameter params;
    params.midiPitch = 26;
    params.midiVelocity = 64;
    cinst->play(info, params, nullptr, 44100);
    assert(info.valid);
    assert(info.needsTranspose);
#ifdef _SAMPFM

    assertClose(info.transposeV, -1.f / 12.f, .0001);
#else
    float pitchMul = float(std::pow(2, semiOffset / 12.0));
    assertEQ(info.transposeAmt, pitchMul);
#endif
}

// make compiler fail to parse something, see what it does
static void testCompileCrash() {
    const char* data = R"foo("abc >dfk z")foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);
    assert(!err.empty());

    //SamplerErrorContext errc;
    CompiledInstrumentPtr cinst = CompiledInstrument::make(err);
    assert(cinst);
    auto emsg = cinst->getInfo()->errorMessage;

    assert(!emsg.empty());
    assert(cinst->isInError());

    VoicePlayInfo info;
    VoicePlayParameter params;

    cinst->play(info, params, nullptr, 44100);
}

static void testCompileCrash2() {
    const char* data = "F:\\foo\\bar.sfz";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::goFile(FilePath(data), inst);
    assert(!err.empty());

    //SamplerErrorContext errc;
    CompiledInstrumentPtr cinst = CompiledInstrument::make(err);
    assert(cinst);
    auto emsg = cinst->getInfo()->errorMessage;

    assert(!emsg.empty());
    assert(cinst->isInError());

    VoicePlayInfo info;
    VoicePlayParameter params;

    cinst->play(info, params, nullptr, 44100);
    assert(!info.valid);
}

static void testCompileGroupSub(const char* data, bool shouldIgnore) {
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    assertEQ(inst->headings.size(), 1);
    assert(inst->headings[0]->type == SHeading::Type::Group);

    SHeadingPtr group = inst->headings[0];
    SamplerErrorContext errc;
    CompiledInstrument::expandAllKV(errc, inst);
    assert(errc.empty());

    assert(inst->wasExpanded);

    //  CompiledGroupPtr cr = std::make_shared<CompiledGroup>(group);
    CompiledRegionPtr cr = std::make_shared<CompiledRegion>(100);
    cr->addRegionInfo(group->compiledValues);
    assertEQ(cr->shouldIgnore(), shouldIgnore);
}

static void testCompileGroup0() {
    testCompileGroupSub(R"foo(<group>)foo", false);
}

static void testCompileGroup1() {
    testCompileGroupSub(R"foo(<group>trigger=attack)foo", false);
}

static void testCompileGroup2() {
    testCompileGroupSub(R"foo(<group>trigger=release)foo", true);
}

static void testCompileMutliControls() {
    //SQWARN("need to re-implement testCompileMutliControls");
}
#if 0  // need to re-do this
static void testCompileMutliControls() {
    //SQINFO("--- start testCompileMutliControls");

    const char* test = R"foo(
        <control>
        default_path=a
        <region>
        sample=r1
        <control>
        default_path=b
        <region>
        sample=r2
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(test, inst);
    assert(err.empty());
    auto ci = CompiledInstrument::make(inst);
    assert(ci);

  // don't have a tree anymore
    auto gps = (ci->_pool())._groups();
    assertEQ(gps.size(), 2);
    assertEQ(gps[0]->regions.size(), 1);

    CompiledRegionPtr r0 = gps[0]->regions[0];
    assert(r0);
    CompiledRegionPtr r1 = gps[1]->regions[0];
    assert(r1);


    std::string expected = std::string("a") + FilePath::nativeSeparator() + std::string("r1");
#if 0
    std::string xx = std::string("a");
    fprintf(stderr, "xx=%s\n", xx.c_str());
     xx += FilePath::nativeSeparator();
    fprintf(stderr, "xx=%s\n", xx.c_str());
 xx += std::string("r1");
    fprintf(stderr, "xx=%s\n", xx.c_str());
#endif

   //SQINFO("at 635, exp=%s", expected.c_str());
    fprintf(stderr, "at 635, exp=%s", expected.c_str());

    fflush(stderr); fflush(stdout);
    assertEQ(r0->sampleFile, expected);
    
    expected = std::string("b") + FilePath::nativeSeparator() + std::string("r2");
    assertEQ(r1->sampleFile, expected);
}
#endif

static void testCompileTreeOne() {
    printf("\n----- testCompileTreeOne\n");
    const char* data = R"foo(<group><region>)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());
#if 0
    auto gps = ci->_pool()._groups();
    assertEQ(gps.size(), 1);
    assertEQ(gps[0]->regions.size(), 1);
#endif

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 60;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100.f);
    assert(info.valid);
}

static void testCompileTreeTwo() {
    printf("\n----- testCompileTreeOne\n");
    const char* data = R"foo(<group>
        <region>key=4
        <region>key=5
        <group>
        <group>)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());

    //SQWARN("need to re-write testCompileTreeTwo");
#if 0
    auto gps = ci->_pool()._groups();
    assertEQ(gps.size(), 3);
    assertEQ(gps[0]->regions.size(), 2);
    assertEQ(gps[1]->regions.empty(), true);
    assertEQ(gps[2]->regions.empty(), true);
#endif
}

static void testCompileKey() {
    const char* data = R"foo(<region>key=12)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    //assertEQ(inst->headings.size(), 1);
    // SHeadingPtr region = inst->headings[0];

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 12;
    params.midiVelocity = 60;

    bool didKS = false;
    const CompiledRegion* region = ci->_pool().play(params, .5, didKS);
    assertEQ(region->lokey, 12);
    assertEQ(region->hikey, 12);
    assertEQ(region->keycenter, 12);

    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.needsTranspose, false);
}

static void testCompileMultiPitch() {
    const char* data = R"foo(
        <region>lokey=10 hikey=12 sample=a pitch_keycenter=11
        <region>lokey=13 hikey=15 sample=b pitch_keycenter=14
        <region>lokey=16 hikey=20 sample=c pitch_keycenter=18
    )foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    VoicePlayInfo info;

    VoicePlayParameter params;
    params.midiPitch = 9;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 21;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 0;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 127;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 11;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.needsTranspose, false);

    //  checking sample index is checking something that just happens to be true
    // If this breaks the test should be fixed.
    assertEQ(info.sampleIndex, 1);

    // pitch 12 requires a semitone up in this region
    params.midiPitch = 12;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);

    assertEQ(info.needsTranspose, true);
#ifdef _SAMPFM
    assertClose(info.transposeV, 1.f / 12.f, .0001);
#else
    assertGT(info.transposeAmt, 1);
#endif

    // Pitch 10 requies a semiton down
    params.midiPitch = 10;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.needsTranspose, true);
#ifdef _SAMPFM
    assertClose(info.transposeV, -1.f / 12.f, .0001);
#else
    assertLT(info.transposeAmt, 1);
#endif

    params.midiPitch = 13;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.needsTranspose, true);
    assertEQ(info.sampleIndex, 2);

    // pitch 20 is up 2 semi in this region
    params.midiPitch = 20;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.needsTranspose, true);
    assertEQ(info.sampleIndex, 3);
#ifdef _SAMPFM
    assertClose(info.transposeV, 2.f / 12.f, .0001);
#else
    assertGT(info.transposeAmt, 1);
#endif
}

static void testCompileMultiVel() {
    printf("\n---- testCompileMultiVel\n");
    const char* data = R"foo(
        <region>key=10 sample=a hivel=20
        <region>key=10 sample=a lovel=21 hivel=90
        <region>key=10 sample=a lovel=91
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    // ci->_dump(0);
    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 11;
    params.midiVelocity = 60;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 10;
    params.midiVelocity = 1;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.sampleIndex, 1);
}

static void testCompileMulPitchAndVelSimple() {
    const char* data = R"foo(
        <region>key=10 sample=a hivel=20  sample=a
        <region>key=10 sample=a lovel=21 hivel=90  sample=b
        <region>key=10 sample=a lovel=91  sample=c

        <region>key=20 sample=a hivel=20  sample=d
        <region>key=20 sample=a lovel=21 hivel=90  sample=e
        <region>key=20 sample=a lovel=91  sample=f

        <region>key=30 sample=a hivel=20  sample=h
        <region>key=30 sample=a lovel=21 hivel=90  sample=i
        <region>key=30 sample=a lovel=91  sample=j
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    VoicePlayInfo info;

    std::set<int> sampleIndicies;

    VoicePlayParameter params;
    params.midiPitch = 10;
    params.midiVelocity = 1;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 10;
    params.midiVelocity = 21;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 10;
    params.midiVelocity = 91;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 20;
    params.midiVelocity = 1;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 20;
    params.midiVelocity = 21;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 20;
    params.midiVelocity = 91;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 30;
    params.midiVelocity = 1;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 30;
    params.midiVelocity = 21;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 30;
    params.midiVelocity = 91;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    assertEQ(sampleIndicies.size(), 9);
    for (auto x : sampleIndicies) {
        assert(x >= 1);
        assert(x <= 9);
    }
}

static void testCompileMulPitchAndVelComplex1() {
    printf("\n----- testCompileMulPitchAndVelComplex1\n");
    const char* data = R"foo(
        <region>key=10 sample=a hivel=20  sample=a
        <region>key=20 sample=a lovel =10 hivel=24  sample=d
       
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    VoicePlayInfo info;
    std::set<int> sampleIndicies;

    VoicePlayParameter params;
    params.midiPitch = 10;
    params.midiVelocity = 20;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 20;
    params.midiVelocity = 22;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    assertEQ(sampleIndicies.size(), 2);
}

static void testCompileAmpegRelease() {
    printf("\n----- static void testCompileAmpegRelease() \n");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    const char* data = R"foo(
        <group>ampeg_release=50
        <region>key=55
        <region>key=30 ampeg_release=10
        <region>key=40 ampeg_release=0
        <group>
        <region>key=20
        )foo";
    auto err = SParse::go(data, inst);
    assert(err.empty());

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 55;
    params.midiVelocity = 127;

    // inherited
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.ampeg_release, 50);

    // set directly
    params.midiPitch = 30;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.ampeg_release, 10);

    // default
    params.midiPitch = 20;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.ampeg_release, .03f);
}

static void testCompileAmpVel() {
    printf("\n----- static void testCompileAmpVel() \n");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    const char* data = R"foo(
        <group>amp_veltrack=50
        <region>key=55<region>key=30 amp_veltrack=100
        <region>key=40 amp_veltrack=0
        <region>key=10
        <group>
        <region>key=20
        )foo";
    auto err = SParse::go(data, inst);
    assert(err.empty());

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());

    VoicePlayInfo info;
    VoicePlayParameter params;

    // velrack = 100
    params.midiPitch = 30;
    params.midiVelocity = 127;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.gain, 1);

    params.midiVelocity = 64;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertClose(info.gain, .25f, .01f);

    // veltrack = 0
    params.midiPitch = 40;
    params.midiVelocity = 127;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.gain, 1);

    params.midiVelocity = 1;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertEQ(info.gain, 1);

    // default. veltrack should be 100
    params.midiPitch = 20;
    params.midiVelocity = 64;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertClose(info.gain, .25f, .01f);

    // veltrack 50, inherited
    // vel = 64
    params.midiPitch = 10;
    params.midiVelocity = 64;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertClose(info.gain, .63, .01f);  // number gotten from known-good.
                                        // but at least it's > .25 and < 1
}

static void testCompileMulPitchAndVelComplex2() {
    printf("\n----- testCompileMulPitchAndVelComplex2\n");
    const char* data = R"foo(
        <region>key=10 sample=a hivel=20  sample=a
        <region>key=10 sample=a lovel=21 hivel=90  sample=b
        <region>key=10 sample=a lovel=95  sample=c

        <region>key=20 sample=a hivel=24  sample=d
        <region>key=20 sample=a lovel=25 hivel=33  sample=e
        <region>key=20 sample=a lovel=91  sample=f

        <region>key=30 sample=a hivel=20  sample=h
        <region>key=30 sample=a lovel=34 hivel=90  sample=i
        <region>key=30 sample=a lovel=111  sample=j
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    VoicePlayInfo info;
    std::set<int> sampleIndicies;
    // ci->_dump(0);

    VoicePlayParameter params;
    params.midiPitch = 10;
    params.midiVelocity = 20;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 10;
    params.midiVelocity = 27;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);

    params.midiPitch = 10;
    params.midiVelocity = 91;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    params.midiPitch = 10;
    params.midiVelocity = 95;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    sampleIndicies.insert(info.sampleIndex);
}

static void testCompileGroupInherit() {
    const char* data = R"foo(
        //snare =====================================
        <group> amp_veltrack=98 key=40 loop_mode=one_shot lovel=101 hivel=127  // snare1 /////
        <region> sample=a
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    // ci->_dump(0);
    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 40;
    params.midiVelocity = 120;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);

    params.midiPitch = 41;
    params.midiVelocity = 120;
    ci->play(info, params, nullptr, 44100);
    assert(!info.valid);

    // This doesn't work, because we don't have exclusive velocity zones.
    // update - yes we do.
    // ci->play(info, 40, 100);
    // assert(!info.valid);
}

static void testCompileSimpleDrum() {
    printf("\n\n\n\n\\n\n------------- testCompileSimpleDrum -----------------------------\n");
    const char* data = R"foo(
        //snare =====================================
        <group> amp_veltrack=98 key=40 loop_mode=one_shot lovel=101 hivel=127 sample=g1
        <region> sample=a lorand=0 hirand=0.3
        <region> sample=b lorand=0.3 hirand=0.6
        <region> sample=c lorand=0.6 hirand=1.0

        //snareStick =====================================
        <group> amp_veltrack=98 volume=-11 key=41 loop_mode=one_shot lovel=1 hivel=127 seq_length=3 sample=g2
        <region> sample=d seq_position=1
        <region> sample=e seq_position=2
        <region> sample=f seq_position=3
    )foo";

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);

    //SQINFO("dumping drum patch");
    //ci->_dump(0);
    //SQINFO("done with dump");

    assertEQ(ci->_pool().size(), 6);
    VoicePlayInfo info;

    std::set<int> waves;
    for (int i = 0; i < 40; ++i) {
        VoicePlayParameter params;
        params.midiPitch = 40;
        params.midiVelocity = 110;
        ci->play(info, params, nullptr, 44100);

        assert(info.valid);
        assert(info.sampleIndex > 0);
        //   printf("sample index = %d\n", info.sampleIndex);
        waves.insert(info.sampleIndex);
    }

    assertEQ(waves.size(), 3);

    waves.clear();
    assertEQ(waves.size(), 0);

    VoicePlayParameter params;
    params.midiPitch = 41;
    params.midiVelocity = 64;
    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    waves.insert(info.sampleIndex);

    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    waves.insert(info.sampleIndex);

    ci->play(info, params, nullptr, 44100);
    assert(info.valid);
    assertGE(info.sampleIndex, 1);
    waves.insert(info.sampleIndex);

    // three should play all of them
    assertEQ(waves.size(), 3);
}

// test sorting of regions.
// Also tests comiling velocity layers
static void testCompileSort() {
    const char* data = R"foo(<region>key=12 lovel=51<region>key=12 hivel=10<region>key=12 lovel=11 hivel=50)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    std::vector<CompiledRegionPtr> regions;
    ci->_pool()._getAllRegions(regions);
    ci->_pool().sortByVelocity(regions);

    assertEQ(regions.size(), 3);
    assertEQ(regions[0]->lovel, 1);
    assertEQ(regions[0]->hivel, 10);

    assertEQ(regions[1]->lovel, 11);
    assertEQ(regions[1]->hivel, 50);

    assertEQ(regions[2]->lovel, 51);
    assertEQ(regions[2]->hivel, 127);
}

static void testComp(const std::string& path) {
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::goFile(FilePath(path), inst);
    if (!err.empty()) {
        //SQWARN("parse ret %s", err.c_str());
    }
    assert(err.empty());
    //  go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(ci);
    // ci->_dump(0);
    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 40;
    params.midiVelocity = 110;
    ci->play(info, params, nullptr, 44100);
}

static void testCompileBassoon() {
    testComp(R"foo(D:\samples\VSCO-2-CE-1.1.0\VSCO-2-CE-1.1.0\BassoonStac.sfz)foo");
}

static void testCompileGroupProbability() {
    const char* data = R"foo(
    
<group> 
lorand=0.0 hirand=0.5
<region>
sample=a.wav
lokey=43
hikey=46
pitch_keycenter=45
lovel=1
hivel=62
volume=12

<region>
sample=c.wav
key=100
lovel=1
hivel=62
volume=12

<group> 
lorand=0.5 hirand=1.5
<region>
sample=b.wav
lokey=43
hikey=46
pitch_keycenter=45
lovel=1
hivel=62
volume=12

<region>
sample=d.wav
key=100
lovel=1
hivel=62
volume=12
)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());
    assert(ci);
}

static void testCompileGroupProbability2() {
    const char* data = R"foo(
    
<group> 
lorand=0.0 hirand=0.5
<region>
sample=a.wav
lokey=43
hikey=46
pitch_keycenter=45
lovel=1
hivel=62
volume=12
<group> 
lorand=0.5 hirand=1.5
<region>
sample=b.wav
lokey=43
hikey=46
pitch_keycenter=45
lovel=1
hivel=62
volume=12
)foo";
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());
    assert(ci);
}

static void testSampleRate() {
    WaveLoader w;
    // TODO: change back to k18 orig when test is done
    w.addNextSample(FilePath("D:\\samples\\K18-Upright-Piano\\K18\\A0.f.wav"));

    w.loadNextFile();
    auto x = w.getInfo(1);
    assert(x->isValid());
    assertEQ(x->getSampleRate(), 48000);

    CompiledRegionPtr cr1 = st::makeRegion(R"foo(<region>sample=a key=60 seq_position=200)foo");

    SimpleVoicePlayer simplePlayer(cr1, 60, 1);

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 60;
    params.midiVelocity = 64;
    simplePlayer.play(info, params, &w, 44100);
    assert(info.valid);

    // if the sample is at 48k, and we are playing at 44.1k, then it will sound slow.
    // so we need to play faster to get it back up to pitch.
    float expectedTranspose = 48000.f / 44100.f;
    assert(info.needsTranspose);
#ifdef _SAMPFM
    {
        // octave pitches
        // (log2 (freq) = cv (+k)
#if 0
        float a1 = std::log2(1);
        float a2 = std::log2(2);
        float a3 = std::log2(4);

        float a4 = std::log2(44100.f / 48000.f);
        float a5 = 6;
#endif
    }
    const float yy = std::log2(expectedTranspose);
    assertClose(info.transposeV, yy, .0001);

#else
    a b
        assertClose(info.transposeAmt, expectedTranspose, .01);
#endif
}

static void testPlayVolumeAndTune() {
    const char* data = (R"foo(<region>sample=a key=44 tune=11 volume=-13)foo");

    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);
    assert(errc.empty());
    assert(ci);

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 44;
    params.midiVelocity = 127;

    // play with not wave and sr??
    ci->play(info, params, nullptr, 44100);

    const float expectedGain = float(AudioMath::gainFromDb(-13));

    assertEQ(info.gain, expectedGain);
    assert(info.needsTranspose);
    assert(info.needsTranspose);
    ;
#ifdef _SAMPFM
    const float cvOffset = 11.f / 1200.f;
    assertClose(info.transposeV, cvOffset, .0001);
#else
    // one octave is 1200 cents.
    // wikipedia tells me that 11 cetns is 1.006374 mult
    const float expectedTransposeMult = std::pow(2.f, 11.f / 1200.f);

    assertEQ(info.transposeAmt, expectedTransposeMult);
#endif
}

static void testCompileLoop() {
    const char* data = (R"foo(
          <group>loop_mode=loop_continuous
          <region>sample=a offset=100 loop_start=1234 loop_end=4567
          )foo");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);

    assertEQ(errc.unrecognizedOpcodes.size(), 0);

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 12;
    params.midiVelocity = 60;

    bool didKS = false;
    const CompiledRegion* region = ci->_pool().play(params, .5, didKS);
    
    assertEQ(region->loopData.offset, 100);
    assertEQ(region->loopData.loop_start, 1234);
    assertEQ(region->loopData.loop_end, 4567);
    assertEQ((int) region->loopData.loop_mode, (int) SamplerSchema::DiscreteValue::LOOP_CONTINUOUS);
}

static void testCompileLoop2() {
    const char* data = (R"foo(
          <group>oscillator=on
          <region>sample=a offset=100 end=200
          )foo");
    SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);

    assertEQ(errc.unrecognizedOpcodes.size(), 0);

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 12;
    params.midiVelocity = 60;

    bool didKS = false;
    const CompiledRegion* region = ci->_pool().play(params, .5, didKS);
    
    assertEQ(region->loopData.offset, 100);
    assertEQ(region->loopData.end, 200);
    assertEQ(region->loopData.oscillator, true);
 //   assertEQ((int) region->loopData.loop_mode, (int) SamplerSchema::DiscreteValue::LOOP_CONTINUOUS);
}

static void testCompileOscOff() {
    const char* data = (R"foo(
          <region>sample=a offset=100 end=200
          )foo");
      SInstrumentPtr inst = std::make_shared<SInstrument>();
    auto err = SParse::go(data, inst);

    SamplerErrorContext errc;
    auto ci = CompiledInstrument::make(errc, inst);

    assertEQ(errc.unrecognizedOpcodes.size(), 0);

    VoicePlayInfo info;
    VoicePlayParameter params;
    params.midiPitch = 12;
    params.midiVelocity = 60;

    bool didKS = false;
    const CompiledRegion* region = ci->_pool().play(params, .5, didKS);

     assertEQ(region->loopData.oscillator, false);

}

void testx2() {
    assert(parseCount == 0);
    assert(compileCount == 0);
   
    testWaveLoader0();
    testWaveLoader1Wav();
    testWaveLoader1Flac();

    testWaveLoader2();
    testWaveLoaderNot44();

    testPlayInfo();

    testCIKeysAndValues();
    testCIKeysAndValuesNotesLC();
    testCIKeysAndValuesNotesUC();
    testCIKeysAndValuesNotesSharp();

    testParseHeadingGlobalAndRegionCompiled();
    testParseHeadingGlobalWithKVAndRegionCompiled();
    testParseHeadingGlobalWitRegionKVCompiled();

    testCompiledRegion();
    testCompiledRegionAddedOpcodes();
    testCompiledRegionInherit();
    testCompiledRegionKey();
    testCompiledRegionVel();
    testCompiledRegionVel2();
    testCompiledRegionVel3();
    testCompiledRegionsRand();
    testCompiledRegionSeqIndex1();
    testCompiledRegionSeqIndex2();

    testCompileGroup0();
    testCompileGroup1();
    testCompileGroup2();

    // Let' put lots of very basic compilation tests here
    testCompileTreeOne();
    testCompileTreeTwo();
    testCompileKey();
    testCompileMultiPitch();
    testCompileMultiVel();
    testCompileMulPitchAndVelSimple();
    testCompileMulPitchAndVelComplex1();
    testCompileMulPitchAndVelComplex2();
    testCompileGroupProbability();
    testCompileBassoon();
    testCompileGroupInherit();

    // put here just for now
    testCompileMutliControls();

    // printf("fix testStreamXpose2\n");
    //testStreamXpose2();

    testCompileCrash();
    testCompileCrash2();

    assertEQ(compileCount, 0);

    testCompileSort();
    testCompileInst0();
    testCompileInst1();
    testCompileOverlap();

    testLoadWavesPiano();

    testCompileTranspose1();
    testSampleRate();
    testPlayVolumeAndTune();

#ifdef _SFZ_RANDOM
    testCompileSimpleDrum();
#else
    assert(false);
#endif
    testParseHeadingControl();
    testParseLabel();
    testParseInclude();

    testCompileAmpVel();
    testCompileAmpegRelease();
    testCompileLoop();
    testCompileLoop2();
    testCompileOscOff();
    // testCompileInstLinNumbers();

    assertEQ(parseCount, 0);
    assertEQ(compileCount, 0);
}
