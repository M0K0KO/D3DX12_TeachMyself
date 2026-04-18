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

	virtual BufferHandle CreateBuffer(const BufferDesc desc, const void* initialData = nullptr) = 0;
	virtual TextureHandle CreateTexture(const TextureDesc desc, const void* initialData = nullptr) = 0;
	virtual TextureHandle CreateCubemapTexture(const CubemapTextureDesc desc, const void* initialData = nullptr) = 0;
	virtual PipelineHandle CreatePipeline(const PipelineDesc desc) = 0;
	virtual PipelineHandle CreateComputePipeline(const ComputePipelineDesc desc) = 0;

	virtual DescriptorHandle GetSRVHandle(TextureHandle handle) = 0;
	virtual DescriptorHandle GetUAVHandle(TextureHandle handle, uint32_t mip = 0) = 0;

	virtual void BeginTextureUpload() = 0;
	virtual TextureHandle LoadTexture(const std::wstring& path) = 0;
	virtual void FlushTextureUploads() = 0;

	virtual void ExecuteImmediate(std::function<void(CommandContext&)> fn) = 0;
	virtual CommandContext& BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual TextureHandle GetCurrentBackBuffer() = 0;
	virtual TextureHandle* GetCurrentBackBufferPtr() = 0;

	virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;
	virtual uint32_t GetWidth() = 0;
	virtual uint32_t GetHeight() = 0;

	virtual float GetTimestampMs(uint32_t passIndex) = 0;
};