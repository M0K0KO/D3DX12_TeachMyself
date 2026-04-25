#pragma once
#include "RHITypes.h"
#include "DescriptorHandle.h"
#include <functional>
#include <string>

class CommandContext;

class GraphicsDevice
{
public:
	virtual ~GraphicsDevice() = default;

	virtual void Initialize(void* hwnd, const uint32_t width, const uint32_t height) = 0;
	virtual void Shutdown() = 0;

	virtual GPUBufferHandle CreateBuffer(const BufferDesc desc, const void* initialData = nullptr) = 0;

	virtual GPUTextureHandle CreateTexture(const TextureInitDesc& init) = 0;
	virtual GPUTextureHandle CreateRTTexture(const TextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateDSTexture(const TextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateUAVTexture(const TextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateDSCubemapTexture(const CubemapTextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateUAVCubemapTexture(const CubemapTextureDesc& desc) = 0;

	virtual PipelineHandle CreatePipeline(const PipelineDesc desc) = 0;
	virtual PipelineHandle CreateComputePipeline(const ComputePipelineDesc desc) = 0;

	virtual DescriptorHandle GetSRVHandle(GPUTextureHandle handle) = 0;
	virtual DescriptorHandle GetUAVHandle(GPUTextureHandle handle, uint32_t mip = 0) = 0;

	virtual void DestroyBuffer(GPUBufferHandle handle) = 0;
	virtual void DestroyTexture(GPUTextureHandle handle) = 0;

	virtual void BeginTextureUpload() = 0;
	virtual GPUTextureHandle LoadTexture(const std::wstring& path) = 0;
	virtual void FlushTextureUploads() = 0;

	virtual void ExecuteImmediate(std::function<void(CommandContext&)> fn) = 0;
	virtual CommandContext& BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual GPUTextureHandle GetCurrentBackBuffer() = 0;
	virtual GPUTextureHandle* GetCurrentBackBufferPtr() = 0;

	virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;
	virtual uint32_t GetWidth() = 0;
	virtual uint32_t GetHeight() = 0;

	virtual void WaitForGpu() = 0;

	virtual float GetTimestampMs(uint32_t passIndex) = 0;

};