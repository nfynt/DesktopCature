#include <dwmapi.h>
#include "WindowTexture.h"
#include "Window.h"
#include "WindowManager.h"
#include "UploadManager.h"
#include "Message.h"
#include "Unity.h"
#include "Debug.h"
#include "Util.h"

using namespace Microsoft::WRL;



WindowTexture::WindowTexture(Window* window)
    : window_(window)
{
}


WindowTexture::~WindowTexture()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    DeleteBitmap();
}


void WindowTexture::SetUnityTexturePtr(ID3D11Texture2D* ptr)
{
    unityTexture_ = ptr;
}


ID3D11Texture2D* WindowTexture::GetUnityTexturePtr() const
{
    return unityTexture_;
}


void WindowTexture::SetCaptureMode(CaptureMode mode)
{
    captureMode_ = mode;
}


CaptureMode WindowTexture::GetCaptureMode() const
{
    return captureMode_;
}


void WindowTexture::SetCursorDraw(bool draw)
{
    drawCursor_ = draw;
}


bool WindowTexture::GetCursorDraw() const
{
    return drawCursor_;
}


UINT WindowTexture::GetWidth() const
{
    return textureWidth_;
}


UINT WindowTexture::GetHeight() const
{
    return textureHeight_;
}


UINT WindowTexture::GetOffsetX() const
{
    return offsetX_;
}


UINT WindowTexture::GetOffsetY() const
{
    return offsetY_;
}


void WindowTexture::CreateBitmapIfNeeded(HDC hDc, UINT width, UINT height)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);

    if (bufferWidth_ == width && bufferHeight_ == height) return;
    if (width == 0 || height == 0) return;

    bufferWidth_ = width;
    bufferHeight_ = height;
    buffer_.ExpandIfNeeded(width * height * 4);

    DeleteBitmap();
    bitmap_ = ::CreateCompatibleBitmap(hDc, width, height);

    SetUnityTexturePtr(nullptr);
}


void WindowTexture::DeleteBitmap()
{
    if (bitmap_ != nullptr) 
    {
        if (!::DeleteObject(bitmap_)) OutputApiError(__FUNCTION__, "DeleteObject");
        bitmap_ = nullptr;
    }
}


