
#include "../Squinky.hpp"
#include "SqGfx.h"
#include "WidgetComposite.h"

#include "ISeqSettings.h"
#include "MidiEvent.h"
#include "MidiSequencer.h"
#include "NoteDragger.h"
#include "NoteDisplay.h"
#include "TimeUtils.h"
#include "UIPrefs.h"

NoteDragger::NoteDragger(MidiSequencerPtr seq, float initX, float initY) :
sequencer(seq),
    startX(initX),
    startY(initY)
{
    curMousePositionX = initX;
    curMousePositionY = initY;
}

NoteDragger::~NoteDragger()
{
}

void NoteDragger::onDrag(float deltaX, float deltaY)
{
    curMousePositionX += deltaX;
    curMousePositionY += deltaY;
}

// all of the "shift" and drag params are in pixels
void NoteDragger::drawNotes(NVGcontext *vg, float verticalShift, float horizontalShift, float horizontalStretch)
{
    auto scaler = sequencer->context->getScaler();
    assert(scaler);
    const int noteHeight = scaler->noteHeight();

    // For drag operations, let's use the selection to pick notes to draw.
    for (auto it : *sequencer->selection) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it);
        if (note) {
            float finalHShift = horizontalShift; 
            if (draggingStartTime() && sequencer->context->settings()->snapToGrid()) {
                finalHShift = quantizeForDisplay(note->startTime, horizontalShift, true);
            }

            float finalHStretch = horizontalStretch;
            if (draggingDuration() && sequencer->context->settings()->snapDurationToGrid()) {
                finalHStretch = quantizeForDisplay(note->duration, horizontalStretch, false);
            }

            const float x = scaler->midiTimeToX(*note) + finalHShift;
            const float y = scaler->midiPitchToY(*note) + verticalShift;
            const float width = scaler->midiTimeTodX(note->duration) + finalHStretch;

            SqGfx::filledRect(
                vg,
                UIPrefs::SELECTED_NOTE_COLOR,
                x, y, width, noteHeight);
        }
    }
}

float NoteDragger::quantizeForDisplay(float metricTime, float timeShiftPixels, bool canGoBelowGridSize)
{
    return timeShiftPixels;       // default imp does nothing
}

bool NoteDragger::draggingStartTime()
{
    return false;
}

bool NoteDragger::draggingDuration()
{
    return false;
}

/******************************************************************
*
* NotePitchDragger
*/

// Remember current viewport pitch range. Shave some off top and
// bottom to allow reasonable dragging.
NotePitchDragger::NotePitchDragger(MidiSequencerPtr seq, float x, float y) :
NoteDragger(seq, x, y),
    viewportUpperPitch0(sequencer->context->pitchHigh()),
    highPitchForDragStart(sequencer->context->pitchHigh() - 2 * PitchUtils::semitone),
    viewportLowerPitch0(sequencer->context->pitchLow()),
    lowPitchForDragStart(sequencer->context->pitchLow() + 2 * PitchUtils::semitone),
    pitch0(sequencer->context->getScaler()->yToMidiCVPitch(y))
{
}

float NotePitchDragger::calcTranspose() const
{
    auto scaler = sequencer->context->getScaler();
    const float verticalShift = curMousePositionY - startY;
    const float transposeCV = scaler->yToMidiDeltaCVPitch(verticalShift);
    return transposeCV;
}

// TODO: take into account note height!
float NotePitchDragger::calcShift(float transpose) const
{
    auto scaler = sequencer->context->getScaler();
    assert(scaler);

    float ret = 0;

    // distance between initial mouse click and top of viewport
    const float deltaP02Hp = highPitchForDragStart - pitch0;
    const float deltaP02Lp = lowPitchForDragStart - pitch0;

    if (transpose > deltaP02Hp) {
        ret = transpose - deltaP02Hp;
    } else if (transpose < deltaP02Lp) {
        ret = transpose - deltaP02Lp;
    }

    return ret;
}

void NotePitchDragger::onDrag(float deltaX, float deltaY)
{
    NoteDragger::onDrag(deltaX, deltaY);
    const float transpose = calcTranspose();
    const float shift = calcShift(transpose);

    // TODO: only if shift moves away from center,
    // or only if pitch not in viewport.
    auto scaler = sequencer->context->getScaler();
    if (shift) {
        sequencer->context->setPitchRange(viewportLowerPitch0 + shift, viewportUpperPitch0 + shift);
    }
    sequencer->context->setCursorPitch(transpose + pitch0);
}

