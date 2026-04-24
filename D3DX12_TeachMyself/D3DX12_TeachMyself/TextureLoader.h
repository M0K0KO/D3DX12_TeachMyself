#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <mutex>
#include <vector>
#include <string>

using namespace Microsoft::WRL;

struct DDSTextureHandle
{
	ComPtr<ID3D12Resource> resource;
	DXGI_FORMAT format;
	UINT width;
	UINT height;
	UINT mipLevels;
};

class TextureLoader
{
public:
	TextureLoader(ID3D12Device* device);

	DDSTextureHandle LoadDDS(
		ID3D12GraphicsCommandList* cmdList,
		const std::wstring& path);

	void CleanupUploads();

private:
	ID3D12Device* m_device;
	std::mutex m_pendingUploadsMutex;
	std::vector<ComPtr<ID3D12Resource>> m_pendingUploads;
};