#pragma once
#include "RHITypes.h"
#include "DescriptorHandle.h"
#include <functional>
#include <string>

class CommandContext;

struct GPUIndexBufferView
{
	uint64_t gpuAddress;
	uint32_t sizeInBytes;
	Format format;
};

class GraphicsDevice
{
public:
	virtual ~GraphicsDevice() = default;

	virtual void Initialize(void* hwnd, const uint32_t width, const uint32_t height) = 0;
	virtual void Shutdown() = 0;

	virtual GPUBufferHandle CreateBuffer(const BufferDesc desc, const void* initialData = nullptr) = 0;
	virtual void UpdateBuffer(GPUBufferHandle h, const void* data, size_t size, size_t offset = 0) = 0;

	virtual GPUTextureHandle CreateTexture(const TextureInitDesc& init) = 0;
	virtual GPUTextureHandle CreateRTTexture(const TextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateDSTexture(const TextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateUAVTexture(const TextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateDSCubemapTexture(const CubemapTextureDesc& desc) = 0;
	virtual GPUTextureHandle CreateUAVCubemapTexture(const CubemapTextureDesc& desc) = 0;

	virtual PipelineHandle CreatePipeline(const PipelineDesc desc) = 0;
	virtual PipelineHandle CreateComputePipeline(const ComputePipelineDesc desc) = 0;

	virtual GPUCommandSignatureHandle CreateCommandSignature(uint32_t byteStride, std::span<const IndirectArgDesc> args, PipelineHandle psoHandle) = 0;

	virtual DescriptorHandle GetSRVHandle(GPUTextureHandle handle) = 0;
	virtual DescriptorHandle GetSRVHandle(GPUBufferHandle handle) = 0;
	virtual DescriptorHandle GetUAVHandle(GPUTextureHandle handle, uint32_t mip = 0) = 0;

	virtual void DestroyBuffer(GPUBufferHandle handle) = 0;
	virtual void DestroyTexture(GPUTextureHandle handle) = 0;
	virtual void DestroyCommandSignature(GPUCommandSignatureHandle handle) = 0;

	virtual void ExecuteImmediate(std::function<void(CommandContext&)> fn) = 0;
	virtual CommandContext& BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual GPUTextureHandle GetCurrentBackBuffer() = 0;
	virtual GPUTextureHandle* GetCurrentBackBufferPtr() = 0;

	virtual GPUIndexBufferView GetIndexBufferView(GPUBufferHandle buffer, uint32_t offsetInBytes, uint32_t sizeInBytes, Format indexFormat) = 0;

	virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;
	virtual uint32_t GetWidth() = 0;
	virtual uint32_t GetHeight() = 0;

	virtual void WaitForGpu() = 0;

	virtual float GetTimestampMs(uint32_t passIndex) = 0;
};