#pragma once
#include "stdafx.h"
#include "GraphicsDevice.h"
#include "CommandContext_DX12.h"
#include "UploadHeapRingAllocator.h"
#include "TextureLoader.h"


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
	PipelineHandle CreatePipeline(const PipelineDesc desc) override;

	void BeginTextureUpload() override;
	TextureHandle LoadTexture(const std::wstring& path) override;
	void FlushTextureUploads() override;

	CommandContext& BeginFrame() override;
	void EndFrame() override;

	TextureHandle GetCurrentBackBuffer() override;
	TextureHandle* GetCurrentBackBufferPtr() override;

	int GetWidth() override;
	int GetHeight() override;

	const ComPtr<ID3D12Resource> GetTextureResource(TextureHandle handle);
private:
	inline D3D12_DESCRIPTOR_RANGE_TYPE GetDX12DescriptorRangeType(RangeType type);
	inline D3D12_SHADER_VISIBILITY GetDX12ShaderVisibility(ShaderVisibility visibility);
	inline D3D12_COMPARISON_FUNC GetDX12ComparisonFunc(ComparisonFunc func);
	inline DXGI_FORMAT GetDXGIFormat(Format format);
	inline Format GetRHIFormat(DXGI_FORMAT format);
	inline const char* GetSemanticString(Semantic semantic);
	inline UINT Align256(UINT size);
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
		uint32_t srvHeapSlot = UINT32_MAX;
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
};