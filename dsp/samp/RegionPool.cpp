

#include "RegionPool.h"

#include "CompiledRegion.h"
#include "HeadingTracker.h"
#include "SInstrument.h"
#include "SParse.h"
#include "SamplerPlayback.h"

//#define _LOGOV

// checks to see if the region is playable
bool RegionPool::shouldRegionPlayNow(const VoicePlayParameter& params, const CompiledRegion* region, float random) {
    bool passesCheck = false;
#ifdef _SFZ_RANDOM
    if ((params.midiVelocity >= region->lovel) &&
        (params.midiVelocity <= region->hivel) &&
        (random >= region->lorand) &&
        (random <= region->hirand)) {
        passesCheck = true;
    }
#else
    if ((params.midiVelocity >= region->lovel) &&
        (params.midiVelocity <= region->hivel)) {
        passesCheck = true;
    }
#endif
    return passesCheck;
}

const CompiledRegion* RegionPool::play(const VoicePlayParameter& params, float random, bool& didKS) {
    // printf("\n... play(%d)\n", params.midiPitch);
    if (!(params.midiPitch >= 0 && params.midiPitch <= 127 && params.midiVelocity > 0 && params.midiVelocity <= 127)) {
        //SQWARN("value out of range: pitch = %d, vel = %d\n", params.midiPitch, params.midiVelocity);
        didKS = false;
        return nullptr;
    }

    // First the keyswitch logic from sfizz
    if (!lastKeyswitchLists_[params.midiPitch].empty()) {
        // printf("!lastKeyswitchLists_[%d].empty()\n", params.midiPitch);

        if (currentSwitch_ >= 0 && currentSwitch_ != params.midiPitch) {
            for (auto& region : lastKeyswitchLists_[currentSwitch_]) {
                //SQINFO("setting region->keySwitched = false (turning off regions from the old keyswitch set) r=%p", region);
                region->keySwitched = false;
            }
        }
        currentSwitch_ = params.midiPitch;
        //SQINFO("setting currentSwitch to %d", params.midiPitch);
        didKS = true;
    } else {
        didKS = false;
    }

    for (auto& region : lastKeyswitchLists_[params.midiPitch]) {
        //SQINFO("setting region keyswitched true because in lastKeySwitches");
        region->keySwitched = true;
    }

    // now the region search logic we always had
    CompiledRegion* foundRegion = nullptr;
    const CompiledRegionList& regions = noteActivationLists_[params.midiPitch];
    for (CompiledRegion* region : regions) {
        assert(params.midiPitch >= region->lokey);
        assert(params.midiPitch <= region->hikey);
        assert(region->lovel >= 0);
        assert(region->hivel <= 127);

        bool sequenceMatch = true;
        if (region->sequenceLength > 1) {
            sequenceMatch =
                ((region->sequenceCounter++ % region->sequenceLength) == region->sequencePosition - 1);
            // fprintf(stderr, "result: sw=%d ctr=%d\n", sequenceMatch, region->sequenceCounter);
        }

        const bool keyswitched = region->isKeyswitched();
        if (sequenceMatch && !foundRegion && keyswitched && shouldRegionPlayNow(params, region, random)) {
            foundRegion = region;
        }
    }
#if 0
    if (foundRegion) {
        //SQINFO("play found region");
        foundRegion->_dump(0);
    }
#endif
    return foundRegion;
}

// TODO: reduce code with the visitor
void RegionPool::_getAllRegions(std::vector<CompiledRegionPtr>& array) const {
    array = regions;
}

void RegionPool::visitRegions(RegionVisitor visitor) const {
    for (auto region : regions) {
        visitor(region.get());
    }
}

void RegionPool::sortByVelocity(std::vector<CompiledRegionPtr>& array) {
    std::sort(array.begin(), array.end(), [](const CompiledRegionPtr a, const CompiledRegionPtr b) -> bool {
        bool less = false;
        if (a->lovel < b->lovel) {
            less = true;
        }
        return less;
    });
}

void RegionPool::sortByPitch(std::vector<CompiledRegionPtr>&) {
    assert(false);
}

void RegionPool::sortByPitchAndVelocity(std::vector<CompiledRegionPtr>& array) {
    std::sort(array.begin(), array.end(), [](const CompiledRegionPtr a, const CompiledRegionPtr b) -> bool {
        bool less = false;
        if (a->lokey < b->lokey) {
            less = true;
        } else if (a->lokey == b->lokey) {
            less = (a->lovel < b->lovel);
        }
        return less;
    });
}

