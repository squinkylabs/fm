

#include "SqMidiEvent.h"
#include "TimeUtils.h"

#include "asserts.h"


static void testType()
{
    MidiNoteEvent note;
    assert(note.type == MidiEvent::Type::Note);
    MidiEndEvent end;
    assert(end.type == MidiEvent::Type::End);
}

static void testCast()
{
    MidiNoteEventPtr note = std::make_shared<MidiNoteEvent>();
    MidiEventPtr evn(note);

    MidiEndEventPtr end = std::make_shared<MidiEndEvent>();
    MidiEventPtr eve(end);

    assert(safe_cast<MidiNoteEvent>(evn));
    assert(safe_cast<MidiEndEvent>(eve));

    assert(!safe_cast<MidiNoteEvent>(eve));
    assert(!safe_cast<MidiEndEvent>(evn));

    assert(!safe_cast<MidiNoteEvent>(end));
    assert(!safe_cast<MidiEndEvent>(note));

    assert(safe_cast<MidiEvent>(note));
    assert(safe_cast<MidiEvent>(end));
}

static void testEqual()
{
    MidiNoteEventPtr note = std::make_shared<MidiNoteEvent>();
    MidiNoteEventPtr note2 = std::make_shared<MidiNoteEvent>();
    MidiEventPtr evn(note);

    MidiEndEventPtr end = std::make_shared<MidiEndEvent>();
    MidiEventPtr eve(end);

    assertEQ(note, evn);
    assertEQ(end, eve);
    assert (*note == *note2);
    assert(note != note2);

    assert(!(*note == *end));
    assert(*note != *end);
    assert(*note != *eve);
    assert(*evn != *end);
    assert(*note != *end);

    note2->pitchCV = 50;
    assert(*note != *note2);
}

static void testPitch()
{
    MidiNoteEvent n;
    n.pitchCV = 0;            // C4 in VCV
    auto pitch = n.getPitch();
    assert(pitch.first == 4);
    assert(pitch.second == 0);

    n.setPitch(pitch.first, pitch.second);
    assertEQ(n.pitchCV, 0);

    //************************************************

    n.pitchCV = 1.f / 12.f;
    pitch = n.getPitch();
    assert(pitch.first == 4);
    assert(pitch.second == 1);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV, 1.f / 12.f, .0001);

    //********************************************************

    n.pitchCV = 3 + 1.f / 12.f;
    pitch = n.getPitch();
    assertEQ(pitch.first, 4.0f + 3);
    assertEQ(pitch.second, 1);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV, 3 + 1.f / 12.f, .0001);

    //********************************************************

    n.pitchCV = 7.f / 12.f;
    pitch = n.getPitch();
    assertEQ(pitch.first, 4.0f);
    assertEQ(pitch.second, 7);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV, 7.f / 12.f, .0001);

    //********************************************************

    n.pitchCV = 3 + 7.f / 12.f;
    pitch = n.getPitch();
    assertEQ(pitch.first, 4.0f+3);
    assertEQ(pitch.second, 7);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV,3 +  7.f / 12.f, .0001);
}

static void testPitch2()
{
    assertNoMidi();     // check for leaks
    MidiNoteEvent n;
    n.pitchCV = -1;            // C3 in VCV
    auto pitch = n.getPitch();
    assert(pitch.first == 3);
    assert(pitch.second == 0);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV, -1, .0001);

    //******************************************************************************
   
    n.pitchCV = 0 + 1.f / 12.f + 1.f / 25.f;      // D4 plus less than half semi
    pitch = n.getPitch();
    assertEQ(pitch.first, 4);
    assertEQ(pitch.second, 1);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV, 1.f / 12.f, .0001);

    //**********************************************************
    n.pitchCV = 0 + 1.f / 12.f + 1.f / 23.f;      // D4 plus more than half semi
    pitch = n.getPitch();
    assertEQ(pitch.first, 4);
    assertEQ(pitch.second, 2);

    n.setPitch(pitch.first, pitch.second);
    assertClose(n.pitchCV, 2.f / 12.f, .0001);
}

