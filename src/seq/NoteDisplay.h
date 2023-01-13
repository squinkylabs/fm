
#pragma once

#include "InputScreenManager.h"
#include "MidiSequencer.h"
#include "NoteScreenScale.h"
#include "Seq.h"

class KbdManager;
class InputScreenManager;
using KbdManagerPtr = std::shared_ptr<KbdManager>;
using InputScreenManagerPtr = std::shared_ptr<InputScreenManager>;


/**
 * This class needs some refactoring and renaming.
 * It is really the entire sequencer UI, including the notes.
 *
 * Pretty soon we should sepparate out the NoteEditor.
 */
class NoteDisplay : public OpaqueWidget
{
public:
    NoteDisplay(
        const Vec& pos,
        const Vec& size,
        MidiSequencerPtr seq,
        ::rack::engine::Module* mod);


    /**
     * Inject a new sequencer into this editor.
     */
    void setSequencer(MidiSequencerPtr seq);
    MidiSequencerPtr getSequencer();
    void songUpdated();

    void onUIThread(std::shared_ptr<Seq<WidgetComposite>> seqComp, MidiSequencerPtr sequencer);

private:
    Label* focusLabel = nullptr;
    MidiSequencerPtr sequencer;
    bool cursorState = false;
    int cursorFrameCount = 0;
    bool haveFocus = true;
#ifdef _USERKB
    KbdManagerPtr kbdManager;
#endif
#ifdef _XFORM
    InputScreenManagerPtr ism;
    bool enabled = true;                // hack to make event propegation work
#endif

    void initEditContext();

    std::shared_ptr<class MouseManager> mouseManager;

    void step() override;


    void updateFocus(bool focus)
    {
        if (focus != haveFocus) {
            haveFocus = focus;
            focusLabel->text = focus ? "" : "Click in editor to get focus";
        }
    }

    void drawNotes(NVGcontext *vg);
    void drawCursor(NVGcontext *vg);
    void drawGrid(NVGcontext *vg);
    void drawBackground(NVGcontext *vg);

    bool isKeyWeNeedToStealFromRack(int key);

    void onSelect(const event::Select &e) override;
    void onDeselect(const event::Deselect &e) override;
    void drawLayer(const DrawArgs &args, int layer) override;
    void onDoubleClick(const event::DoubleClick &e) override;
    void onButton(const event::Button &e) override;
    void onHoverKey(const event::HoverKey &e) override;
    void onSelectKey(const event::SelectKey &e) override;
    void onDragStart(const event::DragStart &e) override;
    void onDragEnd(const event::DragEnd &e) override;
    void onDragMove(const event::DragMove &e)  override;
    void onDragDrop(const event::DragDrop &e) override;
    bool handleKey(int key, int mods, int action);

    void doXform(InputScreenManager::Screens screenCode);
    void addXformMenuItems(::rack::ui::Menu* menu);
    void addXformMenuItem(::rack::ui::Menu* menu, InputScreenManager::Screens);
};