bool WindowTexture::Capture()
{
    auto hWnd = window_->GetHandle();

    auto hDc = ::GetDC(hWnd);
    ScopedReleaser hDcReleaser([&] { ::ReleaseDC(hWnd, hDc); });

    BITMAP bmpHeader;
    ZeroMemory(&bmpHeader, sizeof(BITMAP));
    auto hBitmap = ::GetCurrentObject(hDc, OBJ_BITMAP);
    GetObject(hBitmap, sizeof(BITMAP), &bmpHeader);
    auto dcWidth = bmpHeader.bmWidth;
    auto dcHeight = bmpHeader.bmHeight;

    // If failed, use window size (for example, UWP uses this)
    if (dcWidth == 0 || dcHeight == 0 || window_->IsDesktop())
    {
        dcWidth = window_->GetWidth();
        dcHeight = window_->GetHeight();
    }

    if (dcWidth == 0 || dcHeight == 0)
    {
        return false;
    }

    // DPI scale
    dpiScaleX_ = std::fmax(static_cast<float>(window_->GetWidth()) / dcWidth, 0.01f);
    dpiScaleY_ = std::fmax(static_cast<float>(window_->GetHeight()) / dcHeight, 0.01f);

    if (captureMode_ == CaptureMode::BitBlt && !window_->IsDesktop())
    {
        // Remove frame areas
        const auto frameWidth = window_->GetWidth() - window_->GetClientWidth();
        const auto frameHeight = window_->GetHeight() - window_->GetClientHeight();
        dcWidth -= static_cast<LONG>(ceil(frameWidth / dpiScaleX_));
        dcHeight -= static_cast<LONG>(ceil(frameHeight / dpiScaleY_));
    }

    CreateBitmapIfNeeded(hDc, dcWidth, dcHeight);

    {
        UWC_SCOPE_TIMER(DwmGetWindowAttribute)

        const UINT preTextureWidth = textureWidth_;
        const UINT preTextureHeight = textureHeight_;

        // Remove dropshadow area
        if (captureMode_ == CaptureMode::PrintWindow)
        {
            RECT windowRect;
            ::GetWindowRect(hWnd, &windowRect);

            RECT dwmRect;
            ::DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwmRect, sizeof(RECT));

            offsetX_ = max(dwmRect.left - windowRect.left, 0);
            offsetY_ = max(dwmRect.top - windowRect.top, 0);
            textureWidth_ = static_cast<UINT>((dwmRect.right - dwmRect.left) / dpiScaleX_);
            textureHeight_ = static_cast<UINT>((dwmRect.bottom - dwmRect.top) / dpiScaleY_);

            if (::IsZoomed(hWnd))
            {
                auto hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitor = { sizeof(MONITORINFO) };
                ::GetMonitorInfo(hMonitor, &monitor);
                const auto ml = monitor.rcMonitor.left;
                const auto mr = monitor.rcMonitor.right;
                const auto mt = monitor.rcMonitor.top;
                const auto mb = monitor.rcMonitor.bottom;
                const auto wl = dwmRect.left;
                const auto wr = dwmRect.right;
                const auto wt = dwmRect.top;
                const auto wb = dwmRect.bottom;
                if (wl < ml || wt < mt || wr > mr || wb > mb)
                {
                    // Remote taskbar area and get pixels out of the monitor range
                    const auto calcSize = [&](LONG size) -> LONG
                    {
                        constexpr LONG taskBarSizeThresh = 30;
                        if (size > taskBarSizeThresh) return 0;
                        return max(size, 0);
                    };
                    const auto offsetExLeft = max(calcSize(ml - wl), 0);
                    const auto offsetExRight = max(calcSize(wr - mr), 0);
                    const auto offsetExTop = max(calcSize(mt - wt), 0);
                    const auto offsetExBottom = max(calcSize(wb - mb), 0);
                    const auto offsetExX = max(offsetExLeft, offsetExRight);
                    const auto offsetExY = max(offsetExTop, offsetExBottom);
                    textureWidth_ -= offsetExX * 2;
                    textureHeight_ -= offsetExY * 2;
                    offsetX_ += offsetExX;
                    offsetY_ += offsetExY;
                }
            }
        }
        else
        {
            offsetX_ = 0;
            offsetY_ = 0;
            textureWidth_ = bufferWidth_.load();
            textureHeight_ = bufferHeight_.load();
        }

        if (textureWidth_ != preTextureWidth || textureHeight_ != preTextureHeight)
        {
            MessageManager::Get().Add({ MessageType::WindowSizeChanged, window_->GetId(), window_->GetHandle() });
        }
    }

    auto hDcMem = ::CreateCompatibleDC(hDc);
    ScopedReleaser hDcMemRelaser([&] { ::DeleteDC(hDcMem); });

    HGDIOBJ preObject = ::SelectObject(hDcMem, bitmap_);
    ScopedReleaser selectObject([&] { ::SelectObject(hDcMem, preObject); });

    int offsetLeft = 0, offsetRight = 0, offsetTop = 0, offsetBottom = 0;

    switch (captureMode_)
    {
        case CaptureMode::PrintWindow:
        {
            UWC_SCOPE_TIMER(PrintWindow)
            if (!::PrintWindow(hWnd, hDcMem, PW_RENDERFULLCONTENT)) 
            {
                OutputApiError(__FUNCTION__, "PrintWindow");
                return false;
            }
            break;
        }
        case CaptureMode::BitBlt:
        {
            UWC_SCOPE_TIMER(BitBlt)
            const bool isDesktop = window_->IsDesktop();
            const auto x = isDesktop ? window_->GetX() : 0;
            const auto y = isDesktop ? window_->GetY() : 0;
            if (!::BitBlt(hDcMem, 0, 0, bufferWidth_, bufferHeight_, hDc, x, y, SRCCOPY | CAPTUREBLT))
            {
                OutputApiError(__FUNCTION__, "BitBlt");
                return false;
            }
            break;
        }
        default:
        {
            return false;
        }
    }

    // Draw cursor
    if (drawCursor_)
    {
        DrawCursor(hWnd, hDcMem);
    }

    BITMAPINFOHEADER bmi {};
    bmi.biWidth       = static_cast<LONG>(bufferWidth_);
    bmi.biHeight      = -static_cast<LONG>(bufferHeight_);
    bmi.biPlanes      = 1;
    bmi.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.biBitCount    = 32;
    bmi.biCompression = BI_RGB;
    bmi.biSizeImage   = 0;

    {
        std::lock_guard<std::mutex> lock(bufferMutex_);

        if (!::GetDIBits(hDcMem, bitmap_, 0, bufferHeight_, buffer_.Get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS))
        {
            OutputApiError(__FUNCTION__, "GetDIBits");
            return false;
        }
    }

    return true;
}


