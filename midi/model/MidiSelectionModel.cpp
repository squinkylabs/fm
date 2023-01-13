
#include "IMidiPlayerHost.h"
#include "SqMidiEvent.h"
#include "MidiSelectionModel.h"
#include "MidiTrack.h"

#include <assert.h>
extern int _mdb;
MidiSelectionModel::MidiSelectionModel(IMidiPlayerAuditionHostPtr aud) : auditionHost(aud)
{
    ++_mdb;
}

MidiSelectionModel::MidiSelectionModel(IMidiPlayerAuditionHostPtr aud, bool selectAll) :
    auditionHost(aud),
    allIsSelected(selectAll)
{
    ++_mdb;
}

MidiSelectionModel::~MidiSelectionModel()
{
    --_mdb;
}


bool MidiSelectionModel::CompareEventPtrs::operator() (const std::shared_ptr<MidiEvent>& lhs, const std::shared_ptr<MidiEvent>& rhs) const

{
    MidiEvent& le = *lhs;
    MidiEvent& re = *rhs;
    return  le < re;
}

bool MidiSelectionModel::isAuditionSuppressed() const
{
    return auditionSuppressed;
}

void MidiSelectionModel::setAuditionSuppressed(bool b)
{
    // TODO: is this used now?
    auditionSuppressed = b;
}

void MidiSelectionModel::select(std::shared_ptr<MidiEvent> event)
{
    clear();
    assert(selection.empty());
    add(event);
}

void MidiSelectionModel::extendSelection(std::shared_ptr<MidiEvent> event)
{
    allIsSelected = false;
    add(event);
}

void MidiSelectionModel::addToSelection(std::shared_ptr<MidiEvent> event, bool keepExisting)
{
    allIsSelected = false;
    auto it = selection.find(event);
    if (it != selection.end()) {
        // if note is already in, then don't clear and re-add
        return;
    }

    if (!keepExisting) {
        selection.clear();
    }
    add(event);
}

void MidiSelectionModel::removeFromSelection(std::shared_ptr<MidiEvent> event)
{
    auto it = selection.find(event);
    assert(it != selection.end());
    if (it != selection.end()) {
        selection.erase(it);
    }
}

MidiSelectionModel::const_iterator MidiSelectionModel::begin() const
{
    return selection.begin();
}

MidiSelectionModel::const_iterator MidiSelectionModel::end() const
{
    return selection.end();
}


MidiSelectionModel::const_reverse_iterator MidiSelectionModel::rbegin() const
{
    return selection.rbegin();
}

MidiSelectionModel::const_reverse_iterator MidiSelectionModel::rend() const
{
    return selection.rend();
}

void MidiSelectionModel::clear()
{
    selection.clear();
    allIsSelected = false;
}

void MidiSelectionModel::add(MidiEventPtr evt)
{
    auto found = selection.find(evt);
    if (found != selection.end()) {
        // if the event is already there, don't do anything.
        return;
    }

    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(evt);
    if (note && !auditionSuppressed) {
        auditionHost->auditionNote(note->pitchCV);
    }
    selection.insert(evt);
}

bool MidiSelectionModel::isSelected(MidiEventPtr evt) const
{
    assert(evt);
    auto it = std::find(selection.begin(), selection.end(), evt);
    return it != selection.end();
}

MidiEventPtr MidiSelectionModel::getLast()
{
    MidiEventPtr ret;
    float lastTime = 0;
    for (auto it : selection) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it);
        if (note) {
            float noteEnd = note->startTime + note->duration;
            if (noteEnd > lastTime) {
                ret = note;
                lastTime = noteEnd;
            }
        } else {
            float end = it->startTime;
            if (end > lastTime) {
                ret = it;
                lastTime = end;
            }
        }
    }
    return ret;
}

class NullAudition : public IMidiPlayerAuditionHost
{
public:
    void auditionNote(float pitch) override
    {

    }
};

MidiSelectionModelPtr MidiSelectionModel::clone() const
{
    // Clones ones never need to drive audition
    auto nullAudition = std::make_shared<NullAudition>();
    MidiSelectionModelPtr ret = std::make_shared<MidiSelectionModel>(nullAudition);
    for (auto it : selection) {
        MidiEventPtr clonedEvent = it->clone();
        ret->add(clonedEvent);
    }
    return ret;
}

bool MidiSelectionModel::isSelectedDeep(MidiEventPtr evt) const
{
    auto it = std::find_if(begin(), end(), [evt](MidiEventPtr ev) {
        return *ev == *evt;
    });

    return it != end();
}

std::vector<MidiEventPtr> MidiSelectionModel::asVector() const
{
    std::vector<MidiEventPtr> ret;
    for (auto it : selection) {
        MidiEventPtr event = it;
        ret.push_back(event);
    }
    return ret;
}

IMidiPlayerAuditionHostPtr MidiSelectionModel::_testGetAudition()
{
    return auditionHost;
}

bool MidiSelectionModel::isAllSelected() const
{
    return allIsSelected;
}

 void MidiSelectionModel::selectAll(MidiTrackPtr track)
 {
    clear();
    for (auto it : *track) {
        MidiEventPtr orig = it.second;
        if (orig->type != MidiEvent::Type::End) {
            extendSelection(orig);
        }
    }
    allIsSelected = true;
 }
