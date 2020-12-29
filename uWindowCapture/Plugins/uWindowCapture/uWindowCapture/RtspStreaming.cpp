#include "RtspStreaming.h"

#include "WindowManager.h"
#include "Debug.h"

using std::chrono::steady_clock;

UWC_SINGLETON_INSTANCE(RtspStreaming)

bool RtspStreaming::configured;
bool RtspStreaming::is_streaming;
int RtspStreaming::win_id;
std::shared_ptr<Window> RtspStreaming::active_winptr;
callback_t RtspStreaming::rtsp_interrupt_cb;
bool init_rtsp_header;
steady_clock::time_point start_time;   //time when stream start was attempted

void RtspStreaming::Initialize()
{
    is_streaming = false;
    //set default resolution to FHD
    width = 1920;    height = 1080;
    stream_fps = 24;
    configured = false;
    is_streaming = false;
    init_rtsp_header = false;
    av_register_all();
    avformat_network_init();

    Debug::Log("RtspStreaming initialized");
}

// clear streaming parameter and stop stream if its running
void RtspStreaming::Finalize()
{
    if (IsConfigured())
        ClearConfig();
}

// Create new frame and allocate buffer
AVFrame* AllocateFrameBuffer(AVCodecContext* codec_ctx, double width, double height)
{
    AVFrame* frame = av_frame_alloc();
    std::vector<uint8_t> framebuf(av_image_get_buffer_size(codec_ctx->pix_fmt, width, height, 1));
    av_image_fill_arrays(frame->data, frame->linesize, framebuf.data(), codec_ctx->pix_fmt, width, height, 1);
    frame->width = width;
    frame->height = height;
    frame->format = static_cast<int>(codec_ctx->pix_fmt);
    //Debug::Log("framebuf size: ", framebuf.size(), "  frame format: ", frame->format);
    return frame;
}

int HeaderInterrupt_CB(void* ctx)
{
    AVFormatContext* formatContext = reinterpret_cast<AVFormatContext*>(ctx);
    //check for avio_open? if used
    //check for creating rtsp header
    if (init_rtsp_header)
    {
        //timeout after 5 seconds of no activity
        std::chrono::duration<double> d = std::chrono::duration_cast<std::chrono::duration<double>>(steady_clock::now() - start_time);
        if (d.count() > 5.0)
            return 1;
    }
    return 0;
}

