#include "stdafx.h"
#include "SpinningCube.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	//D3D12HelloWindow sample(1280, 720, L"D3D12 Hello Window");
	//D3D12HelloTriangle sample(1280, 720, L"D3D12 Hello Triangle");
	//D3D12HelloTexture sample(1280, 720, L"D3D12 Hello Texture");
	//D3D12HelloConstBuffers sample(1280, 720, L"D3D12 Hello ConstBuffers");
	//D3D12HelloFrameBuffering sample(1280, 720, L"D3D12 Hello FrameBuffering");
	SpinningCube cube(1280, 720, L"Spinning Cube");
	return Win32Application::Run(&cube, hInstance, nCmdShow);
}