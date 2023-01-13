#pragma once

#include <stdint.h>
#include <memory>
#include <assert.h>
#include "asserts.h"

#include "PitchUtils.h"

// forward declare smart pointers
class MidiEvent;
class MidiEndEvent;
class MidiNoteEvent;
class MidiTestEvent;
class MidiEndEvent;
using MidiEndEventPtr = std::shared_ptr<MidiEndEvent>;
using MidiTestEventPtr = std::shared_ptr<MidiTestEvent>;
using MidiEventPtr = std::shared_ptr<MidiEvent>;
using MidiEventPtrC = std::shared_ptr<const MidiEvent>;
using MidiNoteEventPtr = std::shared_ptr<MidiNoteEvent>;
using MidiNoteEventPtrC = std::shared_ptr<const MidiNoteEvent>;

/**
 * Abstract base class for all events
 */
class MidiEvent
{
public:
    typedef float time_t;
    enum class Type
    {
        Note,
        End,
        Test
    };

    Type type = Type::Test;

    /**
     * time units are floats, 1.0 == quarter note
     */
    time_t startTime = 0;

    bool operator < (const MidiEvent&) const;
    bool operator == (const MidiEvent&) const;
    bool operator != (const MidiEvent&) const;

    virtual MidiEventPtr clone() const = 0;

    virtual void assertValid() const;

    virtual ~MidiEvent()
    {
#ifndef NDEBUG
        --_count;
#endif
    }
#ifndef NDEBUG
    static int _count;      // for debugging - reference count
#endif

protected:
    MidiEvent()
    {
#ifndef NDEBUG
        ++_count;
#endif
    }
    MidiEvent(const MidiEvent& e)
    {
#ifndef NDEBUG
        ++_count;
#endif
        this->startTime = e.startTime;
    }

public:
    virtual bool isEqualBase(const MidiEvent& other) const
    {
        return this->startTime == other.startTime;
    }
    virtual bool isLessBase(const MidiEvent& other) const
    {
        assert(this->type == other.type);
        return this->startTime < other.startTime;
    }
    virtual bool isEqual(const MidiEvent& other) const = 0;
    virtual bool isLess(const MidiEvent& other) const = 0;
};


inline bool MidiEvent::operator < (const MidiEvent& other) const
{
    if (other.type != this->type) {
        return this->type < other.type;
    }
    return isLess(other);
}

inline bool MidiEvent::operator == (const MidiEvent& other) const
{
    if (other.type != this->type) {
        return false;
    }
    return isEqual(other);
}

inline bool MidiEvent::operator != (const MidiEvent& other) const
{
    return !(*this == other);
}

inline void MidiEvent::assertValid() const
{
    assertGE(startTime, 0);
}

/**
 * Derived pointers must provide an implementation for casting
 * base (MidiEventPtr) to derived pointer
 */
template<typename T, typename Q>
inline std::shared_ptr<T> safe_cast(std::shared_ptr<Q>)
{
    // default implementation always fails.
    // this avoids linker errors for unimplemented cases
    return nullptr;
}

/********************************************************************
**
**           MidiNoteEvent
**
********************************************************************/

class MidiNoteEvent : public MidiEvent
{
public:
    MidiNoteEvent()
    {
        type = Type::Note;
    }

    MidiNoteEvent(const MidiNoteEvent& n) : MidiEvent(n)
    {
        type = Type::Note;
        this->pitchCV = n.pitchCV;
        this->duration = n.duration;
    }

    MidiNoteEvent& operator=(const MidiNoteEvent& n)
    {
        assert (type == Type::Note);
        this->pitchCV = n.pitchCV;
        this->duration = n.duration;
        this->startTime = n.startTime;
        return *this;
    }

    /**
     * Pitch is VCV standard 1V/8
     */
    float pitchCV = 0;
    float duration = 1;
    void assertValid() const override;

    void setPitch(int octave, int semi);
    std::pair<int, int> getPitch() const;
    float endTime() const;

    virtual MidiEventPtr clone() const override;
    MidiNoteEventPtr clonen() const;

protected:
    virtual bool isEqual(const MidiEvent&) const override;
    virtual bool isLess(const MidiEvent&) const override;
};

inline std::pair<int, int> MidiNoteEvent::getPitch() const
{
    return PitchUtils::cvToPitch(pitchCV);
}

inline void MidiNoteEvent::setPitch(int octave, int semi)
{
    pitchCV = PitchUtils::pitchToCV(octave, semi);
}

inline float MidiNoteEvent::endTime() const
{
    return startTime + duration;
}