static void testCopyCtor()
{
    assertNoMidi();     // check for leaks
    {
        MidiNoteEvent note;
        note.pitchCV = 1.12f;
        note.duration = 33.45f;
        note.startTime = 15.3f;
        MidiNoteEvent note2(note);
        assert(note == note2);

        MidiNoteEvent note3 = note;
        assert(note == note3);

        MidiEndEvent end;
        end.startTime = 234.5f;
        MidiEndEvent end2(end);
        assert(end == end2);

        MidiEndEvent end3 = end;
        assert(end == end3);
    }
    assertNoMidi();     // check for leaks
}

static void testAssignment()
{
    MidiNoteEvent note;
    note.pitchCV = 1.12f;
    note.duration = 33.45f;
    note.startTime = 15.3f;

    MidiNoteEvent note2;
    note2.pitchCV = 5;
    note2 = note;
    assert(note == note2);

    MidiEndEvent end;
    end.startTime = 234.5f;
    MidiEndEvent end2;
    end2.startTime = 5;
    end2 = end;
    assert(end == end2);
}


static void testEqualNote()
{
    MidiNoteEvent note1;
    MidiNoteEvent note2;
    assert(note1 == note2);
    note2.pitchCV = .5;
    assert(note1 != note2);
    note2 = note1;
    assert(note1 == note2);
    note2.duration = 5;
    assert(note1 != note2);
    MidiNoteEvent note3(note2);
    assert(note3 == note2);
}

static void testEqualEnd()
{
    MidiEndEvent end1;
    MidiEndEvent end2;
    assert(end1 == end2);
    end2.startTime = 55;
    assert(end1 != end2);
}

static void testLessMixed()
{
    MidiEndEvent end;
    MidiNoteEvent note;

    assert(note < end); // note type is first
}

static void testLessEvent()
{
    MidiEndEvent end;
    MidiNoteEvent note;

    MidiEvent& e1 = end;
    MidiEvent& e2 = note;
    assert(e2 < e1);

    const MidiEvent& e1b = note;
    const MidiEvent& e2b = note;
    assert(!(e2b < e1b));
    assert(!(e1b < e2b));
}
static void testLessEnd()
{
    MidiEndEvent end1;
    MidiEndEvent end2;
    MidiEndEvent end3;
    end3.startTime = 1000;

    assert(!(end1 < end2));
    assert(!(end2 < end1));

    assert(end1 < end3);
    assert(!(end3 < end1));
}


static void testLessNote()
{
    MidiNoteEvent note1;
    MidiNoteEvent note2;
   
    assert(!(note1 < note2));
    assert(!(note2 < note1));

    note2.startTime = 1;

    assert(note1 < note2);
    assert(!(note2 < note1));

    note1 = note2;
    note2.pitchCV += 1;

    assert(note1 < note2);
    assert(!(note2 < note1));

    note1 = note2;
    note2.duration += 1;

    assert(note1 < note2);
    assert(!(note2 < note1));

    // start time wins
    note1 = note2;
    note2.startTime += 1;
    note1.pitchCV += 1;
    note1.duration += 1;

    assert(note1 < note2);
    assert(!(note2 < note1));

    // if start time same pitch wins
    note1 = note2;
    note2.pitchCV += 1;
    note1.duration += 1;

    assert(note1 < note2);
    assert(!(note2 < note1));

    // if start time,pitch same duration wins
    note1 = note2;
    note2.duration += 1;

    assert(note1 < note2);
    assert(!(note2 < note1));
}

static void testClone()
{
    MidiNoteEvent note;
    note.pitchCV = 4.1f;
    note.duration = .6f;
    note.startTime = .22f;

    MidiEventPtr evt = note.clone();
    MidiNoteEventPtr note2 = note.clonen();
    assert(note == *evt);
    assert(note == *note2);

    MidiEndEvent end;
    end.startTime = 102345;
    MidiEventPtr end2 = end.clone();
    assert(end == *end2);
}