// new one, for new parser
bool RegionPool::buildCompiledTree(const SInstrumentPtr in) {
#if 1  // new parser. merge conflict here
    HeadingTracker ht(in->headings);

    //SQINFO("---- buildCompiledTree with %d regions", in->headings.size());
    // Enumerate all the parsed regions
    while (ht.getCurrent(SHeading::Type::Region)) {
        // here we know current describes a region
        auto curRegion = ht.getCurrent(SHeading::Type::Region);
        CompiledRegionPtr cReg = std::make_shared<CompiledRegion>(curRegion->lineNumber);

        // now add in all the parents
        for (int typeIndex = int(SHeading::Type::NUM_TYPES_INHERIT) - 1; typeIndex >= int(SHeading::Type::Region); typeIndex--) {
            SHeading::Type type = SHeading::Type(typeIndex);
            auto nextParent = ht.getCurrent(type);
            if (nextParent) {
                cReg->addRegionInfo(nextParent->compiledValues);
            }
        }
        cReg->finalize();
        //SQINFO("one more region from headings");
        //cReg->_dump(0);
        //
        if (!cReg->shouldIgnore()) {
            //SQINFO("not ignoring");
            maybeAddToKeyswitchList(cReg);
            if (cReg->sw_default >= 0) {
                currentSwitch_ = cReg->sw_default;
#else
    for (auto group : in->groups) {
        auto cGroup = std::make_shared<CompiledGroup>(group);
        if (!cGroup->shouldIgnore()) {
            for (auto reg : group->regions) {
                //SQWARN("need to add global info");
                CompiledRegionPtr cReg = std::make_shared<CompiledRegion>(reg->lineNumber);
                cReg->addRegionInfo(in->global.compiledValues);
                cReg->addRegionInfo(group->compiledValues);
                cReg->addRegionInfo(reg->compiledValues);

                // actually we should do our ignoreing on the region
                if (!cReg->shouldIgnore()) {
                    //  auto cReg = std::make_shared<CompiledRegion>(reg, cGroup, group);
                    maybeAddToKeyswitchList(cReg);
                    if (cReg->sw_default >= 0) {
                        currentSwitch_ = cReg->sw_default;
                    }
                    regions.push_back(cReg);
                }
#endif
            }
            //SQINFO("adding");
            regions.push_back(cReg);
        }

        ht.nextRegion();
    }

#if 0
    //SQINFO("regions before fixup ");
    for (auto region : regions) {
        region->_dump(0);

    }
#endif

    bool bRet = fixupCompiledTree();
    fillRegionLookup();
    return bRet;
}

void RegionPool::maybeAddToKeyswitchList(CompiledRegionPtr region) {
    if (region->sw_lolast >= 0 && region->sw_hilast >= region->sw_lolast) {
        for (int pitch = region->sw_lolast; pitch <= region->sw_hilast; ++pitch) {
            lastKeyswitchLists_[pitch].push_back(region.get());
        }
    }
}

void RegionPool::fillRegionLookup() {
    sortByPitchAndVelocity(regions);
    removeOverlaps();
    assert(noteActivationLists_.size() == 128);

    for (auto region : regions) {
        const int low = region->lokey;
        const int high = region->hikey;
        assert(high >= low);
        assert(low >= 0);

        // map this region to every key it contains
        // but don't do it if a crazy negative key slipped in here
        if (low >= 0) {
            for (int i = low; i <= high; ++i) {
                noteActivationLists_[i].push_back(region.get());
            }
        }
    }
}

// #define _LOGOV

static bool regionsOverlap(CompiledRegionPtr firstRegion, CompiledRegionPtr secondRegion) {
    return (firstRegion->overlapsPitch(*secondRegion) &&
            firstRegion->overlapsVelocity(*secondRegion) &&
            firstRegion->overlapsRand(*secondRegion) &&
            firstRegion->sameSequence(*secondRegion));
}

bool RegionPool::attemptOverlapRepairWithVel(CompiledRegionPtr firstRegion, CompiledRegionPtr secondRegion) {
    auto velOverlap = firstRegion->overlapVelocityAmount(*secondRegion);
    if (velOverlap.first > 0) {
        if (velOverlap.second > .8f) {
            //SQINFO("velocity overlap %f too large to repair at %d and %d", velOverlap.second, firstRegion->lineNumber, secondRegion->lineNumber);
            return true;
        }
        //assert(firstRegion->lovel <= secondRegion->lovel);  // sorted
        if (firstRegion->lovel > secondRegion->lovel) {
            //SQWARN("in overlap vel, first=%d second=%d  ilnes %d,%d", firstRegion->lovel, secondRegion->lovel, firstRegion->lineNumber, secondRegion->lineNumber);
            return true;
        }
        while (velOverlap.first) {
            const int firstRange = firstRegion->velRange();
            const int secondRange = secondRegion->velRange();
            if (firstRange >= secondRange) {
                // There should be enough range to take on
                if (firstRange < 2) {
                    assert(false);
                    return true;
                }
                // since we know first region starts before or at second,
                // and that overlap isn't 100%, then
                // we know shrinking 1 from the bottom will help
                firstRegion->hivel -= 1;
                --velOverlap.first;
            } else {
                if (secondRange < 2) {
                    assert(false);
                    return true;
                }
                secondRegion->lovel += 1;
                --velOverlap.first;
            }
        };
    }
    return false;
}

bool RegionPool::attemptOverlapRepairWithPitch(CompiledRegionPtr firstRegion, CompiledRegionPtr secondRegion) {
    auto pitchOverlap = firstRegion->overlapPitchAmount(*secondRegion);
    if (pitchOverlap.first > 0) {
        if (pitchOverlap.second > .8f) {
            //SQINFO("pitch overlap %f too large to repair at %d and %d", pitchOverlap.second, firstRegion->lineNumber, secondRegion->lineNumber);
            return true;
        }
        // If we have patched so much that we are our of order, give up
        if (firstRegion->lokey > secondRegion->lokey) {
            //SQWARN("in overlap pitch, first=%d second=%d  ilnes %d,%d", firstRegion->lokey, secondRegion->lokey, firstRegion->lineNumber, secondRegion->lineNumber);
            return true;
            //assert(firstRegion->lokey <= secondRegion->lokey);  // sorted
        }
        while (pitchOverlap.first) {
            const int firstRange = firstRegion->pitchRange();
            const int secondRange = secondRegion->pitchRange();
            if (firstRange >= secondRange) {
                // There should be enough range to take on
                if (firstRange < 2) {
                    assert(false);
                    return true;
                }
                // since we know first region starts before or at second,
                // and that overlap isn't 100%, then
                // we know shrinking 1 from the bottom will help
                firstRegion->hikey -= 1;
                --pitchOverlap.first;
            } else {
                if (secondRange < 2) {
                    assert(false);
                    return true;
                }
                secondRegion->lokey += 1;
                --pitchOverlap.first;
            }
        };
    }
    return false;
}

bool RegionPool::evaluateOverlapsAndAttemptRepair(CompiledRegionPtr firstRegion, CompiledRegionPtr secondRegion) {
    const int hk1 = firstRegion->hikey;
    const int hk2 = secondRegion->hikey;
    const int lk1 = firstRegion->lokey;
    const int lk2 = secondRegion->lokey;

    const int hv1 = firstRegion->hivel;
    const int hv2 = secondRegion->hivel;
    const int lv1 = firstRegion->lovel;
    const int lv2 = secondRegion->lovel;

#ifdef _LOGOV
    //SQINFO("overlap comparing line %d with %d", firstRegion->lineNumber, secondRegion->lineNumber);
    //SQINFO("  first pitch=%d,%d, vel=%d,%d", firstRegion->lokey, firstRegion->hikey, firstRegion->lovel, firstRegion->hivel);
    //SQINFO("  second pitch=%d,%d, vel=%d,%d", firstRegion->lokey, secondRegion->hikey, secondRegion->lovel, secondRegion->hivel);

    //SQINFO("  first sw_ range=%d, %d. second=%d, %d", firstRegion->sw_lolast, firstRegion->sw_hilast, secondRegion->sw_lolast, secondRegion->sw_hilast);
    //SQINFO("  overlap pitch = %d, overlap vel = %d", firstRegion->overlapsPitch(*secondRegion), firstRegion->overlapsVelocity(*secondRegion));
#endif

    // If there is no overlap, then everything is fine.
    // Can keep and use regions as they are
    if (!regionsOverlap(firstRegion, secondRegion)) {
#ifdef _LOGOV
        //SQINFO("no overlap, do nothing");
#endif
        return false;
    }

    // ok, there is overlap. maybe we can tweak regions
    // to make them not overlap any longer
    auto pitchOverlap = firstRegion->overlapPitchAmount(*secondRegion);
    auto velOverlap = firstRegion->overlapVelocityAmount(*secondRegion);
    const bool velLessOverlap = velOverlap.second < pitchOverlap.second;

    // first try to repair the property with the least overlap (pitch or velocity)
    velLessOverlap ? attemptOverlapRepairWithVel(firstRegion, secondRegion) : attemptOverlapRepairWithPitch(firstRegion, secondRegion);

    // if regions are good now, then stop
    if (!regionsOverlap(firstRegion, secondRegion)) {
#ifdef _LOGOV
        //SQINFO("overlap, repaired using %s", velLessOverlap ? "vel" : "pitch");
#endif
        return false;
    }

    // If we still aren't good, might as well try patching the other one.
    // After all one of these regsions will get discared if we can't fix it.
    velLessOverlap ? attemptOverlapRepairWithPitch(firstRegion, secondRegion) : attemptOverlapRepairWithVel(firstRegion, secondRegion);
    attemptOverlapRepairWithPitch(firstRegion, secondRegion);

    bool stillBad = regionsOverlap(firstRegion, secondRegion);
    if (stillBad) {
        //SQINFO("unable to repair overlaps at  lines %d and %d", firstRegion->lineNumber, secondRegion->lineNumber);
    #if 0
        //SQINFO(" First region:");
        firstRegion->_dump(1);
        //SQINFO(" second region:");
        secondRegion->_dump(1);
    #endif

        // If we can't repair, then restore everything,
        // This ensures that post-delete the remaining region will be intact.
        firstRegion->hikey = hk1;
        secondRegion->hikey = hk2;
        firstRegion->lokey = lk1;
        secondRegion->lokey = lk2;

        firstRegion->hivel = hv1;
        secondRegion->hivel = hv2;
        firstRegion->lovel = lv1;
        secondRegion->lovel = lv2;
    }
    return stillBad;
}

void RegionPool::removeOverlaps() {
#ifdef _LOGOV
    printf("enter remove overlaps there are %d regions\n",
           (int)regions.size());
    _dump(0);
    printf("\n\n");
#endif
    int removed = 0;
    if (regions.size() < 2) {
        return;
    }
    sortByPitchAndVelocity(regions);

    using iterator = std::vector<CompiledRegionPtr>::iterator;
    for (iterator it = regions.begin(); it != regions.end();) {
        iterator itNext = it + 1;
        if (itNext == regions.end()) {
            //return removed;
            //printf("leaving remove at 143 with %d regions", (int)regions.size());
            return;
        }
        CompiledRegionPtr first = *it;
        CompiledRegionPtr second = *itNext;

        if (evaluateOverlapsAndAttemptRepair(first, second)) {
            // keep the region with the smallest pitch range
            const int firstPitchRange = first->hikey - first->lokey;
            const int secondPitchRange = second->hikey - second->lokey;
            if (firstPitchRange <= secondPitchRange) {
                //SQINFO("about to erase region from %d based on conflict from %d\n", second->lineNumber, first->lineNumber);
#if 0
                //SQINFO("here is one going away: ");
                second->_dump(1);
                //SQINFO("here is one staying: ");
                first->_dump(1);
#endif

                // if we want to erase the second one, do that.
                // it still points at first, but next iteration there will be a different next;
                regions.erase(itNext);
                ++removed;
            } else {
#ifdef _LOGOV
                printf("about to(b) erase region from %d\n", first->lineNumber);
#endif
                // we erase the first one, leaving
                // it pointing at next.
                // so we are set up to continue loop fine
                it = regions.erase(it);
            }
        } else {
            ++it;
#ifdef _LOGOV
            printf("not removing\n");
#endif
        }
    }
    //return removed;
#ifdef _LOGOV
    printf("leaving remove overlaps there are %d regions\n", (int)regions.size());
#endif
    return;
}

bool RegionPool::fixupCompiledTree() {
    // TODO: do we need this function any more?
    //SQWARN("fixup compiled tree does nothing");
    return true;
}

void RegionPool::_dump(int depth) const {
    //SQINFO("dumping region pool");
    for (int i = 0; i < depth; ++i) {
        printf(" ");
    }
    for (auto region : regions) {
        region->_dump(depth + 4);
    }
    fflush(stdout);
    //SQINFO("dunp dumping region pool");
}