void RtspStream(AVFormatContext* ofmt_ctx, AVStream* vid_stream, AVCodecContext* vid_codec_ctx, char* rtsp_url)
{
    printf("Output stream info:\n");
    av_dump_format(ofmt_ctx, 0, rtsp_url, 1);

    const int width = RtspStreaming::ActiveWindow()->GetTextureWidth();
    const int height = RtspStreaming::ActiveWindow()->GetTextureHeight();
     
    //DirectX BGRA to mpeg4/h264 YUV420p
    SwsContext* conversion_ctx = sws_getContext(width, height, src_pix_fmt,
        vid_stream->codecpar->width, vid_stream->codecpar->height, target_pix_fmt, 
        SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!conversion_ctx)
    {
        Debug::Error("Could not initialize sample scaler!");
        return;
    }

    AVFrame* frame = AllocateFrameBuffer(vid_codec_ctx,vid_codec_ctx->width,vid_codec_ctx->height);
    if (!frame) {
        Debug::Error("Could not allocate video frame\n");
        return;
    }

    if (av_frame_get_buffer(frame, 0) < 0) {
        Debug::Error("Could not allocate the video frame data\n");
        return;
    }

    start_time = steady_clock::now();
    init_rtsp_header = true;

    // get's stuck in writing header is the server is not running
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        Debug::Error("Error occurred when writing header");
        init_rtsp_header = false;
        av_frame_free(&frame);
        RtspStreaming::rtsp_interrupt_cb(TEXT("Failed during writing RTSP header"));
        return;
    }
    Debug::Log("done writing header");
    init_rtsp_header = false;
    //ofmt_ctx->interrupt_callback.callback = nullptr;
    //ofmt_ctx->interrupt_callback.opaque = nullptr;

    int frame_cnt = 0;
    //av start time in microseconds
    int64_t start_time_av = av_gettime();
    AVRational time_base = vid_codec_ctx->time_base; //printf("\ntime_base: %f \n", av_q2d(time_base));
    //AVRational time_base_q = { 1, AV_TIME_BASE };
    
    // frame pixel data info
    int data_size = width * height * 4;
    uint8_t* data = new uint8_t[data_size];

    while (RtspStreaming::IsStreaming())
    {
        /* make sure the frame data is writable */
        if (av_frame_make_writable(frame) < 0)
        {
            Debug::Error("Can't make frame writable");
            break;
        }
        
        //get copy/ref of the texture
        //uint8_t* data = WindowManager::Get().GetWindow(RtspStreaming::WindowId())->GetBuffer();
        if (!RtspStreaming::ActiveWindow()->GetPixels(data, 0, 0, width, height, true))
        {
            Debug::Error("Failed to get frame buffer. ID: ", RtspStreaming::WindowId());
            std::this_thread::sleep_for (std::chrono::seconds(2));
            continue;
        }
        //printf("got pixels data\n");
        // convert BGRA to yuv420 pixel format
        int srcStrides[1] = { 4 * width };
        if (sws_scale(conversion_ctx, &data, srcStrides, 0, height, frame->data, frame->linesize) < 0)
        {
            Debug::Error("Unable to scale d3d11 texture to frame. ", frame_cnt);
            break;
        }
        //Debug::Log("frame pts: ", frame->pts, "  time_base:", av_rescale_q(1, vid_codec_ctx->time_base, vid_stream->time_base));
        frame->pts = frame_cnt++; //av_rescale_q(1, vid_codec_ctx->time_base, vid_stream->time_base);
        //frame_cnt++;
        //printf("scale conversion done\n");
        
        int ret = avcodec_send_frame(vid_codec_ctx, frame);
        if (ret < 0)
        {
            Debug::Error("Error sending frame to codec context! ",frame_cnt);
            break;
        }
        
        AVPacket* pkt = av_packet_alloc();
        //av_init_packet(pkt);
        ret = avcodec_receive_packet(vid_codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            //av_packet_unref(pkt);
            av_packet_free(&pkt);
            continue;
        }
        else if (ret < 0)
        {
            Debug::Error("Error during receiving packet: ",AVERROR(ret));
            //av_packet_unref(pkt);
            av_packet_free(&pkt);
            break;
        }

        if (pkt->pts == AV_NOPTS_VALUE)
        {
            //printf("pkt no pts value. frame_cnt: %d", frame_cnt);
            //Write PTS
            //Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(vid_stream->r_frame_rate);
            //Parameters
            pkt->pts = (double)(frame_cnt * calc_duration) / (double)(av_q2d(time_base) * AV_TIME_BASE);
            pkt->dts = pkt->pts;
            pkt->duration = (double)calc_duration / (double)(av_q2d(time_base) * AV_TIME_BASE);
        }
        else {

            pkt->pts = av_rescale_q_rnd(pkt->pts, vid_codec_ctx->time_base, vid_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt->dts = av_rescale_q_rnd(pkt->dts, vid_codec_ctx->time_base, vid_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt->duration = av_rescale_q(pkt->duration, vid_codec_ctx->time_base, vid_stream->time_base);
        }
        //int64_t pts_time = av_rescale_q(pkt->dts, time_base, time_base_q);
        //int64_t now_time = av_gettime() - start_time_av;

        //if (pts_time > now_time)
        //    av_usleep(pts_time - now_time);

        //pkt->pos = -1;

        //Debug::Log(video_time.count());
        //write frame and send
        if (av_interleaved_write_frame(ofmt_ctx, pkt)<0)
        {
            Debug::Error("Error muxing packet, frame number:",frame_cnt);
            break;
        }

        //Debug::Log("RTSP streaming...");
        //sstd::this_thread::sleep_for(std::chrono::milliseconds(1000/20));
        //av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    //av_free_packet(pkt);
    delete[] data;
    
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(ofmt_ctx);
    av_frame_unref(frame);
    av_frame_free(&frame);
    //printf("streaming thread CLOSED!\n");
}

bool RtspStreaming::StartStreaming(int id, const char* url, int target_fps, bool encrypt)
{
    win_id = id;
    active_winptr = WindowManager::Get().GetWindow(id);

    // check to see if window is active and updating
    if (!active_winptr->IsEnabled()) {
        Debug::Error("selected windows id is not ready yet!");
        return false;
    }

    if (IsConfigured())
    {
        //clear previous stream configuration
        ClearConfig();
    }

    rtsp_url = (char*)url;
    //auth_user = (char*)user;
    //auth_pass = (char*)pass;
    stream_fps = target_fps;
    encryption_enabled = encrypt;

    //set video resolution from texture prop
    width = active_winptr->GetTextureWidth();
    height = active_winptr->GetTextureHeight();
    Debug::Log("win id: ",win_id,"  res: ", width,"x", height);

    if (!SetupStreaming())
        return false;
    
    RtspStreaming::is_streaming = true;
    //start streaming thread process
    stream_th = std::thread(RtspStream, ofmt_ctx, vid_stream, vid_codec_ctx, rtsp_url);
    return true;
}

// Stop or clear streaming
bool RtspStreaming::StopStreaming()
{
    RtspStreaming::is_streaming = false;
    Debug::Log("stopping stream: ", RtspStreaming::IsStreaming(), ";\nwaiting for thread to stop ");
    //stop and wait for stream thread to finish
    if (stream_th.joinable())
        stream_th.join();
    
    //clear video stream and context
    //if (vid_codec_ctx) {
    //    avcodec_close(vid_codec_ctx); //printf("avcodec closed\n");
    //    avcodec_free_context(&vid_codec_ctx); //printf("avcodec freed\n");
    //}
    
    /* Close the output file. */
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    Debug::Log("thread finished. Clearing config");
    ClearConfig();
    Debug::Log("Streaming stopped");
    return true;
}

bool RtspStreaming::SetupStreaming()
{
    RtspStreaming::configured = false;
    Debug::Log("Setting up stream configuration");
    /* Initialize AVFormat context */
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, "rtsp", rtsp_url); //RTSP
    if (!ofmt_ctx)
    {
        Debug::Error("Could not deduce output format from file extension: using mpeg.\n");
        //flv-rtmp, mpegts-udp
        avformat_alloc_output_context2(&ofmt_ctx, nullptr, "mpeg", rtsp_url);
        //return false;
    }
    if (!ofmt_ctx || !ofmt_ctx->oformat)
    {
        Debug::Error("Error creating outformat\n");
        return false;
    }

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        int ret = avio_open2(&ofmt_ctx->pb, rtsp_url, AVIO_FLAG_WRITE, nullptr, nullptr);
        if (ret < 0)
        {
            Debug::Error("Could not open output IO context!",rtsp_url);
            return false;
        }
    }

    //create new interrupt callback for libav
    ofmt_ctx->interrupt_callback.callback = HeaderInterrupt_CB;
    ofmt_ctx->interrupt_callback.opaque = &ofmt_ctx;

    //vid_codec = avcodec_find_encoder(ofmt_ctx->oformat->video_codec); //crashes: mpeg4 codec type/id mismatch
    vid_codec = avcodec_find_encoder(target_codec_id);  //fails at avcodec_open2
    if (!vid_codec)
    {
        Debug::Error("Failed to find video encoder: ",avcodec_get_name(ofmt_ctx->oformat->video_codec));
        return false;
    }
    ofmt_ctx->oformat->video_codec = target_codec_id;
    Debug::Log("Using video codec: ", avcodec_get_name(vid_codec->id));
    vid_stream = avformat_new_stream(ofmt_ctx,vid_codec);
    vid_codec_ctx = avcodec_alloc_context3(vid_codec);

    if (!vid_codec_ctx || !vid_stream)
    {
        Debug::Error("failed to create video stream/context");
        return false;
    }
    
    SetCodecParams(ofmt_ctx, vid_codec_ctx);
    if (!InitializeVidStream(vid_stream, vid_codec_ctx, vid_codec))
        return false;
    
    RtspStreaming::configured = true;
    return true;
}