static void testTimeUtil0()
{
    int b = TimeUtils::time2bar(0);
    assertEQ(b, 0);
    b = TimeUtils::time2bar(1);
    assertEQ(b, 0);
    b = TimeUtils::time2bar(2);
    assertEQ(b, 0);
    b = TimeUtils::time2bar(3);
    assertEQ(b, 0);
    b = TimeUtils::time2bar(4);
    assertEQ(b, 1);
    b = TimeUtils::time2bar(5);
    assertEQ(b, 1);
}

static void testTimeUtil1()
{
    float t = TimeUtils::bar2time(0);
    assertEQ(t, 0);
    t = TimeUtils::bar2time(1);
    assertEQ(t, 4);
}

static void testTimeUtil2()
{
    float t = 0;
    auto x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(0, std::get<2>(x));

    t = TimeUtils::bar2time(1);
    x = TimeUtils::time2bbf(t);
    assertEQ(1, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(0, std::get<2>(x));

    t = TimeUtils::bar2time(100);
    x = TimeUtils::time2bbf(t);
    assertEQ(100, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(0, std::get<2>(x));
}

static void testTimeUtil3()
{
    float t = TimeUtils::quarterNote();
    auto x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(1, std::get<1>(x));
    assertEQ(0, std::get<2>(x));

    t = 2 * TimeUtils::quarterNote();
    x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(2, std::get<1>(x));
    assertEQ(0, std::get<2>(x));

    t = 3 * TimeUtils::quarterNote();
    x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(3, std::get<1>(x));
    assertEQ(0, std::get<2>(x));
}

static void testTimeUtil4()
{
    float t = TimeUtils::quarterNote() / 100.f;
    auto x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(1, std::get<2>(x));

    t = 40 * TimeUtils::quarterNote() / 100.f;
    x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(40, std::get<2>(x));

    t = 60 * TimeUtils::quarterNote() / 100.f;
    x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(60, std::get<2>(x));

    t = 99 * TimeUtils::quarterNote() / 100.f;
    x = TimeUtils::time2bbf(t);
    assertEQ(0, std::get<0>(x));
    assertEQ(0, std::get<1>(x));
    assertEQ(99, std::get<2>(x));
}

static void testTimeUtil5()
{
    float t = 0;
    auto x = TimeUtils::time2str(t);
    assert(x == "1.1.0");

    t = TimeUtils::quarterNote() + TimeUtils::bar2time(2);
    x = TimeUtils::time2str(t);
    assert(x == "3.2.0");

    t += 4.f * TimeUtils::quarterNote() / 100.f;
    x = TimeUtils::time2str(t);
    assert(x == "3.2.4");
}


static void testTimeUtil6()
{
    float t = 0;
    auto x = TimeUtils::time2str(t, 3);
    assert(x == "1.1.0");
    x = TimeUtils::time2str(t, 2);
    assert(x == "1.1");
    x = TimeUtils::time2str(t, 1);
    assert(x == "1");

    t = TimeUtils::quarterNote() + TimeUtils::bar2time(2);
    x = TimeUtils::time2str(t,3);
    assert(x == "3.2.0");
    x = TimeUtils::time2str(t, 2);
    assert(x == "3.2");
    x = TimeUtils::time2str(t, 1);
    assert(x == "3");
}

static void testTimeUtil7()
{
    float l = 8;
    auto x = TimeUtils::length2str(l);
    assert(x == "2.0.0");

    l = TimeUtils::quarterNote() + TimeUtils::bar2time(2);
    x = TimeUtils::length2str(l);
    assert(x == "2.1.0");

    l += 4.f * TimeUtils::quarterNote() / 100.f;
    x = TimeUtils::length2str(l);
    assert(x == "2.1.4");
}

static void testTimeUtilSimpleQuant()
{
    assertEQ(TimeUtils::quantize(10, 1, true), 10);
    assertEQ(TimeUtils::quantize(4, 10, true), 0);
    assertEQ(TimeUtils::quantize(6, 10, true), 10);

    assertEQ(TimeUtils::quantize(10, 1, false), 10);
    assertEQ(TimeUtils::quantize(4, 10, false), 10);
    assertEQ(TimeUtils::quantize(6, 10, false), 10);


    assertEQ(TimeUtils::quantize(-10, 1, true), -10);
    assertEQ(TimeUtils::quantize(-4, 10, true), 0);
    assertEQ(TimeUtils::quantize(-6, 10, true), -10);
}

static void testTimeUtilQ16()
{
    static float _16th = .25;
    static float _32nd = _16th / 2;
    static float _64th = _32nd / 2;

    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(_16th), _16th);
    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(_16th + _64th), 2 * _16th);

    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(_16th * 1.01f), 2 * _16th);

    // TODO: figure out what to do with shorter notes
    // If slightly short 16th should probably still be a 16th?
    // Maybe just base off 32nd?
  // assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(_16th * .99f), 1);

    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(2 * _16th), 2 * _16th);
    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(3 * _16th), 4 * _16th);
    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(5 * _16th), 8 * _16th);

    assertEQ(TimeUtils::getTimeAsPowerOfTwo16th(_64th), 0);
    
}



