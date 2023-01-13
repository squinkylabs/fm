
#pragma once

#include <assert.h>

#include <string>

class StochasticDisplayNote;

class StochasticNote {
public:
    StochasticNote(int dur) : duration(dur) {}

    static const int ppq;
    int duration;

    static StochasticNote half();
    static StochasticNote quarter();
    static StochasticNote eighth();
    static StochasticNote sixteenth();
    static StochasticNote thirtysecond();

    static StochasticNote dotted_half();
    static StochasticNote dotted_quarter();
    static StochasticNote dotted_eighth();
    static StochasticNote dotted_sixteenth();

    static StochasticNote trip_half();
    static StochasticNote trip_quarter();
    static StochasticNote trip_eighth();
    static StochasticNote trip_sixteenth();
   // static StochasticNote thirtysecond();

    static StochasticNote fromString(const std::string&);

    bool operator<(const StochasticNote& other) const;

    std::string toText() const;
    bool isValid() const { return duration > 0; }

private:
};

/**
 * I did this first. Now I won't want to use it as my fundamental note type.
 * But it might be useful later. Or not...
 */
class StochasticDisplayNote {
public:
    enum class Durations {
        whole,
        doubleWhole,
        half,
        quarter,
        eighth,
        sixteenth,
        thirtysecond
    };
    enum class Tuples {
        none,
        triplet,
        quintuplet,
        setputlet
    };

    StochasticDisplayNote(Durations, Tuples);
    StochasticDisplayNote(Durations);
    StochasticDisplayNote() = delete;

    const Durations duration;
    const Tuples tuple;

    /** half note = 1/2
     * whole note TBD...
     */
    double timeDuration() const;

private:
};

inline StochasticDisplayNote::StochasticDisplayNote(Durations d, Tuples t) : duration(d), tuple(t) {
}

inline StochasticDisplayNote::StochasticDisplayNote(Durations d) : duration(d), tuple(Tuples::none) {
}

inline double StochasticDisplayNote::timeDuration() const {
    double ret = 0;
    switch (duration) {
        case Durations::half:
            ret = .5;
            break;
        case Durations::quarter:
            ret = .25;
            break;
        case Durations::eighth:
            ret = .125;
            break;
        case Durations::sixteenth:
            ret = .25 * .25;
            break;
        case Durations::thirtysecond:
            ret = .5 * .25 * .25;
            break;
        default:
            assert(false);
    }
    return ret;
}
