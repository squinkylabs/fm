
#include "MidiFileProxy.h"

#include "MidiFile.h"
#include "MidiLock.h"
#include "MidiSong.h"
#include "TimeUtils.h"

//#include <direct.h>
#include <assert.h>

#include <iostream>

bool MidiFileProxy::save(MidiSongPtr song, const std::string& filePath) {
    smf::MidiFile midiFile;
    midiFile.setTPQ(480);
    const int ppq = midiFile.getTPQ();
    assert(ppq == 480);

    MidiTrackPtr track = song->getTrack(0);  // for now we always have one track here
    const int outputTkNum = 0;
    for (auto it : *track) {
        MidiEventPtr evt = it.second;
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(evt);
        MidiEndEventPtr end = safe_cast<MidiEndEvent>(evt);
        if (note) {
            int startTick = int(note->startTime * ppq);
            int duration = int(note->duration * ppq);
            int key = PitchUtils::pitchCVToMidi(note->pitchCV);
            if (key < 0 || key > 127) {
                fprintf(stderr, "pitch outside MIDI range, not writing to file");
            } else {
                const int endTick = startTick + duration;
                const int outputMidiChannel = 0;
                const int velocity = 0x3f;
                midiFile.addNoteOn(outputTkNum, startTick, outputMidiChannel, key, velocity);
                midiFile.addNoteOff(outputTkNum, endTick, outputMidiChannel, key);
            }
        } else if (end) {
            int tick = int(end->startTime * ppq);
            std::vector<smf::uchar> data;
            data.push_back(0);
            assert(data.size() == 1);
            midiFile.addMetaEvent(outputTkNum, tick, 0x2f, data);
        }
    }

    midiFile.sortTracks();
    return midiFile.write(filePath);
    return false;
}

MidiSongPtr MidiFileProxy::load(const std::string& filename) {
    smf::MidiFile midiFile;

    bool b = midiFile.read(filename);
    if (!b) {
        printf("open failed\n");
        return nullptr;
    }
    midiFile.makeAbsoluteTicks();
    midiFile.linkNotePairs();

    assert(midiFile.isAbsoluteTicks());

    MidiSongPtr song = std::make_shared<MidiSong>();
    MidiTrackPtr track = getFirst(song, midiFile);
    if (!track) {
        //printf("get first failed\n");
        return nullptr;
    }
    song->addTrack(0, track);
    song->assertValid();
    return song;
}

MidiTrackPtr MidiFileProxy::getFirst(MidiSongPtr song, smf::MidiFile& midiFile) {
    MidiLocker l(song->lock);
    const double ppq = midiFile.getTicksPerQuarterNote();

    const int tracks = midiFile.getTrackCount();

    bool foundNotes = false;
    for (int track = 0; track < tracks; track++) {
        MidiTrackPtr newTrack = std::make_shared<MidiTrack>(song->lock);
        ;
        for (int event = 0; event < midiFile[track].size(); event++) {
            smf::MidiEvent& evt = midiFile[track][event];
            if (evt.isNoteOn()) {
                const double dur = double(evt.getTickDuration()) / ppq;
                const double start = double(evt.tick) / ppq;
                const float pitch = PitchUtils::midiToCV(evt.getKeyNumber());

                MidiNoteEventPtr note = std::make_shared<MidiNoteEvent>();
                note->startTime = float(start);
                note->duration = float(dur);
                note->pitchCV = float(pitch);

                newTrack->insertEvent(note);
                foundNotes = true;
            } else if (evt.isEndOfTrack()) {
                const float start = float(double(evt.tick) / ppq);

                // quantize end point to 1/16 note, because that's what we support
                float startq = (float)TimeUtils::quantize(start, .25f, false);
                if (startq < start) {
                    startq += .25f;
                }
                newTrack->insertEnd(startq);
            } else if (evt.isTrackName()) {
                // std::string name = evt.getMetaContent();
                // printf("track name is %s\n", name.c_str());
            }
        }
        if (foundNotes) {
            // newTrack->_dump();
            return newTrack;
        }
    }
    return nullptr;
}