void NotePitchDragger::commit()
{
    //printf("enter commit-1, cursor pitch = %f\n", sequencer->context->cursorPitch());
    // TODO: use calcTranspose
    auto scaler = sequencer->context->getScaler();
    const float verticalShift = curMousePositionY - startY;
    const float transposeCV = scaler->yToMidiDeltaCVPitch(verticalShift);
    const int semiShift = PitchUtils::deltaCVToSemitone(transposeCV);

    // Only do the edit if significant change.
    if (semiShift != 0) {
        // Restore cursor to original pitch.
        sequencer->context->setCursorPitch(pitch0);
        // Now transpose notes and cursor.
        sequencer->editor->changePitch(semiShift);
    }
}

void NotePitchDragger::draw(NVGcontext *vg)
{
    float verticalShift = curMousePositionY - startY;
    drawNotes(vg, verticalShift, 0, 0);
    SqGfx::drawText(vg, curMousePositionX + 20, curMousePositionY + 20, "transpose");
}

/******************************************************************
*
* HorizontalDragger
*/

NoteHorizontalDragger::NoteHorizontalDragger(MidiSequencerPtr seq, float x, float y, float initialNoteValue) :
NoteDragger(seq, x, y),
    viewportStartTime0(sequencer->context->startTime()),
    viewportEndTime0(sequencer->context->endTime()),
    time0(sequencer->context->getScaler()->xToMidiTime(x)),
    initialNoteValue(initialNoteValue)
{
}

float NoteHorizontalDragger::calcTimeShift() const
{
    auto scaler = sequencer->context->getScaler();
    const float horizontalShift = curMousePositionX - startX;
    const float timeShiftAmount = scaler->xToMidiDeltaTime(horizontalShift);
    return timeShiftAmount;
}

void NoteHorizontalDragger::onDrag(float deltaX, float deltaY)
{
    NoteDragger::onDrag(deltaX, deltaY);
    const float timeShift = calcTimeShift();

    const float t = timeShift + time0;

    auto x = TimeUtils::time2barsAndRemainder(2, t);
    const float newStartTime = std::get<0>(x) * TimeUtils::bar2time(2);
    const float newEndTime = newStartTime + TimeUtils::bar2time(2);
    sequencer->context->setTimeRange(newStartTime, newEndTime);
    sequencer->context->setCursorTime(t);
}

static const int shiftPixThreshold = 2;

float NoteHorizontalDragger::quantizeForDisplay(float metricTime, float timeShiftPixels, bool canGoBelowGridSize)
{
    if (std::abs(timeShiftPixels) <  shiftPixThreshold) {
        return 0;
    }

    auto scaler = sequencer->context->getScaler();

    float grid = sequencer->context->settings()->getQuarterNotesInGrid();
    float timeShiftMetric = scaler->xToMidiDeltaTime(timeShiftPixels);

    float quantizedMetricTime = TimeUtils::quantize(metricTime + timeShiftMetric, grid, true );
    //printf("hd::qfd time shift metric, unquant=%f q=%f\n", timeShiftMetric, quantizedMetricTime);
    if (!canGoBelowGridSize && quantizedMetricTime < grid) {
        quantizedMetricTime = grid;
    }
    float metricDelta = quantizedMetricTime - metricTime;
    float pixelDelta = scaler->midiTimeTodX(metricDelta);
    // printf("hd::qfd q for disp will return pixel delta %f metric delta = %f\n", pixelDelta, metricDelta);
    return pixelDelta;
}

#if 0 // orig
float NoteHorizontalDragger::quantizeForDisplay(float metricTime, float timeShiftPixels, bool canGoBelowGridSize)
{
    auto scaler = sequencer->context->getScaler();

    float grid = sequencer->context->settings()->getQuarterNotesInGrid();
    float timeShiftMetric = scaler->xToMidiDeltaTime(timeShiftPixels);

    float quantizedMetricTime = TimeUtils::quantizeForEdit(metricTime, timeShiftMetric, grid);
    printf("hd::qfd time shift metric, unquant=%f q=%f\n", timeShiftMetric, quantizedMetricTime);
    if (!canGoBelowGridSize && quantizedMetricTime < grid) {
        quantizedMetricTime = grid;
    }
    float metricDelta = quantizedMetricTime - metricTime;
    float pixelDelta = scaler->midiTimeTodX(metricDelta);
    printf("hd::qfd q for disp will return pixel delta %f metric delta = %f\n", pixelDelta, metricDelta);
    return pixelDelta;
}
#endif