static void testPitchUtil0()
{
    // C
    float v = PitchUtils::pitchToCV(4, 0);
    assert(!PitchUtils::isAccidental(v));
    assert(PitchUtils::isC(v));

    // C#
    v = PitchUtils::pitchToCV(4, 1);
    assert(PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // D
    v = PitchUtils::pitchToCV(4, 2);
    assert(!PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // D#
    v = PitchUtils::pitchToCV(4, 3);
    assert(PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // E
    v = PitchUtils::pitchToCV(4, 4);
    assert(!PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // F
    v = PitchUtils::pitchToCV(4, 5);
    assert(!PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // F#
    v = PitchUtils::pitchToCV(4, 6);
    assert(PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // G
    v = PitchUtils::pitchToCV(4, 7);
    assert(!PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // G#
    v = PitchUtils::pitchToCV(4, 8);
    assert(PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // A
    v = PitchUtils::pitchToCV(4, 9);
    assert(!PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // A#
    v = PitchUtils::pitchToCV(4, 10);
    assert(PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

    // B
    v = PitchUtils::pitchToCV(4, 11);
    assert(!PitchUtils::isAccidental(v));
    assert(!PitchUtils::isC(v));

      // c
    v = PitchUtils::pitchToCV(5, 0);
    assert(!PitchUtils::isAccidental(v));
    assert(PitchUtils::isC(v));
}


static void testPitchUtil1()
{
    // C
    float v = PitchUtils::pitchToCV(4, 0);
    assert(PitchUtils::isC(v));

    v = PitchUtils::pitchToCV(4, 0) + (PitchUtils::semitone / 2.f) - .001f;
    assert(PitchUtils::isC(v));

    v = PitchUtils::pitchToCV(4, 0) + (PitchUtils::semitone / 2.f) + .001f;
    assert(!PitchUtils::isC(v));

    v = 0;
    assert(PitchUtils::isC(v));

    v = -.0001f;
    assert(PitchUtils::isC(v));
}

static void testPitchUtil2()
{       
    float v = PitchUtils::pitchToCV(4, 0);
    std::string s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "C:4");

    v = PitchUtils::pitchToCV(4, 1);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "C#:4");

    v = PitchUtils::pitchToCV(4, 2);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "D:4");

    v = PitchUtils::pitchToCV(4, 3);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "D#:4");

    v = PitchUtils::pitchToCV(4, 4);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "E:4");

    v = PitchUtils::pitchToCV(4, 5);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "F:4");

    v = PitchUtils::pitchToCV(4, 6);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "F#:4");

    v = PitchUtils::pitchToCV(4, 7);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "G:4");

    v = PitchUtils::pitchToCV(4, 8);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "G#:4");

    v = PitchUtils::pitchToCV(4, 9);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "A:4");

    v = PitchUtils::pitchToCV(4, 10);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "A#:4");

    v = PitchUtils::pitchToCV(4, 11);
    s = PitchUtils::pitch2str(v);
    assert(PitchUtils::pitch2str(v) == "B:4");
}

