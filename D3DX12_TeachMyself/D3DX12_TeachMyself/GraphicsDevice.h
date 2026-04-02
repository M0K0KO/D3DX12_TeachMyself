#pragma once
#include "RHITypes.h"
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
	virtual PipelineHandle CreatePipeline(const PipelineDesc desc) = 0;

	virtual void BeginTextureUpload() = 0;
	virtual TextureHandle LoadTexture(const std::wstring& path) = 0;
	virtual void FlushTextureUploads() = 0;

	virtual CommandContext& BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual TextureHandle GetCurrentBackBuffer() = 0;
	virtual TextureHandle* GetCurrentBackBufferPtr() = 0;

	virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;
	virtual uint32_t GetWidth() = 0;
	virtual uint32_t GetHeight() = 0;

	virtual float GetTimestampMs(uint32_t passIndex) = 0;
};