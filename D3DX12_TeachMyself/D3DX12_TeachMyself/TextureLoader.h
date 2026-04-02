#pragma once
#include "stdafx.h"

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
	std::vector<ComPtr<ID3D12Resource>> m_pendingUploads;
};