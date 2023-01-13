#pragma once

#include <stdio.h>

#include "stream_decoder.h"

class FilePath;

class FlacReader {
public:
    ~FlacReader();
    void read(const FilePath& filePath);
    bool ok() const { return isOk; }

    const float* getSamples() const;
    float* takeSampleBuffer();
    uint64_t getNumSamples() const;
    unsigned int getSampleRate();
    virtual uint64_t getTotalFrameCount();

private:
    static float read16Bit(const int32_t*);
    static float read24Bit(const int32_t*);

    FLAC__StreamDecoder* decoder = nullptr;
    bool isOk = false;

    // Who owns this
    float* monoData = nullptr;
    float* writePtr = nullptr;

    uint64_t framesExpected = 0;
    uint64_t framesRead = 0;
    //unsigned bitsPerSample = 0;
    unsigned channels_ = 0;
    unsigned bitsPerSample_ = 0;
    unsigned sampleRate_ = 0;
    // bps = metadata->data.stream_info.bits_per_sample;
    void onFormat(uint64_t totalSamples, unsigned sampleRate, unsigned channels, unsigned bitsPerSampl);

    /**
	 * returns true if ok.
	 * @param rightData will be null for mono data.
	 */
    bool onData(unsigned samples, const int32_t* leftData, const int32_t* rightData);

    static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
    static void metadata_callback(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data);
    static void error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);
};
