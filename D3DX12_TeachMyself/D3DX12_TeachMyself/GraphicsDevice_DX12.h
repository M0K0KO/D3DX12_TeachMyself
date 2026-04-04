#pragma once
#include "stdafx.h"
#include "GraphicsDevice.h"
#include "CommandContext_DX12.h"
#include "UploadHeapRingAllocator.h"
#include "TextureLoader.h"
#include "GPUTimestampProfiler.h"
#include <functional>


using namespace Microsoft::WRL;
using namespace DirectX;

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

	uint32_t GetSRVHeapSlot(TextureHandle handle) override;
	uint32_t GetUAVHeapSlot(TextureHandle handle) override;

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

	const ComPtr<ID3D12Resource> GetTextureResource(TextureHandle handle);
private:
	ComPtr<ID3D12RootSignature> BuildRootSignature(const RootSignatureDesc& desc);
	TextureHandle CreateRTTexture(const TextureDesc& desc);
	TextureHandle CreateSRTexture(const TextureDesc& desc, const void* initialData);
	TextureHandle CreateDSTexture(const TextureDesc& desc);
	
	inline D3D12_DESCRIPTOR_RANGE_TYPE GetDX12DescriptorRangeType(RangeType type);
	inline D3D12_SHADER_VISIBILITY GetDX12ShaderVisibility(ShaderVisibility visibility);
	inline D3D12_COMPARISON_FUNC GetDX12ComparisonFunc(ComparisonFunc func);
	inline D3D12_CULL_MODE GetDX12CullMode(CullMode mode);
	inline DXGI_FORMAT GetDXGIFormat(Format format);
	inline D3D12_STATIC_SAMPLER_DESC GetDX12StaticSamplerDesc(const StaticSamplerDesc& desc);
	inline Format GetRHIFormat(DXGI_FORMAT format);
	inline const char* GetSemanticString(Semantic semantic);
	inline UINT Align256(UINT size);
	inline UINT GetBytesPerPixel(Format format);
	void WaitForGpu();
	void MoveToNextFrame();

private:
	static const UINT FrameCount = 3;

	UINT m_width;
	UINT m_height;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	CommandContext_DX12 m_commandContext;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[FrameCount];

	std::unique_ptr<UploadHeapRingAllocator> uploadHeapAllocator;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_immediateAllocator;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;
	UINT m_dsvDescriptorSize;

	struct InternalBuffer
	{
		ComPtr<ID3D12Resource> resource;
		ComPtr<ID3D12Resource> frameResources[FrameCount];
		UINT8* mappedPointers[FrameCount] = {};
		uint32_t heapSlot = UINT32_MAX;
		BufferDesc desc;
	};

	struct InternalTexture
	{
		ComPtr<ID3D12Resource> resource;
		TextureDesc desc;
		CubemapTextureDesc cubeDesc;
		uint32_t srvHeapSlot = UINT32_MAX;
		uint32_t uavHeapSlot = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	};

	struct InternalPipeline
	{
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pso;
	};

	std::vector<InternalBuffer> m_buffers;
	std::vector<InternalTexture> m_textures;
	std::vector<InternalPipeline> m_pipelines;

	TextureHandle m_backBufferHandles[FrameCount];

	uint32_t m_rtvHeapNextSlot = 0;
	uint32_t m_cbvSrvHeapNextSlot = 0;

	std::unique_ptr<TextureLoader> m_pTextureLoader;

	UINT m_cbvSrvDescriptorSize;

	GPUTimestampProfiler m_profiler;
};