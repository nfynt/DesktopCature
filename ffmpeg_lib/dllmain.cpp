#pragma once

#include "dllmain.h"

void GetWidthAndHeight(const char* filename, int* width, int* height)
{
    *width = *height = -1;
    
    // open video file
    AVFormatContext* pFormatCtx = avformat_alloc_context();

    int ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL);
    if (ret != 0) {
        printf("Unable to open video file: %s\n", filename);
        return;
    }
    printf("Total stream= %d\n", pFormatCtx->nb_streams);

    // Retrieve stream information
    ret = avformat_find_stream_info(pFormatCtx, NULL);

    AVCodecParameters* codec_pars;
    int videoStream = -1;

    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
        printf("stream %d type: %d\n", i, pFormatCtx->streams[i]->codecpar->codec_type);
    } // end for i
    if (videoStream < 0) {
        printf("Unable to find video stream\n");
        return;
    }

    // Get a pointer to the codec context for the video stream
    codec_pars = pFormatCtx->streams[videoStream]->codecpar;
    AVCodec* pLocalCodec = avcodec_find_decoder(codec_pars->codec_id);

    *width = codec_pars->width;
    *height = codec_pars->height;

}

void SaveFrameFromFile(const char* filename)
    {
        AVFormatContext* pFormatCtx = avformat_alloc_context();
        AVCodecParameters* pCodecParams;
        int videoStream = -1;
        int i = 0;

        // open video file
        if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
            printf("Unable to open video file: %s\n", filename);
            return;
        }
        
        // Retrieve stream information
        avformat_find_stream_info(pFormatCtx, NULL);

        for (i = 0; i < pFormatCtx->nb_streams; i++) {
            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStream = i;
                break;
            }
            printf("stream %d type: %d\n", i, pFormatCtx->streams[i]->codecpar->codec_type);
        } // end for i
        if (videoStream < 0) {
            printf("Unable to find video stream\n");
            return;
        }

        // Get a pointer to the codec context for the video stream
        pCodecParams = pFormatCtx->streams[videoStream]->codecpar;
        AVCodec* pLocalCodec = avcodec_find_decoder(pCodecParams->codec_id);

        // specific for video and audio
        if (pCodecParams->codec_type == AVMEDIA_TYPE_VIDEO) {
            printf("Video Codec: resolution %d x %d", pCodecParams->width, pCodecParams->height);
        }
        // general
        printf("\nCodec %s ID %d bit_rate %lld\n", pLocalCodec->long_name, pLocalCodec->id, pCodecParams->bit_rate);

        AVCodecContext* pCodecContext = avcodec_alloc_context3(pLocalCodec);
        avcodec_parameters_to_context(pCodecContext, pCodecParams);
        avcodec_open2(pCodecContext, pLocalCodec, NULL);

        AVPacket* pPacket = av_packet_alloc();
        AVFrame* pFrame = av_frame_alloc();

        int fr = 0;
        while (av_read_frame(pFormatCtx, pPacket) >= 0) {
            
            avcodec_send_packet(pCodecContext, pPacket);
            avcodec_receive_frame(pCodecContext, pFrame);

            printf(
                "\nFrame %c (%d) pts %lld dts %lld key_frame %d [coded_picture_number %d, display_picture_number %d]",
                av_get_picture_type_char(pFrame->pict_type),
                pCodecContext->frame_number,
                pFrame->pts,
                pFrame->pkt_dts,
                pFrame->key_frame,
                pFrame->coded_picture_number,
                pFrame->display_picture_number
            );

            save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, fr++);

            if(fr>3)
            break;
        }
    } //end RTSPFromFile

int StartLiveStream(const char* vid, const char* url)
{
    Streaming streamer(vid, url);

    if (streamer.IsInitialized())
        return streamer.Stream();
    else
        printf("streamer not initialized\n");

    return -1;
}



void save_gray_frame(unsigned char* buf, int wrap, int xsize, int ysize,int fr)
{
    FILE* f;
    int i;
    char filename[100];
    sprintf_s(filename, "C:\\Users\\Shubham-DTT\\Pictures\\bbbuny_360%d.pgm", fr);
    fopen_s(&f,filename, "w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}