#pragma once
#include "RHITypes.h"

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

	virtual void UpdateBuffer(const BufferHandle handle, const void* data, const uint32_t size) = 0;

	virtual CommandContext& BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual TextureHandle GetCurrentBackBuffer() = 0;
	virtual TextureHandle* GetCurrentBackBufferPtr() = 0;

	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
};