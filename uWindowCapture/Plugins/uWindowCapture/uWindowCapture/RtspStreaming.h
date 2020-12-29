#pragma once

#include <memory>
#include <d3d11.h>
#include <thread>
#include <chrono>

#include "Singleton.h"
#include "Window.h"


extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avio.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libavresample/avresample.h"
#include "libswscale/swscale.h"
};

const AVPixelFormat src_pix_fmt = AV_PIX_FMT_RGBA;	//Id3d11texture2D buffer format
const AVPixelFormat target_pix_fmt = AV_PIX_FMT_YUV420P;
const AVCodecID target_codec_id = AV_CODEC_ID_H264;

typedef void(__stdcall* callback_t)(LPCTSTR str);

class RtspStreaming
{
	UWC_SINGLETON(RtspStreaming)

public:
	void Initialize();
	void Finalize();
	bool StartStreaming(int id, const char* url, int target_fps, bool encypt);
	bool StopStreaming();
	
	static bool IsConfigured() { return configured; }
	static bool IsStreaming() { return is_streaming; }
	static int WindowId() { return win_id; }
	static std::shared_ptr<Window> ActiveWindow() { return active_winptr; }

	//interrupt callback for Unity3d
	static callback_t rtsp_interrupt_cb;

private:
	int width, height;
	int stream_fps;
	char* rtsp_url; 
	static bool configured;
	static bool is_streaming;

	static int win_id;
	static std::shared_ptr<Window> active_winptr;
	std::thread stream_th;

	void ClearConfig();
	bool SetupStreaming();
	bool InitializeVidStream(AVStream* stream, AVCodecContext* codec_ctx, AVCodec* codec);
	//void RtspStream();
	void SetCodecParams(AVFormatContext* fctx, AVCodecContext* codec_ctx);

protected:
	//char* auth_user, * auth_pass;
	bool encryption_enabled;
	AVFormatContext* ofmt_ctx;

	//Video stream params
	AVStream* vid_stream;
	AVCodec* vid_codec;
	AVCodecContext* vid_codec_ctx;
};

