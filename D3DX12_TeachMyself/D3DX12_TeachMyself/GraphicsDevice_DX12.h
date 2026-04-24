#pragma once
#include "stdafx.h"
#include "GraphicsDevice.h"
#include "CommandContext_DX12.h"
#include "UploadHeapRingAllocator.h"
#include "TextureLoader.h"
#include "GPUTimestampProfiler.h"
#include <functional>
#include "DescriptorAllocator.h"
#include "DescriptorHandle.h"
#include <array>
#include "UploadQueue.h"

using namespace Microsoft::WRL;
using namespace DirectX;

static const UINT FRAMECOUNT = 3;

class GraphicsDevice_DX12 : public GraphicsDevice
{
	friend class CommandContext_DX12;
public:
	GraphicsDevice_DX12() = default;

	void Initialize(void* hwnd, const uint32_t width, const uint32_t height) override;
	void Shutdown() override;

	BufferHandle CreateBuffer(const BufferDesc desc, const void* initialData = nullptr) override;
	TextureHandle CreateTexture(const TextureDesc desc, const void* initialData = nullptr) override;
	TextureHandle CreateCubemapTexture(const CubemapTextureDesc desc, const void* initialData = nullptr) override;

	PipelineHandle CreatePipeline(const PipelineDesc desc) override;
	PipelineHandle CreateComputePipeline(const ComputePipelineDesc desc) override;

	DescriptorHandle GetSRVHandle(TextureHandle handle) override;
	DescriptorHandle GetUAVHandle(TextureHandle handle, uint32_t mip = 0) override;

	void DestroyBuffer(BufferHandle handle) override;
	void DestroyTexture(TextureHandle handle) override;

	void BeginTextureUpload() override;
	TextureHandle LoadTexture(const std::wstring& path) override;
	void FlushTextureUploads() override;

	void ExecuteImmediate(std::function<void(CommandContext&)> fn) override;
	CommandContext& BeginFrame() override;
	void EndFrame() override;

	TextureHandle GetCurrentBackBuffer() override;
	TextureHandle* GetCurrentBackBufferPtr() override;

	void ResizeSwapChain(uint32_t width, uint32_t height) override;
	uint32_t GetWidth() override;
	uint32_t GetHeight() override;

	float GetTimestampMs(uint32_t passIndex) override;
	void WaitForGpu() override;

public:
	const ComPtr<ID3D12Resource> GetTextureResource(TextureHandle handle);

	DescriptorAllocator& GetCbvSrvUavAllocator() { return m_cbvSrvUavAllocator; }
	DescriptorAllocator& GetRtvAllocator() { return m_rtvAllocator; }
	DescriptorAllocator& GetDsvAllocator() { return m_dsvAllocator; }

	DescriptorHandle AllocateSRV() { return m_cbvSrvUavAllocator.Allocate(); }
	DescriptorHandle AllocateUAV() { return m_cbvSrvUavAllocator.Allocate(); }
	DescriptorHandle AllocateCBV() { return m_cbvSrvUavAllocator.Allocate(); }

	DescriptorHandle AllocateRTV() { return m_rtvAllocator.Allocate(); }
	DescriptorHandle AllocateDSV() { return m_dsvAllocator.Allocate(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTV()
	{
		const auto& tex = m_textures[m_backBufferHandles[m_frameIndex].id];
		return tex.rtv.cpu;
	}

public:
	ID3D12Device* GetDevicePtr() { return m_device.Get(); };
	ID3D12CommandQueue* GetCommandQueuePtr() { return m_commandQueue.Get(); };
	DXGI_FORMAT GetSwapChainFormat() { return DXGI_FORMAT_R8G8B8A8_UNORM; };
	GPUTimestampProfiler* GetGPUProfiler() { return &m_profiler; };

private:
	ComPtr<ID3D12RootSignature> BuildRootSignature(const RootSignatureDesc& desc);

	TextureHandle CreateRTTexture(const TextureDesc& desc);
	TextureHandle CreateSRTexture(const TextureDesc& desc, const void* initialData);
	TextureHandle CreateDSTexture(const TextureDesc& desc);
	TextureHandle CreateUAVTexture(const TextureDesc& desc);

	TextureHandle CreateSRCubemapTexture(const CubemapTextureDesc& desc, const void* initialData);
	TextureHandle CreateDSCubemapTexture(const CubemapTextureDesc& desc);
	TextureHandle CreateUAVCubemapTexture(const CubemapTextureDesc& desc);

	void ReserveResources();
	void EnqueueResourceRelease(ComPtr<ID3D12Resource>&& resource, uint64_t fenceValue);
	void ProcessCompletedResourceReleases(uint64_t completedFenceValue);
	
	void MoveToNextFrame();

private:

	UINT m_width;
	UINT m_height;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	CommandContext_DX12 m_commandContext;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[FRAMECOUNT];
	UINT64 m_nextFenceValue = 1;

	std::unique_ptr<UploadHeapRingAllocator> uploadHeapAllocator;

	std::unique_ptr<UploadQueue> m_uploadQueue;
	std::vector<D3D12_RESOURCE_BARRIER> m_pendingTransitions;

	DescriptorAllocator m_cbvSrvUavAllocator;
	DescriptorAllocator m_dsvAllocator;
	DescriptorAllocator m_rtvAllocator;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Resource> m_renderTargets[FRAMECOUNT];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FRAMECOUNT];
	ComPtr<ID3D12CommandAllocator> m_immediateAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12GraphicsCommandList> m_immediateCommandList;

	struct InternalBuffer
	{
		ComPtr<ID3D12Resource> resource;
		ComPtr<ID3D12Resource> frameResources[FRAMECOUNT];
		UINT8* mappedPointers[FRAMECOUNT] = {};
		BufferDesc desc;
	};

	struct InternalTexture
	{
		ComPtr<ID3D12Resource> resource;
		TextureDesc desc;
		CubemapTextureDesc cubeDesc;

		DescriptorHandle srv;
		DescriptorHandle uav;
		DescriptorHandle rtv;
		DescriptorHandle dsv;

		std::vector<DescriptorHandle> uavMips;
		std::array<DescriptorHandle, 6> faceDsvs;
	};

	struct InternalPipeline
	{
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pso;
	};

	std::vector<InternalBuffer> m_buffers;
	std::vector<InternalTexture> m_textures;
	std::vector<InternalPipeline> m_pipelines;

	struct PendingResourceRelease
	{
		ComPtr<ID3D12Resource> resource;
		uint64_t fenceValue = 0;
	};
	std::deque<PendingResourceRelease> m_pendingResourceReleases;

	TextureHandle m_backBufferHandles[FRAMECOUNT];

	std::unique_ptr<TextureLoader> m_pTextureLoader;

	GPUTimestampProfiler m_profiler;
};