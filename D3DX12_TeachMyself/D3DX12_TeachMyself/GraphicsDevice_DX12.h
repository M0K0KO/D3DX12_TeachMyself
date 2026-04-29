#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "GraphicsDevice.h"
#include "CommandContext_DX12.h"
#include "DescriptorAllocator.h"
#include "DescriptorHandle.h"
#include "UploadHeapRingAllocator.h"
#include "UploadQueue.h"
#include "GPUTimestampProfiler.h"

using namespace Microsoft::WRL;

static const UINT FRAMECOUNT = 3;

class GraphicsDevice_DX12 : public GraphicsDevice
{
	friend class CommandContext_DX12;
public:
	GraphicsDevice_DX12() = default;

	void Initialize(void* hwnd, const uint32_t width, const uint32_t height) override;
	void Shutdown() override;

	GPUBufferHandle CreateBuffer(const BufferDesc desc, const void* initialData = nullptr) override;
	void UpdateBuffer(GPUBufferHandle h, const void* data, size_t size, size_t offset = 0) override;
	
	GPUTextureHandle CreateTexture(const TextureInitDesc& init) override;

	GPUTextureHandle CreateRTTexture(const TextureDesc& desc) override;
	GPUTextureHandle CreateDSTexture(const TextureDesc& desc) override;
	GPUTextureHandle CreateUAVTexture(const TextureDesc& desc) override;
	GPUTextureHandle CreateDSCubemapTexture(const CubemapTextureDesc& desc) override;
	GPUTextureHandle CreateUAVCubemapTexture(const CubemapTextureDesc& desc) override;

	PipelineHandle CreatePipeline(const PipelineDesc desc) override;
	PipelineHandle CreateComputePipeline(const ComputePipelineDesc desc) override;

	GPUCommandSignatureHandle CreateCommandSignature(uint32_t byteStride, std::span<const IndirectArgDesc> args, PipelineHandle psoHandle) override;

	DescriptorHandle GetSRVHandle(GPUTextureHandle handle) override;
	DescriptorHandle GetSRVHandle(GPUBufferHandle handle) override;
	DescriptorHandle GetUAVHandle(GPUTextureHandle handle, uint32_t mip = 0) override;

	void DestroyBuffer(GPUBufferHandle handle) override;
	void DestroyTexture(GPUTextureHandle handle) override;
	void DestroyCommandSignature(GPUCommandSignatureHandle handle) override;

	void ExecuteImmediate(std::function<void(CommandContext&)> fn) override;
	CommandContext& BeginFrame() override;
	void EndFrame() override;

	GPUTextureHandle GetCurrentBackBuffer() override;
	GPUTextureHandle* GetCurrentBackBufferPtr() override;

	GPUIndexBufferView GetIndexBufferView(GPUBufferHandle buffer, uint32_t offsetInBytes, uint32_t sizeInBytes, Format indexFormat) override;

	void ResizeSwapChain(uint32_t width, uint32_t height) override;
	uint32_t GetWidth() override;
	uint32_t GetHeight() override;

	float GetTimestampMs(uint32_t passIndex) override;
	void WaitForGpu() override;

public:
	const ComPtr<ID3D12Resource> GetTextureResource(GPUTextureHandle handle);

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
		DescriptorHandle srv;

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
		PrimitiveTopology topology;
	};

	struct InternalCommandSignature
	{
		ComPtr<ID3D12CommandSignature> cmdSig;
	};

	std::vector<InternalBuffer> m_buffers;
	std::vector<InternalTexture> m_textures;
	std::vector<InternalPipeline> m_pipelines;
	std::vector<InternalCommandSignature> m_cmdSigs;

	struct PendingResourceRelease
	{
		ComPtr<ID3D12Resource> resource;
		uint64_t fenceValue = 0;
	};
	std::deque<PendingResourceRelease> m_pendingResourceReleases;

	GPUTextureHandle m_backBufferHandles[FRAMECOUNT];

	GPUTimestampProfiler m_profiler;
};