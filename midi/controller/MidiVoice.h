#pragma once


// #define _MLOG
class IMidiPlayerHost4;

/**
 * Midi voice represents one "voice" of playback, so typically
 * each voice will drive one CV/Gate pair.
 *
 * Voices are pretty dumb, but what the are responsible for is:
 *      reporting status back to note assigner.
 *      playing a note "immediately"
 *      shutting off the note when it expires
 *      re-triggering when playing two back-to-back notes.
 *
 * Re-triggering defined as so: When a new note is played at exactly the same time as 
 * the end of the previous note in that voice, then it will output a low gate for one millisecond, 
 * before playing the requested note.
 */
class MidiVoice
{
public:
    enum class State {Idle, Playing, ReTriggering };
    void setHost(IMidiPlayerHost4*);
    void setIndex(int);
    void setTrack(int);
    void setSampleCountForRetrigger(int samples);

    /**
     * Will always play the note, not matter what state it is in.
     * Clever voice assignment should have been done long before calling this.
     * Also, voice should have already been updated to the start time of the note (current
     * metric time)
     *
     * @param pitch is the pitch in our standard 1V/8 units
     * @param current time is the metric time of "now", and also the time when the note will 
     * be played.
     * @param endTime is the start time + duration for the note.
     */
    void playNote(float pitch, double currentTime, float endTime);

    /**
     * Advance clock, sending any data that needs to be sent.
     * @param quarterNotes is the absolute metric time, in our standard quarter note units.
     * @returns true if it did anything.
     */
    bool updateToMetricTime(double quarterNotes);

    void updateSampleCount(int samples);

    /**
     * resets all internal playback state.
     * @param clearGate will set the host's gate low, if true
     */
    void reset(bool clearGate);

    State state() const;
    float pitch() const;

    // these are only for debugging
    int _getIndex() const;
    void _setState(State);
   
private:
    double noteOffTime = -1;        // the absolute metric time when the 
                                    // currently playing note should stop
    float curPitch= -100;           // the pitch of the last note played in this voice

    double lastNoteOffTime = -1;

    /**
     * the "delayed note" is the note we would have played if
     * we hand not needed to re-trigger.
     */
    float delayedNotePitch;
    double delayedNoteEndtime;

    /**
     * during re-trigger, this counts down. when it hits zero, retrigger is over
     */
    int retriggerSampleCounter = 0;
    int numSamplesInRetrigger = 0;

    IMidiPlayerHost4* host = nullptr;

    State curState = State::Idle;

    /**
     * Index is the voice number was are associated with, 0..15.
     * These can't be const, as we need to have a default ctor.
     */
    int index = 0;

    int track = 0;

    void setGate(bool);
    void setCV(float);
};