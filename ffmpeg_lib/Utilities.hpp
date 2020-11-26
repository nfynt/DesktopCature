#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <d3d11.h>
#include <dxgi.h>
#include <wrl.h>

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
