#pragma once


#ifdef FFMPEGLIB_EXPORTS
#define FFMPEGLIB_API __declspec(dllexport)
#else
#define FFMPEGLIB_API __declspec(dllimport)
#endif


//#include "rtsp_stream.hpp"
#include "Streaming.h"
#include "Utilities.hpp"


void save_gray_frame(unsigned char* buf, int wrap, int xsize, int ysize, int fr);

extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/file.h"


	FFMPEGLIB_API void GetWidthAndHeight(const char* filename, int* width, int* height);
	FFMPEGLIB_API void SaveFrameFromFile(const char* filename);
	FFMPEGLIB_API int StartLiveStream(const char* vid, const char* url);
};