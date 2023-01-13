#pragma once

#include "Divider.h"
#include "IComposite.h"
#include "IMidiPlayerHost.h"
#include "MidiAudition.h"
#include "MidiPlayer2.h"
#include "MidiSong.h"
#include "SeqClock.h"
#include "StepRecordInput.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class SeqDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

/**
 *
 */
template <class TBase>
class Seq : public TBase {
public:
    template <class Tx>
    friend class SeqHost;

    Seq(Module* module, MidiSongPtr song) : TBase(module),
                                            runStopProcessor(true),
                                            stepRecordInput(Seq<TBase>::inputs[CV_INPUT], Seq<TBase>::inputs[GATE_INPUT]) {
        init(song);
    }

    Seq(MidiSongPtr song) : TBase(),
                            runStopProcessor(true),
                            stepRecordInput(Seq<TBase>::inputs[CV_INPUT], Seq<TBase>::inputs[GATE_INPUT]) {
        init(song);
    }

    IMidiPlayerAuditionHostPtr getAuditionHost() {
        return audition;
    }

    /**
     * Set new song, perhaps after loading a new patch
     */
    void setSong(MidiSongPtr);

    enum ParamIds {
        CLOCK_INPUT_PARAM,
        UNUSED_TEMPO_PARAM,
        UNUSED_RUN_STOP_PARAM,  // the switch the user pushes (actually unused????
        PLAY_SCROLL_PARAM,
        RUNNING_PARAM,  // the invisible param that stores the run
        NUM_VOICES_PARAM,
        AUDITION_PARAM,
        STEP_RECORD_PARAM,
        REMOTE_EDIT_PARAM,  // also invisible. Are we enabled for host editing?
        NUM_PARAMS
    };

    enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        RUN_INPUT,
        GATE_INPUT,
        CV_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        CV_OUTPUT,
        GATE_OUTPUT,
        EOC_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        GATE_LIGHT,
        RUN_STOP_LIGHT,
        NUM_LIGHTS
    };

    void step() override;

    /** This should be called on audio thread
     * (but is it??)
     */
    void toggleRunStop() {
        runStopRequested = true;
    }

    using sr = StepRecordInput<typename TBase::Port>;

    // may be called from any thread, but meant for UI.
    bool poll(RecordInputData* p);

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<SeqDescription<TBase>>();
    }

    bool isRunning();

    float getPlayPosition() {
        // NOTE: this calculation is wrong. need subrange loop start, too
        double absTime = clock.getCurMetricTime();
        double loopDuration = player->getCurrentLoopIterationStart();

        // absTime - loop duration is the metric time of the start of the current loop,
        // if the overall loop starts at t=0
        double ret = absTime - loopDuration;

        // push it up to take into account subrange looping
        ret += player->getCurrentSubrangeLoopStart();
        return float(ret);
    }

    void onSampleRateChange() override;

    static std::vector<std::string> getClockRates();
    static std::vector<std::string> getPolyLabels();

private:
    GateTrigger runStopProcessor;
    void init(MidiSongPtr);
    void serviceRunStop();
    void allGatesOff();

    std::shared_ptr<MidiAudition> audition;
    SeqClock clock;
    Divider div;
    bool runStopRequested = false;

    bool wasRunning = false;
    std::shared_ptr<MidiPlayer2> player;
    /**
     * called by the divider every 'n' step calls
     */
    void stepn(int n);

    StepRecordInput<typename TBase::Port> stepRecordInput;
};

template <class TBase>
class SeqHost : public IMidiPlayerHost4 {
public:
    float delta = 0;
    SeqHost(Seq<TBase>* s) : seq(s) {
    }
    void setEOC(int track, bool eoc) override {
        assert(track == 0);
        if (eoc) {
            delta = delta + APP->engine->getSampleTime();
            eocTrigger.trigger(1e-3f);
        }
        float time = eocTrigger.process(APP->engine->getSampleTime());
        if (time)
            seq->outputs[Seq<TBase>::EOC_OUTPUT].setVoltage(10.f);
        else    
            seq->outputs[Seq<TBase>::EOC_OUTPUT].setVoltage(0.f);
    }
    void setGate(int track, int voice, bool gate) override {
        assert(track == 0);
#if defined(_MLOG)
        printf("host::setGate(%d) = (%d, %.2f) t=%f\n",
               voice,
               gate,
               seq->outputs[Seq<TBase>::CV_OUTPUT].getVoltage(voice),
               seq->getPlayPosition());
        fflush(stdout);
#endif
        seq->outputs[Seq<TBase>::GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f,voice);
    }
    void setCV(int track, int voice, float cv) override {
        assert(track == 0);
#if defined(_MLOG)
        printf("*** host::setCV(%d) = (%d, %.2f) t=%f\n",
               voice,
               seq->outputs[Seq<TBase>::GATE_OUTPUT].getVoltage(voice]) > 5,
               cv,
               seq->getPlayPosition());
        fflush(stdout);
#endif
        seq->outputs[Seq<TBase>::CV_OUTPUT].setVoltage(cv,voice);
    }
    void onLockFailed() override {
    }

    void resetClock() override {
        assert(false);
    }

private:
    Seq<TBase>* const seq;
};

template <class TBase>
void Seq<TBase>::init(MidiSongPtr song) {
    std::shared_ptr<IMidiPlayerHost4> host = std::make_shared<SeqHost<TBase>>(this);
    player = std::make_shared<MidiPlayer2>(host, song);
    audition = std::make_shared<MidiAudition>(host);

    div.setup(4, [this] {
        this->stepn(div.getDiv());
    });
    onSampleRateChange();
}