/* Initialize video stream and options */
bool RtspStreaming::InitializeVidStream(AVStream* stream, AVCodecContext* codec_ctx, AVCodec* codec)
{
    if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0)
    {
        Debug::Error("Could not initialize stream codec parameters!");
        return false;
    }

    AVDictionary* codec_options = nullptr;
    av_dict_set(&codec_options, "-c:v", "libx264", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);
    //av_dict_set(&codec_options, "threads", "2", 0);
    av_dict_set(&codec_options, "muxdelay", "0.1", 0);
    //av_dict_set(&codec_options, "rtsp_transport", "udp", 0);
    //ultrafast superfast veryfast faster fast medium – default preset
    //crt for controlling the quality [0,51]: 23 default with exponential effect on bit rate. 
    //crt 17 visually lossless & 28 nearly half the bitrate
    av_dict_set(&codec_options, "preset", "veryfast", 0);
    //if (codec->id == AV_CODEC_ID_H264) {
    //    // some old/obsolete devices use baseline/main profile. 
    //    // most modern devices support modern high profiles
    //    av_dict_set(&codec_options, "profile", "high", 0); 
    //}
    // open video encoder
    int ret = avcodec_open2(codec_ctx, codec, &codec_options);
    if (ret<0) {
        Debug::Error("Could not open video encoder: ", avcodec_get_name(codec->id), " error ret: ", AVERROR(ret));
        return false;
    }

    stream->codecpar->extradata = codec_ctx->extradata;
    stream->codecpar->extradata_size = codec_ctx->extradata_size;

    Debug::Log("Codec params set successfully and encoder initialized");
    return true;
}

void RtspStreaming::ClearConfig()
{
    if (!IsConfigured()) return;

    if (IsStreaming())
        StopStreaming();

    init_rtsp_header = false;
    Debug::Log("RtspStreaming configuration resetted");
    configured = false;
}

void RtspStreaming::SetCodecParams(AVFormatContext* fctx, AVCodecContext* codec_ctx)
{
    codec_ctx->bit_rate = 900000;
    //codec_ctx->bit_rate_tolerance = 0;
    //codec_ctx->rc_buffer_size = 3000000;
    //codec_ctx->rc_max_rate = 1200000;
    codec_ctx->codec_tag = 0;
    //codec_ctx->codec_id = target_codec_id;
    //codec_ctx->codec_id = fctx->oformat->video_codec;
    //codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    //codec_ctx->gop_size = 12;
    codec_ctx->gop_size = 40;
    //codec_ctx->max_b_frames = 3;
    codec_ctx->pix_fmt = target_pix_fmt;
    codec_ctx->framerate = { stream_fps, 1 };
    codec_ctx->time_base = { 1, stream_fps};
    if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
}