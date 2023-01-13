
#include <assert.h>
#include <memory>
#include "MidiLock.h"
#include "MidiSelectionModel.h"
#include "MidiSong.h"
#include "MidiTrack.h"
#include "TestAuditionHost.h"

#include "asserts.h"

static void testCanInsert()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->pitchCV = 3.3f;
    ev->startTime = 55.f;
    assert(mt.size() == 0);
    mt.insertEvent(ev);
    assert(mt.size() == 1);

    mt.insertEnd(100);
   // MidiEndEventPtr end = std::make_shared<MidiEndEvent>();
   // end->startTime = 100;
   // mt.insertEvent(end);

    MidiEventPtr ev2 = mt._testGetVector()[0];
    assert(*ev2 == *ev);

    mt.assertValid();
}

static void testInsertSorted()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->pitchCV = 3.3f;
    ev->startTime = 11;

    MidiNoteEventPtr ev2 = std::make_shared<MidiNoteEvent>();
    ev2->pitchCV = 4.4f;
    ev2->startTime = 1;

    mt.insertEvent(ev);
    mt.insertEvent(ev2);

    mt.insertEnd(100);

    auto mv = mt._testGetVector();
    MidiEventPtr ev3 = mv.at(0);
    MidiNoteEventPtr no3 = safe_cast<MidiNoteEvent>(ev3);
    assert(ev3 == ev2);

    ev3 = mv.at(1);
    assert(ev3 == ev);

    mt.assertValid();
}

static void testDelete()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);

    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->pitchCV = 33;
    ev->startTime = 11;

    mt.insertEvent(ev);
    mt.deleteEvent(*ev);
    assert(mt.size() == 0);
}

static void testDelete2()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();

    ev->pitchCV = 33;
    ev->startTime = 11;
    mt.insertEvent(ev);

    MidiNoteEventPtr ev2 = std::make_shared<MidiNoteEvent>();
    ev2->pitchCV = 44;
    mt.insertEvent(ev2);

    mt.deleteEvent(*ev2);     // delete the second one, with pitch 44
    auto mv = mt._testGetVector();

    assert(mt.size() == 1);

    MidiNoteEventPtr no = safe_cast<MidiNoteEvent>(mv[0]);
    assert(no->pitchCV == 33);
}

static void testDelete3()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();

#ifndef NDEBUG
    assert(MidiEvent::_count > 0);
#endif
    ev->pitchCV = 44;
    ev->startTime = 11;
    mt.insertEvent(ev);

    MidiNoteEventPtr ev2 = std::make_shared<MidiNoteEvent>();
    ev2->pitchCV = 33;
    mt.insertEvent(ev2);

    MidiNoteEventPtr ev3 = std::make_shared<MidiNoteEvent>();
    ev3->pitchCV = 44;
    mt.deleteEvent(*ev);     // delete the first one, with pitch 44
    auto mv = mt._testGetVector();

    assert(mt.size() == 1);

    MidiNoteEventPtr no = safe_cast<MidiNoteEvent>(mv[0]);
    assert(no->pitchCV == 33);
}

static void testFind1()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->pitchCV = 5;
    mt.insertEvent(ev);
    auto found = mt.findEventDeep(*ev);
    assert(found->second == ev);
}

static void testTimeRange0()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 100;

    mt.insertEvent(ev);
    
    MidiTrack::iterator_pair its = mt.timeRange(99, 101);
    assert(its.first != its.second);
    auto count = std::distance(its.first, its.second);
    assertEQ(count, 1);

    its = mt.timeRange(101, 1000);
    assert(its.first == its.second);
    count = std::distance(its.first, its.second);
    assertEQ(count, 0);
}

static void testNoteTimeRange0()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 100;

    mt.insertEvent(ev);

    MidiTrack::note_iterator_pair its = mt.timeRangeNotes(99, 101);
    assert(its.first != its.second);
    auto count = std::distance(its.first, its.second);
    assertEQ(count, 1);

    its = mt.timeRangeNotes(101, 1000);
    assert(its.first == its.second);
    count = std::distance(its.first, its.second);
    assertEQ(count, 0);
}

//should skip over non-note events
static void testNoteTimeRange0Mixed()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);

    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 100;
    mt.insertEvent(ev);

    MidiTestEventPtr tev = std::make_shared<MidiTestEvent>();
    tev->startTime = 100.1f;
    mt.insertEvent(tev);

    MidiTrack::note_iterator_pair its = mt.timeRangeNotes(99, 101);
    assert(its.first != its.second);
    auto count = std::distance(its.first, its.second);
    assertEQ(count, 1);

    its = mt.timeRangeNotes(101, 1000);
    assert(its.first == its.second);
    count = std::distance(its.first, its.second);
    assertEQ(count, 0);
}