template <class TBase>
void Seq<TBase>::onSampleRateChange() {
    float secondsPerRetrigger = 1.f / 1000.f;
    float samplePerTrigger = secondsPerRetrigger * this->engineGetSampleRate();
    player->setSampleCountForRetrigger((int)samplePerTrigger);
    audition->setSampleTime(this->engineGetSampleTime());
}

template <class TBase>
void Seq<TBase>::setSong(MidiSongPtr newSong) {
    player->setSong(newSong);
}

template <class TBase>
void Seq<TBase>::step() {
    div.step();
}

template <class TBase>
bool Seq<TBase>::isRunning() {
    return TBase::params[RUNNING_PARAM].value > .5;
}

template <class TBase>
void Seq<TBase>::serviceRunStop() {
    runStopProcessor.go(TBase::inputs[RUN_INPUT].getVoltage(0));
    if (runStopProcessor.trigger() || runStopRequested) {
        runStopRequested = false;
        bool curValue = isRunning();
        curValue = !curValue;
        TBase::params[RUNNING_PARAM].setValue(curValue ? 1.f : 0.f);
    }
    TBase::lights[RUN_STOP_LIGHT].value = TBase::params[RUNNING_PARAM].value;
}

// may be called from any thread, but meant for UI.
template <class TBase>
bool Seq<TBase>::poll(RecordInputData* p) {
    return stepRecordInput.poll(p);
}

template <class TBase>
void Seq<TBase>::stepn(int n) {
    serviceRunStop();

    if (TBase::params[STEP_RECORD_PARAM].value > .5f) {
        stepRecordInput.step();
    }

    audition->enable(!isRunning() && (TBase::params[AUDITION_PARAM].value > .5f));
    audition->sampleTicksElapsed(n);
    // first process all the clock input params
    const SeqClock::ClockRate clockRate = SeqClock::ClockRate((int)std::round(TBase::params[CLOCK_INPUT_PARAM].value));
    //const float tempo = TBase::params[TEMPO_PARAM].value;
    clock.setup(clockRate, 0, TBase::engineGetSampleTime());

    // and the clock input
    const float extClock = TBase::inputs[CLOCK_INPUT].getVoltage(0);

    // now call the clock
    const float reset = TBase::inputs[RESET_INPUT].getVoltage(0);
    const bool running = isRunning();
    int samplesElapsed = n;

    // Our level sensitive reset will get turned into an edge in here
    SeqClock::ClockResults results = clock.update(samplesElapsed, extClock, running, reset);
    if (results.didReset) {
        player->reset(true);
        allGatesOff();  // turn everything off on reset, just in case of stuck notes.
    }

    player->updateToMetricTime(results.totalElapsedTime, float(clock.getMetricTimePerClock()), running);

    // copy the current voice number to the poly ports
    const int numVoices = (int)std::round(TBase::params[NUM_VOICES_PARAM].value + 1);
    TBase::outputs[CV_OUTPUT].setChannels(numVoices);
    TBase::outputs[GATE_OUTPUT].setChannels(numVoices);

    player->setNumVoices(0, numVoices);

    if (!running && wasRunning) {
        allGatesOff();
    }
    wasRunning = running;

    // light the gate LED is any voices playing
    bool isGate = false;
    for (int i = 0; i < numVoices; ++i) {
        isGate = isGate || (TBase::outputs[GATE_OUTPUT].getVoltage(i) > 5);
    }
    TBase::lights[GATE_LIGHT].setBrightness(isGate);

    player->updateSampleCount(n);
}

template <class TBase>
inline void Seq<TBase>::allGatesOff() {
    for (int i = 0; i < 16; ++i) {
        TBase::outputs[GATE_OUTPUT].setVoltage(0,i);
    }
}

template <class TBase>
inline std::vector<std::string> Seq<TBase>::getClockRates() {
    return SeqClock::getClockRates();
}

template <class TBase>
inline std::vector<std::string> Seq<TBase>::getPolyLabels() {
    return {
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "10",
        "11",
        "12",
        "13",
        "14",
        "15",
        "16",
    };
}

template <class TBase>
int SeqDescription<TBase>::getNumParams() {
    return Seq<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config SeqDescription<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case Seq<TBase>::CLOCK_INPUT_PARAM: {
            float low = 0;
            float high = int(SeqClock::ClockRate::NUM_CLOCKS) + 1;
            ret = {low, high, low, "Clock Rate"};
        } break;
        case Seq<TBase>::UNUSED_TEMPO_PARAM:
            ret = {40, 200, 120, "Tempo"};
            break;
        case Seq<TBase>::UNUSED_RUN_STOP_PARAM:
            ret = {0, 1, 0, "unused Run/Stop"};
            break;
        case Seq<TBase>::PLAY_SCROLL_PARAM:
            ret = {0, 1, 0, "Scroll during playback"};
            break;
        case Seq<TBase>::RUNNING_PARAM:
            ret = {0, 1, 1, "Running"};
            break;
        case Seq<TBase>::NUM_VOICES_PARAM:
            ret = {0, 15, 0, "Polyphony"};
            break;
        case Seq<TBase>::AUDITION_PARAM:
            ret = {0, 1, 1, "Audition"};
            break;
        case Seq<TBase>::STEP_RECORD_PARAM:
            ret = {0, 1, 1, "Step record enable"};
            break;
        case Seq<TBase>::REMOTE_EDIT_PARAM:
            ret = {0, 1, 0, "re"};
            break;
        default:
            assert(false);
    }
    return ret;
}
