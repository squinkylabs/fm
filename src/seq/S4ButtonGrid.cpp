
#include "../Squinky.hpp"
#include "MidiSequencer4.h"
#include "S4ButtonGrid.h"
#include "S4Button.h"
#include "Seq4.h"
#include "SqStream.h"
#include "UndoRedoStack.h"
#include "../Sequencer4Widget.h"
#include "../ctrl/SqWidgets.h"

#ifdef _SEQ4

S4Button* S4ButtonGrid::getButton(int row, int col) {
    assert(row >= 0 && row < 4 && col >= 0 && col < 4);
    return buttons[row][col];
}

void S4ButtonGrid::setNewSeq(MidiSequencer4Ptr newSeq) {
    seq = newSeq;
    for (int row = 0; row < MidiSong4::numTracks; ++row) {
        for (int col = 0; col < MidiSong4::numTracks; ++col) {
            buttons[row][col]->setNewSeq(newSeq);
        }
    }
}

/***************************** S4ButtonGrid ***********************************/

using Comp = Seq4<WidgetComposite>;
void S4ButtonGrid::init(Sequencer4Widget* parent, rack::engine::Module* module,
                        MidiSequencer4Ptr _seq, std::shared_ptr<Seq4<WidgetComposite>> _seq4Comp) {
    
    widget = parent;
    seq = _seq;

    MidiSong4Ptr song;
    if (seq) {
        song = seq->song;
    } else {
        song =  MidiSong4::makeTest(MidiTrack::TestContent::eightQNotesCMaj, 0, 0);
    }

    if (!seq) {
       seq = MidiSequencer4::make(song);
    }

    if (!seq->song) {
        seq->song = MidiSong4::makeTest(MidiTrack::TestContent::eightQNotesCMaj, 0, 0);
    }
 
   // std::shared_ptr<IComposite> icomp = Comp::getDescription();

    seq4Comp = _seq4Comp;
 
    for (int row = 0; row < MidiSong4::numTracks; ++row) {
        const float y = grid_y + row * (buttonSize + buttonMargin);
        for (int col = 0; col < MidiSong4::numSectionsPerTrack; ++col) {
            const float x = grid_x + col * (buttonSize + buttonMargin);
            const int padNumber = row * MidiSong4::numSectionsPerTrack + col;

            S4Button* button = new S4Button(
                rack::math::Vec(buttonSize, buttonSize),
                rack::math::Vec(x, y),
                row,
                col,
            seq,
            seq4Comp,
            module);
#if 1   // param widget way
            if (module) {
                button->paramQuantity = module->paramQuantities[Comp::PADSELECT0_PARAM + padNumber];       
            }
            parent->addParam(button);
#else
           
            parent->addChild(b);
#endif
            
            button->setClickHandler(makeButtonHandler(row, col));
            buttons[row][col] = button;
        }

        const float jacksX1 = 12;
        const float jacksX2 = 368;
        const float cv_out_dy = 0;
        const float gate_out_dy = 28;
        const float cv_in_dy = 0;


        // Tooltips are now supported by VCV so will handle it there
        {
            //SqStream s;
            //s.add("Track ");
            //s.add(row+1);
            //s.add(" CV out");
           //s << "Track " << row + 1 << " CV out";
            SqOutputJack* oj = rack::createOutput<SqOutputJack>(
                rack::math::Vec(jacksX2, y + cv_out_dy),
                module,
                Comp::CV0_OUTPUT + row);
            //oj->setTooltip(s.str());
            parent->addOutput(oj);
        }

       
        {
            //SqStream s;
            //s.add("Track ");
            //s.add(row+1);
            //s.add(" Gate out");
           // s << "Track " << row + 1 << " Gate out";
            SqOutputJack* oj = createOutput<SqOutputJack>(
                rack::math::Vec(jacksX2, y + gate_out_dy),
                module,
                Comp::GATE0_OUTPUT + row);
            //oj->setTooltip(s.str());
            parent->addOutput(oj);
        }

        {
            //SqStream s;
            //s.add("Track ");
            //s.add(row+1);
            //s.add(" section selector CV in");
            //s << "Track " << row + 1 << " section selector CV in";
            SqInputJack* ij = rack::createInput<SqInputJack>(
                rack::math::Vec(jacksX1, y + cv_in_dy ),
                module,
                Comp::MOD0_INPUT + row);
            //ij->setTooltip(s.str());
            parent->addInput(ij);
        }
    }
}

class S4ButtonClickCommand : public Sq4Command
{
public:
    S4ButtonClickCommand(int row, int col) : rowToSelect(row), colToSelect(col) 
    {

    }

    void execute(MidiSequencer4Ptr seq, Sequencer4Widget* widget) override
    {
        // we always need to get a fresh pointer - can't store in undo object
        assert(widget);
        std::shared_ptr<S4ButtonGrid> grid = widget->getButtonGrid();
        assert(grid);

       // select the one we just clicked into
        for (int r = 0; r < MidiSong4::numTracks; ++r) {
            for (int c = 0; c < MidiSong4::numSectionsPerTrack; ++c) {
                auto button = grid->getButton(r, c);
                assert(button);
                if (button->isSelected()) {
                    origRowSelected = r;
                    origColSelected = c;
                }
                button->setSelection(r == rowToSelect && c == colToSelect);
            }
        }
 
        auto button = grid->getButton(rowToSelect, colToSelect);
        button->doEditClip();
    }
    
    void undo(MidiSequencer4Ptr seq, Sequencer4Widget* widget) override
    {
        // we always need to get a fresh pointer - can't store in undo object
        assert(widget);
        std::shared_ptr<S4ButtonGrid> grid = widget->getButtonGrid();
        assert(grid);

        // first restore the edited track
        if (origColSelected >= 0 && origRowSelected >= 0) {
            auto button = grid->getButton(origRowSelected, origColSelected);
            assert(button);
            button->doEditClip();
        }

        // unset the selection we set before
        auto button = grid->getButton(rowToSelect, colToSelect);
        button->setSelection(false);

        // and select the original
        if (origRowSelected >= 0 && origColSelected >= 0) {
            button = grid->getButton(origRowSelected, origColSelected);
            button->setSelection(true);
        }
    }

private:
    const int rowToSelect;
    const int colToSelect;
    int origRowSelected = -1;
    int origColSelected = -1;
};

void S4ButtonGrid::onClick(bool isCtrl, int row, int col) {
    Command4Ptr cmd = std::make_shared<S4ButtonClickCommand>(row, col);
    cmd->name = "click";
    assert(widget);
    seq->undo->execute4(seq, widget, cmd);

    // we still do our own selection of next section, outside of unto
    if (isCtrl) {
        // then the select next clip
        // remember, section is 1..4
        seq4Comp->setNextSectionRequest(row, col + 1);
    } else {
        for (int r = 0; r < MidiSong4::numTracks; ++r) {
            seq4Comp->setNextSectionRequest(r, col + 1);
        }
    }
}

std::function<void(bool isCtrlKey)> S4ButtonGrid::makeButtonHandler(int row, int col) {
    return [this, row, col](bool isCtrl) {
        this->onClick(isCtrl, row, col);
    };
}

#endif
