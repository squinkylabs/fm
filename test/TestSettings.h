#pragma once


#include <assert.h>
#include "ISeqSettings.h"
#include "TimeUtils.h"

class TestSettings : public ISeqSettings
{
public:
    ::rack::ui::Menu* invokeUI (::rack::widget::Widget* parent) override
    {
        return nullptr;
    }
    float getQuarterNotesInGrid() override
    {
        return _quartersInGrid;
    }
    bool snapToGrid() override
    {
        return _snapToGrid;
    }
    bool snapDurationToGrid() override
    {
        return false;
    }

    /**
     * if snap to grid,will quantize the passed value to the current grid.
     * otherwise does nothing.
     * will not quantize smaller than a grid.
     */
    float quantize(float time, bool allowZero) override
    {
        auto quantized = time;
        if (snapToGrid()) {
            quantized = (float) TimeUtils::quantize(time, getQuarterNotesInGrid(), allowZero);
        }
        return quantized;
    }

    float quantizeAlways(float time, bool allowZero) override
    {
        return  (float) TimeUtils::quantize(time, getQuarterNotesInGrid(), allowZero);
    }

    float articulation() override
    {
        return _articulation;
    }

    std::string getMidiFilePath() override
    {
        return midiFilePath;
    }
    void setMidiFilePath(const std::string& s) override
    {
        midiFilePath = s;
    }

    virtual std::pair<int, Scale::Scales> getKeysig() override
    {
        return std::make_pair<int, Scale::Scales>(0, Scale::Scales::Major);
    }
    void setKeysig(int root, Scale::Scales mode) override
    {
    }

    std::string midiFilePath;
    float _articulation = 1;
    float _quartersInGrid = .25;
    bool _snapToGrid = true;
};