/******************************************************************
*
* NoteStartDragger
*/
NoteStartDragger::NoteStartDragger(MidiSequencerPtr seq, float x, float y, float noteStartTime) :
NoteHorizontalDragger(seq, x, y, noteStartTime)
{
}

bool NoteStartDragger::draggingStartTime()
{
    return true;
}

void NoteStartDragger::draw(NVGcontext *vg)
{
    const float horizontalShift = curMousePositionX - startX;
    drawNotes(vg, 0, horizontalShift, 0);
    SqGfx::drawText(vg, curMousePositionX + 20, curMousePositionY + 20, "shift");
}



void NoteStartDragger::commit()
{
    auto scaler = sequencer->context->getScaler();
    const float horizontalShiftPix = curMousePositionX - startX; 

    if (std::abs(horizontalShiftPix) <  shiftPixThreshold) {
        return;
    }

    // find the shift required for each note
    std::vector<float> shifts;

    // TODO: get rid of this useless appendage.
    bool isShift = true;
    for (auto it : *sequencer->selection) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it);
        float timeShiftAmountPixels = horizontalShiftPix;
        if (sequencer->context->settings()->snapToGrid()) {
            timeShiftAmountPixels = quantizeForDisplay(note->startTime, horizontalShiftPix, true);
        }
       // printf("in commit after quantize timeshift = %f\n", timeShiftAmountPixels);  fflush(stdout);
        float timeshiftAmountMetric = scaler->xToMidiDeltaTime(timeShiftAmountPixels);
        shifts.push_back(timeshiftAmountMetric);
        if (std::abs(timeshiftAmountMetric) > .1) {
            isShift = true;
        }
    }

    if (isShift) {
        sequencer->editor->changeStartTime(shifts);
    }
   
}

/******************************************************************
*
* NoteDurationDragger
*/

NoteDurationDragger::NoteDurationDragger(MidiSequencerPtr seq, float x, float y, float duration) :
NoteHorizontalDragger(seq, x, y, duration)
{
}


bool NoteDurationDragger::draggingDuration()
{
    return true;
}

void NoteDurationDragger::draw(NVGcontext *vg)
{
    const float horizontalStretch = curMousePositionX - startX;
    // printf("in draw, stretch = %f\n", horizontalStretch);
    drawNotes(vg, 0, 0, horizontalStretch);
    SqGfx::drawText(vg, curMousePositionX + 20, curMousePositionY + 20, "stretch");
}


void NoteDurationDragger::commit()
{
    auto scaler = sequencer->context->getScaler();
    const float horizontalShiftPix = curMousePositionX - startX; 
    // printf("\n\n*** in commit, stretchpix = %f\n", horizontalShiftPix);

    // find the shift required for each note
    std::vector<float> shifts;
    bool isShift = false;
    for (auto it : *sequencer->selection) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it);
        float DurationShiftPixels = horizontalShiftPix;
        if (sequencer->context->settings()->snapDurationToGrid()) {
            DurationShiftPixels = quantizeForDisplay(note->duration, DurationShiftPixels, false);
        }
        float timeshiftAmountMetric = scaler->xToMidiDeltaTime(DurationShiftPixels);

        float finalDuration = note->duration + timeshiftAmountMetric;
        if (finalDuration < TimeUtils::sixtyFourthNote()) {
            timeshiftAmountMetric += (TimeUtils::sixtyFourthNote() - finalDuration);
        }

        // printf("timeShiftAmountQuantize = %f, metric = %f\n", timeShiftAmountQuantized, timeshiftAmountMetric);
        shifts.push_back(timeshiftAmountMetric);
        if (std::abs(timeshiftAmountMetric) > .1) {
            isShift = true;
        }
    }

    if (isShift) {
        sequencer->editor->changeDuration(shifts);
    }
}

