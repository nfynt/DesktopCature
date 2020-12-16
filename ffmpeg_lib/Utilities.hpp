#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <d3d11.h>
#include <dxgi.h>
#include <wrl.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
};

bool dx_tex_init = false;

void setup_directx_context()
{
    /*DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 640;
    sd.BufferDesc.Height = 480;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 20;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = FALSE;

    const D3D_FEATURE_LEVEL lvl[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif


    ID3D11Device* device = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, lvl, _countof(lvl), D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3ddevice, &FeatureLevelsSupported, &g_pImmediateContext);
    if (hr == E_INVALIDARG)
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &lvl[1], _countof(lvl) - 1, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3ddevice, &FeatureLevelsSupported, &g_pImmediateContext);
    }


    if (FAILED(hr))
        return hr;

    dx_tex_init = true;*/
}

/* Prepare a dummy image. */
static bool fill_yuv_image(AVFrame* frame, int frame_index, int width, int height)
{
    if (frame == NULL)
    {
        frame = av_frame_alloc();
        if (!frame) {
            printf("Could not allocate video frame\n");
            return false;
        }
        //1 cr & cb for every 2x2 Y samples
        frame->format = AV_PIX_FMT_YUV420P;
        frame->width = width;
        frame->height = height;
    }
    /* make sure the frame data is writable */;
    if (av_frame_make_writable(frame) < 0)
    {
        printf("frame not writable\n"); return false;
    }

    /* Y */
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            frame->data[0][y * frame->linesize[0] + x] = x + y + frame_index * 3;

    /* Cb and Cr */
    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + frame_index * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }
    //frame->pts = frame_index;

    return true;
}

// Set a dummy d3d11 texture to the frame
static bool fill_d3d11_tex_image(AVFrame* frame, int frame_index, int width, int height)
{
    if (frame == NULL)
    {
        frame = av_frame_alloc();
        if (!frame) {
            printf("Could not allocate video frame\n");
            return false;
        }
        frame->format = AV_PIX_FMT_RGBA;
        frame->width = width;
        frame->height = height;
    }
    /* make sure the frame data is writable */;
    if (av_frame_make_writable(frame) < 0)
    {
        printf("frame not writable\n"); return false;
    }

    // D3D objects
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   //GUID_WICPixelFormat32bppRGBA
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    ID3D11Device* d3dDevice; // Don't forget to initialize this
    ID3D11Texture2D* tex = nullptr;
    d3dDevice->CreateTexture2D(&desc, NULL, &tex);

    //AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    //AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
    //AVD3D11VADeviceContext* d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
    //// m_device is our ComPtr<ID3D11Device>
    //d3d11va_device_ctx->device = m_device.Get();
    //// codec_ctx is a pointer to our FFmpeg AVCodecContext
    //codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    //av_hwdevice_ctx_init(codec_ctx->hw_device_ctx);

    //Microsoft::WRL::ComPtr<ID3D11Texture2D> texture = (ID3D11Texture2D*)frame->data[0];

    /*SwsContext* conversion_ctx = sws_getContext(
        SRC_WIDTH, SRC_HEIGHT, AV_PIX_FMT_NV12,
        DST_WIDTH, DST_HEIGHT, AV_PIX_FMT_RGBA,
        SWS_BICUBLIN | SWS_BITEXACT, nullptr, nullptr, nullptr);
        // decode frame
AVFrame* sw_frame = av_frame_alloc();
av_hwframe_transfer_data(sw_frame, frame, 0);
sws_scale(conversion_ctx, sw_frame->data, sw_frame->linesize,
          0, sw_frame->height, dst_data, dst_linesize);
sw_frame->data = dst_data
sw_frame->linesize = dst_linesize
sw_frame->pix_fmt = AV_PIX_FMT_RGBA
sw_frame->width = DST_WIDTH
sw_frame->height = DST_HEIGHT
        */
}

//convert ffmpeg frame to dx_tex
void convert_frame_to_tex() 
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
}

//convert dx_tex to ffmpeg frame

void convert_tex_to_frame()
{

}


#endif // !UTILITIES_HPP