inline void MidiNoteEvent::assertValid() const
{
    MidiEvent::assertValid();
    assertLE(pitchCV, 10);
    assertGE(pitchCV, -10);
    assertGT(duration, 0);
}

inline  bool MidiNoteEvent::isEqual(const MidiEvent& other) const
{
    const MidiNoteEvent* otherNote = static_cast<const MidiNoteEvent*>(&other);
    return other.isEqualBase(*this) &&
        this->pitchCV == otherNote->pitchCV &&
        this->duration == otherNote->duration;
}

inline  bool MidiNoteEvent::isLess(const MidiEvent& other) const
{
    assert(this->type == other.type);

    const MidiNoteEvent* otherNote = static_cast<const MidiNoteEvent*>(&other);

    if (!this->isEqualBase(*otherNote)) {
        return this->isLessBase(*otherNote);
    }

    if (this->pitchCV != otherNote->pitchCV) {
        return (this->pitchCV < otherNote->pitchCV);
    }

    return this->duration < otherNote->duration;
}

template<>
inline MidiNoteEventPtr safe_cast(std::shared_ptr<MidiEvent> ev)
{
    MidiNoteEventPtr note;
    if (ev->type == MidiEvent::Type::Note) {
        note = std::static_pointer_cast<MidiNoteEvent>(ev);
    }
    return note;
}

template<>
inline std::shared_ptr<MidiEvent> safe_cast(std::shared_ptr<MidiNoteEvent> ev)
{
    return ev;
}

inline MidiNoteEventPtr MidiNoteEvent::clonen() const
{
    return std::make_shared<MidiNoteEvent>(*this);
}

inline MidiEventPtr MidiNoteEvent::clone() const
{
    return this->clonen();
}
/********************************************************************
**
**           MidiEndEvent
**
********************************************************************/

class MidiEndEvent : public MidiEvent
{
public:
    void assertValid() const override;
    MidiEndEvent()
    {
        type = Type::End;
    }

    MidiEndEvent(const MidiEndEvent& e) : MidiEvent(e)
    {
        type = Type::End;
    }

    virtual MidiEventPtr clone() const override;
    MidiEndEventPtr clonee() const;
protected:
    virtual bool isEqual(const MidiEvent&) const override;
    virtual bool isLess(const MidiEvent&) const override;
};

inline void MidiEndEvent::assertValid() const
{
    MidiEvent::assertValid();
}

inline  bool MidiEndEvent::isLess(const MidiEvent& other) const
{
    return this->isLessBase(other);
}

inline  bool MidiEndEvent::isEqual(const MidiEvent& other) const
{
    //const MidiEndEvent* otherNote = static_cast<const MidiEndEvent*>(&other);
    return other.isEqualBase(*this);
}

template<>
inline std::shared_ptr<MidiEndEvent> safe_cast(std::shared_ptr<MidiEvent> ev)
{
    std::shared_ptr<MidiEndEvent> endev;
    if (ev->type == MidiEvent::Type::End) {
        endev = std::static_pointer_cast<MidiEndEvent>(ev);
    }
    return endev;
}

template<>
inline std::shared_ptr<MidiEvent> safe_cast(std::shared_ptr<MidiEndEvent> ev)
{
    return ev;
}

inline MidiEventPtr MidiEndEvent::clone() const
{
    return std::make_shared<MidiEndEvent>(*this);
}


/********************************************************************
**
**           MidiTestEvent
** (just for unit tests)
********************************************************************/

class MidiTestEvent : public MidiEvent
{
public:
    void assertValid() const override;
    MidiTestEvent()
    {
        type = Type::Test;
    }
    virtual MidiEventPtr clone() const override;
protected:
    virtual bool isEqual(const MidiEvent&) const override;
    virtual bool isLess(const MidiEvent&) const override;
};

inline void MidiTestEvent::assertValid() const
{
    MidiEvent::assertValid();
}

inline  bool MidiTestEvent::isLess(const MidiEvent& other) const
{
    assert(false);
    return false;
}

inline  bool MidiTestEvent::isEqual(const MidiEvent& other) const
{
    return other.isEqualBase(*this);
}

template<>
inline std::shared_ptr<MidiTestEvent> safe_cast(std::shared_ptr<MidiEvent> ev)
{
    std::shared_ptr<MidiTestEvent> test;
    if (ev->type == MidiEvent::Type::Test) {
        test = std::static_pointer_cast<MidiTestEvent>(ev);
    }
    return test;
}

template<>
inline std::shared_ptr<MidiEvent> safe_cast(std::shared_ptr<MidiTestEvent> ev)
{
    return ev;
}

inline MidiEventPtr MidiTestEvent::clone() const
{
    return std::make_shared<MidiTestEvent>(*this);
}
