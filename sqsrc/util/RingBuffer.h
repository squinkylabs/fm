#pragma once
#include <assert.h>

#include "SqLog.h"

/**
 * A simple ring buffer.
 * Template arguments are for numeric type stored, and for size.
 * Not thread safe.
 * Guaranteed to be non-blocking. Adding or removing items will never
 * allocate or free memory.
 * Objects in RingBuffer are not owned by RingBuffer - they will not be destroyed.
 */
template <typename T, int SIZE>
class SqRingBuffer
{
public:
    SqRingBuffer()
    {
        for (int i = 0; i < SIZE; ++i) {
            memory[i] = 0;
        }
    }

    // this constructor does not try to initialize
    SqRingBuffer(bool b)
    {
        assert(!b);
    }

    void push(T);
    T pop();
    bool full() const;
    bool empty() const;

    void _dump();

private:
    T memory[SIZE];
    bool couldBeFull = false;           // full and empty are ambiguous, both are in--out
    int inIndex = 0;
    int outIndex = 0;

    /** Move up 'p' (a buffer index), wrap around if we hit the end
     * (this is the core of the circular ring buffer).
     */
    void advance(int &p);
};

template <>
inline void SqRingBuffer<uint16_t, 3>::_dump() {
    //SQINFO("*** ring buffer dump.");
    //SQINFO("in=%d out=%d", inIndex, outIndex);
    for (int i = 0; i < 3; ++i) {
        //SQINFO("buffer[%d] = %x", i, memory[i]);
    }
}


template <typename T, int SIZE>
inline void SqRingBuffer<T, SIZE>::push(T value)

{
    assert(!full());
    memory[inIndex] = value;
    advance(inIndex);
    couldBeFull = true;
}

template <typename T, int SIZE>
inline T SqRingBuffer<T, SIZE>::pop()
{
    assert(!empty());
    T value = memory[outIndex];
    advance(outIndex);
    couldBeFull = false;
    return value;
}

template <typename T, int SIZE>
inline bool SqRingBuffer<T, SIZE>::full() const
{
    return (inIndex == outIndex) && couldBeFull;
}

template <typename T, int SIZE>
inline bool SqRingBuffer<T, SIZE>::empty() const
{
    return (inIndex == outIndex) && !couldBeFull;
}

template <typename T, int SIZE>
inline void SqRingBuffer<T, SIZE>::advance(int &p)
{
    if (++p >= SIZE) p = 0;
}