class CC
{
public:
    std::string c;
    bool operator < (const CC& rhs) const
    {
        return this->c < rhs.c;
    }
};


struct compareC
{
    bool operator() (const CC& lhs, const CC& rhs) const
    {
        printf("compare l=%s, r=%s\n", lhs.c.c_str(), rhs.c.c_str());
        return lhs.c < rhs.c;
    }
};

static void testPitchUtilMidi()
{
    const int midiMiddleC = 60;
    const float vcvMiddleC = 0.f;
    assertEQ(PitchUtils::midiToCV(midiMiddleC), vcvMiddleC);
    assertEQ(PitchUtils::pitchCVToMidi(vcvMiddleC), midiMiddleC);

    assertEQ(PitchUtils::midiToCV(midiMiddleC + 12), vcvMiddleC + 1);
    assertEQ(PitchUtils::pitchCVToMidi(vcvMiddleC + 1), midiMiddleC + 12);
}

static void testPitchUtilSemi()
{
    // octave up
    assertClose(PitchUtils::semitoneToFreqRatio(12), 2, .001);
    assertClose(PitchUtils::freqRatioToSemitone(2), 12, .001);

    assertClose(PitchUtils::semitoneToFreqRatio(-12), .5, .001);
    assertClose(PitchUtils::semitoneToFreqRatio(1), std::pow(2.f, 1 / 12.f), .001);

    for (float f = -30; f < 30; f += .23f) {
        const float x = PitchUtils::semitoneToFreqRatio(f);
        const float y = PitchUtils::freqRatioToSemitone(x);
        assertClose(y, f, .001);
    }

#if 0  // just a test of float vs double
    // x1	1.05946314	const float
    // x2	1.00000083	const float
    // x3	1.0594630943592953	const double
    // x4	1.0000000000000009	const double

    // fourCent	1.0023131618421728	const double
    // oneCent	1.0005777895065548	const double
    // emiPlusFourCent	1.0619138039623575	const double

    // what I see at end of semitone = 1.059463
    // and it's measuring 4 cents sharp
    // conclusion: the math is ok, but either the out of tune in "imaginary".
    // or it's happening at playback.

    const float x1 = PitchUtils::semitoneToFreqRatio(1);
    const float x2 = PitchUtils::freqRatioToSemitone(x1);

    const double x3 =  std::pow(2.0, 1.0 / 12.0);
    const double x4 = 12.0 * std::log2(x3);

    const double oneCent = std::pow(2.0, .01 / 12.0);
    const double fourCent = std::pow(2.0, .04 / 12.0);
    const double semiPlusFourCent = std::pow(2.0, 1.04 / 12.0);

    assert(false);
#endif
    
}

void  testMidiEvents()
{
    assertNoMidi();     // check for leaks
    testType();
    testCast();
    testEqual();
    testPitch();
    testPitch2();
    testCopyCtor();
    testAssignment();

    testEqualNote();
    testEqualEnd();
    testClone();

    testLessMixed();
    testLessEnd();
    testLessNote();
    testLessEvent();

    testTimeUtil0();
    testTimeUtil1();
    testTimeUtil2();
    testTimeUtil3();
    testTimeUtil4();
    testTimeUtil5();
    testTimeUtil6();
    testTimeUtil7();
    testTimeUtilSimpleQuant();
    testTimeUtilQ16();

    testPitchUtil0();
    testPitchUtil1();
    testPitchUtil2();

    testPitchUtilMidi();
    testPitchUtilSemi();
    assertNoMidi();     // check for leaks
}