void WindowTexture::DrawCursor(HWND hWnd, HDC hDcMem)
{
    const auto cursorWindow = WindowManager::Get().GetCursorWindow();
    const bool isCursorWindow = cursorWindow && cursorWindow->GetHandle() == window_->GetHandle();
    if (!isCursorWindow && !window_->IsDesktop()) return;

    CURSORINFO cursorInfo { 0 };
    cursorInfo.cbSize = sizeof(CURSORINFO);
    if (!::GetCursorInfo(&cursorInfo))
    {
        OutputApiError(__FUNCTION__, "GetCursorInfo");
        return;
    }
    POINT pos = cursorInfo.ptScreenPos;

    if (cursorInfo.flags != CURSOR_SHOWING) return;

    int localX = pos.x;
    int localY = pos.y;

    switch (captureMode_)
    {
        case CaptureMode::PrintWindow:
        {
            localX = static_cast<int>((pos.x - window_->GetX()) / dpiScaleX_);
            localY = static_cast<int>((pos.y - window_->GetY()) / dpiScaleY_);
            break;
        }
        case CaptureMode::BitBlt:
        {
            if (window_->IsDesktop())
            {
                localX -= window_->GetX();
                localY -= window_->GetY();
            }
            else
            {
                if (::ScreenToClient(hWnd, &pos))
                {
                    localX = static_cast<int>(pos.x / dpiScaleX_);
                    localY = static_cast<int>(pos.y / dpiScaleY_);
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }

    ::DrawIcon(hDcMem, localX, localY, cursorInfo.hCursor);
}


bool WindowTexture::Upload()
{
    if (!unityTexture_.load()) 
    {
        MessageManager::Get().Add({ MessageType::TextureNullError, window_->GetId(), nullptr });
        return false;
    }

    UWC_SCOPE_TIMER(UploadTexture)

    std::lock_guard<std::mutex> lock(sharedTextureMutex_);

    {
        D3D11_TEXTURE2D_DESC desc;
        unityTexture_.load()->GetDesc(&desc);
        if (desc.Width != GetWidth() && desc.Height != GetHeight())
        {
            MessageManager::Get().Add({ MessageType::TextureSizeError, window_->GetId(), nullptr });
            Debug::Error(__FUNCTION__, " => Texture size is wrong.");
            return false;
        }
    }

    bool shouldUpdateTexture = true;

    if (sharedTexture_)
    {
        D3D11_TEXTURE2D_DESC desc;
        sharedTexture_->GetDesc(&desc);
        if (desc.Width == GetWidth() && desc.Height == GetHeight())
        {
            shouldUpdateTexture = false;
        }
    }

    if (offsetX_ + textureWidth_ > bufferWidth_ || offsetY_ + textureHeight_ > bufferHeight_)
    {
        Debug::Error(__FUNCTION__, " => Offsets are invalid.");
        return false;
    }

    auto& uploader = WindowManager::GetUploadManager();
    if (!uploader) return false;

    if (shouldUpdateTexture)
    {
        sharedTexture_ = uploader->CreateCompatibleSharedTexture(unityTexture_.load());

        if (!sharedTexture_)
        {
            Debug::Error(__FUNCTION__, " => Shared texture is null.");
            return false;
        }

        ComPtr<IDXGIResource> dxgiResource;
        sharedTexture_.As(&dxgiResource);
        if (FAILED(dxgiResource->GetSharedHandle(&sharedHandle_)))
        {
            Debug::Error(__FUNCTION__, " => GetSharedHandle() failed.");
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(bufferMutex_);

        const UINT rawPitch = bufferWidth_ * 4;
        const int startIndex = offsetX_ * 4 + offsetY_ * rawPitch;
        const auto* start = buffer_.Get(startIndex);

        ComPtr<ID3D11DeviceContext> context;
        uploader->GetDevice()->GetImmediateContext(&context);
        context->UpdateSubresource(sharedTexture_.Get(), 0, nullptr, start, rawPitch, 0);
        context->Flush();
    }

    return true;
}


bool WindowTexture::Render()
{
    if (!unityTexture_.load() || !sharedTexture_ || !sharedHandle_) return false;

    UWC_SCOPE_TIMER(Render)

    std::lock_guard<std::mutex> lock(sharedTextureMutex_);

    ComPtr<ID3D11DeviceContext> context;
    GetUnityDevice()->GetImmediateContext(&context);

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(GetUnityDevice()->OpenSharedResource(sharedHandle_, __uuidof(ID3D11Texture2D), &texture)))
    {
        Debug::Error(__FUNCTION__, " => OpenSharedResource() failed.");
        return false;
    }

    context->CopyResource(unityTexture_.load(), texture.Get());

    MessageManager::Get().Add({ MessageType::WindowCaptured, window_->GetId(), window_->GetHandle() });

    return true;
}


BYTE* WindowTexture::GetBuffer()
{
    if (buffer_.Empty()) return nullptr;

    std::lock_guard<std::mutex> lock(bufferMutex_);

    bufferForGetBuffer_.ExpandIfNeeded(buffer_.Size());
    memcpy(bufferForGetBuffer_.Get(), buffer_.Get(), buffer_.Size());

    return bufferForGetBuffer_.Get();
}


UINT WindowTexture::GetPixel(int x, int y) const
{
    BYTE output[4];
    if (GetPixels(output, x, y, 1, 1))
    {
        return *reinterpret_cast<UINT*>(output);
    }
    return 0;
}


bool WindowTexture::GetPixels(BYTE* output, int x, int y, int width, int height, bool flipY) const
{
    if (!buffer_)
    {
        Debug::Error("WindowTexture::GetPixels() => buffer has not been set yet.");
        return false;
    }

    int bufferWidth = bufferWidth_.load();
    int bufferHeight = bufferHeight_.load();
    if (x < 0 || x + width > bufferWidth || y < 0 || y + height > bufferHeight)
    {
        Debug::Error("The given range is out of the buffer area: x=", x, ", y=", y, ", width=", width, ", height=", height);
        Debug::Error("The buffer width=", bufferWidth, ", height=", bufferHeight);
        return false;
    }

    std::lock_guard<std::mutex> lock(bufferMutex_);

    constexpr int rgba = 4;
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            for (int c = 0; c < rgba; ++c)
            {
                const int indexOut = i + j * width;
                const int indexIn = (x + i) + (y + (flipY?j:(height - 1 - j))) * bufferWidth_;
                output[indexOut * rgba + 0] = buffer_[indexIn * rgba + 2];
                output[indexOut * rgba + 1] = buffer_[indexIn * rgba + 1];
                output[indexOut * rgba + 2] = buffer_[indexIn * rgba + 0];
                output[indexOut * rgba + 3] = buffer_[indexIn * rgba + 3];
            }
        }
    }

    return true;
}
