#pragma once

#include <memory>
#include <atomic>
#include <stdint.h>

class SqCommand;
class Sq4Command;
class SequencerWidget;
class Sequencer4Widget;
class MidiSequencer;
class MidiSequencer4;
using MidiSequencerPtr = std::shared_ptr<MidiSequencer>;
using MidiSequencer4Ptr = std::shared_ptr<MidiSequencer4>;

#if defined(__PLUGIN)
#define __USE_VCV_UNDO
#endif

#ifdef __USE_VCV_UNDO

class UndoRedoStack
{
public:
    UndoRedoStack() {
         moduleId = 0;
    }
    // It's a bit of a hack to have a version that doesn't require a widget,
    // But since the widget param is rarely used... 
    // execute the command, make undo record
    void execute(MidiSequencerPtr, std::shared_ptr<SqCommand>);
    void execute(MidiSequencerPtr, SequencerWidget*, std::shared_ptr<SqCommand>);
    void execute4(MidiSequencer4Ptr, std::shared_ptr<Sq4Command>);
    void execute4(MidiSequencer4Ptr, Sequencer4Widget*, std::shared_ptr<Sq4Command>);
    void setModuleId(int64_t, bool fromUI);
private:
    std::atomic<int64_t> moduleId;
};

using UndoRedoStackPtr = std::shared_ptr<UndoRedoStack>;

#else

#include <memory>
#include <list>

class UndoRedoStack
{
public:
    bool canUndo() const;
    bool canRedo() const;

    // It's a bit of a hack to have a version that doesn't require a widget,
    // But since the widget param is rarely used... 
    // execute the command, make undo record
    void execute(MidiSequencerPtr, std::shared_ptr<SqCommand>);
    void execute4(MidiSequencer4Ptr, std::shared_ptr<Sq4Command>);
    void execute(MidiSequencerPtr, SequencerWidget*, std::shared_ptr<SqCommand>);
    void undo(MidiSequencerPtr);
    void redo(MidiSequencerPtr);
    void undo4(MidiSequencer4Ptr);
    void redo4(MidiSequencer4Ptr);

private:

    // It doesn't really "work" to have lists of differnt types of commands,
    // but it's only for test. And in any case only one can have data at a time.
    std::list<std::shared_ptr<SqCommand>> undoList;
    std::list<std::shared_ptr<SqCommand>> redoList;
    std::list<std::shared_ptr<Sq4Command>> undo4List;
    std::list<std::shared_ptr<Sq4Command>> redo4List;

};

using UndoRedoStackPtr = std::shared_ptr<UndoRedoStack>;
#endif
