
#ifndef STREAMING_H_
#define STREAMING_H_

// Import STD
#include <iostream>
#include <chrono>

// Import LibAV
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"


class Streaming
{
public:
    Streaming() {}
    Streaming(const char* videoFileName, const char* rtspServerAdress);
    ~Streaming();
    int Stream();

    AVOutputFormat* ofmt = NULL;
    AVFormatContext* ifmt_ctx = NULL;
    AVFormatContext* ofmt_ctx = NULL;
    AVPacket pkt;
    //AVStream* audio_st, * video_st;
    //AVCodec* audio_codec, * video_codec;

    bool IsInitialized() { return initialized; }
private:
    int setupInput(const char* videoFileName);
    int setupOutput(const char* rtmpServerAdress);

    int ret;

    // Input file and RTMP server address
    const char* videoFileName;
    const char* rtspServerAdress;

protected:
    int videoIndex = -1;
    int frameIndex = 0;
    int64_t startTime = 0;
    bool initialized = false;
};
};
#endif