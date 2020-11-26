#include "Streaming.h"


Streaming::Streaming(const char* _videoFileName, const char* _rtmpServerAdress) : videoFileName(_videoFileName), rtspServerAdress(_rtmpServerAdress)
{
    initialized = false;

    //av_register_all();
    //avformat_network_init();

    ret = setupInput(videoFileName);
    if (ret < 0)
    {
        printf("failed to initialize input stream");
        return;
    }

    ret = setupOutput(rtspServerAdress);
    if (ret < 0)
    {
        printf("failed to initialize output stream");
        return;
    }

    initialized = true;
}

int Streaming::setupInput(const char* _videoFileName)
{
    ifmt_ctx = avformat_alloc_context();

    if ((ret = avformat_open_input(&ifmt_ctx, _videoFileName, 0, 0)) < 0)
    {
        printf("Could not open input file: %s ",_videoFileName);
        return -1;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        printf("Failed to retrieve input stream information");
        return -1;
    }

    for (int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
            break;
        }
    }

    printf("Input stream info:\n");
    av_dump_format(ifmt_ctx, 0, _videoFileName, 0);
    return 0;
}

int Streaming::setupOutput(const char* _rtspServerAdress)
{
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", _rtspServerAdress); //RTSP
    if (!ofmt_ctx)
    {
        printf("Could not deduce output format from file extension: using mpeg.\n");
        //flv-rtmp, mpegts-udp
        avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpeg", _rtspServerAdress);
    }

    ofmt = ofmt_ctx->oformat;
    if (!ofmt)
        printf("Error creating outformat\n");
    
    for (int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        //Create output AVStream according to input AVStream
        AVCodecContext* out_ctx;
        AVStream* in_stream = ifmt_ctx->streams[i];
        AVCodec* in_codec = avcodec_find_encoder(in_stream->codecpar->codec_id);
        if (in_codec==NULL) {
            printf("Could not find input codec/context for '%s'.\n", avcodec_get_name(in_stream->codecpar->codec_id));
            return -1;
        }

        AVStream* out_stream = avformat_new_stream(ofmt_ctx, in_codec);
        if (!out_stream)
        {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            return -1;
        }
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        out_ctx = avcodec_alloc_context3(avcodec_find_encoder(out_stream->codecpar->codec_id));
        if (avcodec_parameters_to_context(out_ctx, out_stream->codecpar) < 0)
        {
            printf("failed to create output context from params");
        }
        //Copy the settings of AVCodecContext
        //ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        //ret = avcodec_parameters_to_context(out_ctx, in_stream->codecpar);
        //if (ret < 0)
        //{
        //    printf("Failed to copy context from input to output stream codec context\n");
        //    return -1;
        //}
        
        out_stream->codecpar->codec_tag = 0;

        if (ofmt_ctx->oformat->flags & 1 << 22)
            out_ctx->flags |= 1 << 22;
            //out_stream->codec->flags |= 1 << 22;
    }

    return 0;
}


int Streaming::Stream()
{
    printf("Output stream info:\n");
    av_dump_format(ofmt_ctx, 0, rtspServerAdress, 1);
    //Open output URL
    if (!(ofmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, rtspServerAdress, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open output URL '%s'", rtspServerAdress);
            return -1;
        }
    }
    //Write file header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        printf("Error occurred when opening output URL\n");
        return -1;
    }

    startTime = av_gettime();
    while (1)
    {
        AVStream* in_stream, * out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        if (pkt.pts == AV_NOPTS_VALUE)
        {
            //Write PTS
            AVRational time_base1 = ifmt_ctx->streams[videoIndex]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[videoIndex]->r_frame_rate);
            //Parameters
            pkt.pts = (double)(frameIndex * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
        }
        if (pkt.stream_index == videoIndex)
        {
            AVRational time_base = ifmt_ctx->streams[videoIndex]->time_base;
            AVRational time_base_q = { 1, AV_TIME_BASE };
            int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - startTime;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);
        }

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        if (pkt.stream_index == videoIndex)
        {
            frameIndex++;
        }
        //ret = av_write_frame(ofmt_ctx, &pkt);
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

        if (ret < 0)
        {
            printf("Error muxing packet\n");
            break;
        }

        //av_free_packet(&pkt);
        av_packet_unref(&pkt);
    }

    //Write file trailer
    av_write_trailer(ofmt_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}

Streaming::~Streaming()
{
    avformat_close_input(&ifmt_ctx);
    avformat_close_input(&ofmt_ctx);
}