static void testTimeRange1()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();


    ev->startTime = 100;
    mt.insertEvent(ev);

    ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 110;
    mt.insertEvent(ev);

    ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 120;
    mt.insertEvent(ev);

    ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 130;
    mt.insertEvent(ev);

    MidiTrack::iterator_pair its = mt.timeRange(110, 120);
    assert(its.first != its.second);
    auto count = std::distance(its.first, its.second);
    assertEQ(count, 2);
}


static void testNoteTimeRange1()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    MidiTestEventPtr evt = std::make_shared<MidiTestEvent>();

    ev->startTime = 100;
    mt.insertEvent(ev);

    ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 110;
    mt.insertEvent(ev);

    ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 120;
    mt.insertEvent(ev);

    ev = std::make_shared<MidiNoteEvent>();
    ev->startTime = 130;
    mt.insertEvent(ev);


    evt->startTime = 111;
    mt.insertEvent(evt);


    MidiTrack::note_iterator_pair its = mt.timeRangeNotes(110, 120);
    assert(its.first != its.second);
    auto count = std::distance(its.first, its.second);
    assertEQ(count, 2);
}

static void testSeekTime1()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
 //   MidiTestEventPtr evt = std::make_shared<MidiTestEvent>();

    ev->startTime = 100;
    mt.insertEvent(ev);

    auto it = mt.seekToTimeNote(100);       // should find note at pitch
    assert(it != mt.end());

    it = mt.seekToTimeNote(101);        // nothing here
    assert(it == mt.end());
}


static void testSeekTime2()
{
    auto lock = MidiLock::make();
    MidiTrack mt(lock);
    MidiLocker l(lock);
    MidiNoteEventPtr ev = std::make_shared<MidiNoteEvent>();
    MidiTestEventPtr evt = std::make_shared<MidiTestEvent>();

    ev->startTime = 100;
    mt.insertEvent(ev);

    evt->startTime = 110;
    mt.insertEvent(evt);

    auto it = mt.seekToTimeNote(100);       // should find note at pitch
    assert(it != mt.end());

    it = mt.seekToTimeNote(101);        // nothing here
    assert(it == mt.end());
}

static void testSameTime()
{
    printf("ADD A TEST FOR SAME TIME\n");
}

static void testSong()
{
    auto p = MidiSong::makeTest(MidiTrack::TestContent::eightQNotes, 0);
    assert(_mdb > 0);
    p->assertValid();
}

static void testQuant()
{
    float pitchCV = 0;
    const float quarterStep = PitchUtils::semitone / 2.f;
    const float eighthStep = PitchUtils::semitone / 4.f;
    const int  x = PitchUtils::cvToSemitone(pitchCV);

    assertEQ(PitchUtils::cvToSemitone(pitchCV + eighthStep), x);     // round to middle
    assertEQ(PitchUtils::cvToSemitone(pitchCV - eighthStep), x);     // round to middle
    assertEQ(PitchUtils::cvToSemitone(pitchCV + (quarterStep * .99f)), x);     // round to middle
    assertEQ(PitchUtils::cvToSemitone(pitchCV - (quarterStep * .99f)), x);     // round to middle

    assertEQ(PitchUtils::cvToSemitone(pitchCV + (quarterStep * 1.01f)), x+1);     // round to middle
    assertEQ(PitchUtils::cvToSemitone(pitchCV - (quarterStep * 1.01f)), x-1);     // round to middle
 
    // TODO: why isn't this 64 - looks at specs for VCV and MIDI
    //assertEQ(x, 64);
}



static void testQuantRel()
{
    assertEQ(PitchUtils::deltaCVToSemitone(0), 0);
    assertEQ(PitchUtils::deltaCVToSemitone(PitchUtils::semitone), 1);
    assertEQ(PitchUtils::deltaCVToSemitone(-PitchUtils::semitone), -1);

    const float halfSemi = PitchUtils::semitone / 2.f;
    assertEQ(PitchUtils::deltaCVToSemitone(halfSemi - .001f), 0);
    assertEQ(PitchUtils::deltaCVToSemitone(halfSemi + .001f), 1);

    assertEQ(PitchUtils::deltaCVToSemitone(-halfSemi + .001f), 0);
    assertEQ(PitchUtils::deltaCVToSemitone(-halfSemi - .001f), -1);
}

static void testPitchRoundTrip()
{
    int x = PitchUtils::cvToSemitone(0);
    assertEQ(PitchUtils::semitoneToCV(x), 0);
}

void testMidiDataModel()
{
    assertNoMidi();     // check for leaks
    testCanInsert();
    testInsertSorted();
    testDelete();
    testDelete2();
    testDelete3();
    testFind1();
    testTimeRange0();
    testNoteTimeRange0();
    testNoteTimeRange0Mixed();
    testTimeRange1();
    testNoteTimeRange1();
    testSameTime();
    testSeekTime1();
    testSeekTime2();
    testSong();
    testQuant();
    testQuantRel();
    testPitchRoundTrip();



    assertNoMidi();     // check for leaks
}