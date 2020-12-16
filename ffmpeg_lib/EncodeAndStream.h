#ifndef ENCODEANDSTREAM_H_
#define ENCODEANDSTREAM_H_



extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
};

enum TexureType
{
	D3D11TEXTURE2D,
	YUV420
};

class EncodeAndStream
{
public:
	EncodeAndStream(const char* rtspUrl = "rtsp://127.0.0.1:8554/test");
	~EncodeAndStream();
	void Stream(const char* filename, const char* codec_name);
private:
	const char* rtspServerAdress;
};

#endif // !ENCODEANDSTREAM_H_



