#include "stdafx.h"
#include "HRException.h"
#include "GraphicsDevice_DX12.h"
#include "Renderer.h"
#include "DX12Helpers.h"

void GraphicsDevice_DX12::ReserveResources()
{
	m_textures.reserve(2048);
	m_buffers.reserve(1024);
	m_pipelines.reserve(256);
}

void GraphicsDevice_DX12::EnqueueResourceRelease(ComPtr<ID3D12Resource>&& resource, uint64_t fenceValue)
{
	if (!resource)
		return;

	m_pendingResourceReleases.push_back({ std::move(resource), fenceValue });
}

void GraphicsDevice_DX12::ProcessCompletedResourceReleases(uint64_t completedFenceValue)
{
	while (!m_pendingResourceReleases.empty() && m_pendingResourceReleases.front().fenceValue <= completedFenceValue)
	{
		m_pendingResourceReleases.pop_front();
	}
}

void GraphicsDevice_DX12::Initialize(void* hWnd, const uint32_t width, const uint32_t height)
{
	m_width = width;
	m_height = height;
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));

	ReserveResources();

	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	HR_CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> hwAdapter;

	ComPtr<IDXGIFactory6> factory6;
	if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
	{
		for (
			UINT adapterIndex = 0;
			SUCCEEDED(factory6->EnumAdapterByGpuPreference(
				adapterIndex,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(&hwAdapter)));
				++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			hwAdapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(hwAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	if (hwAdapter.Get() == nullptr)
	{
		for (UINT adapterIndex = 0; SUCCEEDED(factory->EnumAdapters1(adapterIndex, &hwAdapter)); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			hwAdapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(hwAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	HR_CHECK(D3D12CreateDevice(hwAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	HR_CHECK(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FRAMECOUNT;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	HWND windowHandle = static_cast<HWND>(hWnd);
	HR_CHECK(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
	HR_CHECK(factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));

	HR_CHECK(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	{
		m_cbvSrvUavAllocator.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16384, true);
		m_rtvAllocator.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128, false);
		m_dsvAllocator.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128, false);
	}

	{
		for (UINT n = 0; n < FRAMECOUNT; n++)
		{
			HR_CHECK(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			auto rtvHandle = AllocateRTV();
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle.cpu);

			InternalTexture tex = {};
			tex.resource = m_renderTargets[n];
			tex.desc = { m_width, m_height, 1, 1, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget, false };
			tex.rtv = rtvHandle;

			m_backBufferHandles[n].id = static_cast<uint32_t>(m_textures.size());
			m_textures.push_back(tex);

			HR_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}

		HR_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_immediateAllocator)));
	}

	HR_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	HR_CHECK(m_commandList->Close());

	HR_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_immediateAllocator.Get(), nullptr, IID_PPV_ARGS(&m_immediateCommandList)));
	HR_CHECK(m_immediateCommandList->Close());
	
	m_commandContext.Init(this, m_commandList.Get());

	{
		HR_CHECK(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValues[m_frameIndex]++;

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			HR_CHECK(HRESULT_FROM_WIN32(GetLastError()));
		}

		WaitForGpu();
	}

	uploadHeapAllocator = std::make_unique<UploadHeapRingAllocator>(m_device.Get(), m_fence.Get());
	m_uploadQueue = std::make_unique<UploadQueue>(m_device.Get());

	m_profiler.Initialize(m_device.Get(), m_commandQueue.Get());
}

void GraphicsDevice_DX12::WaitForGpu()
{
	const UINT64 fenceToWaitFor = m_nextFenceValue++;
	HR_CHECK(m_commandQueue->Signal(m_fence.Get(), fenceToWaitFor));

	if (m_fence->GetCompletedValue() < fenceToWaitFor)
	{
		HR_CHECK(m_fence->SetEventOnCompletion(fenceToWaitFor, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	m_fenceValues[m_frameIndex] = fenceToWaitFor;
}

void GraphicsDevice_DX12::MoveToNextFrame()
{
	const UINT64 fenceToSignal = m_nextFenceValue++;
	HR_CHECK(m_commandQueue->Signal(m_fence.Get(), fenceToSignal));
	m_fenceValues[m_frameIndex] = fenceToSignal;

	uploadHeapAllocator->FinishFrame(fenceToSignal);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		HR_CHECK(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}
}

void GraphicsDevice_DX12::ExecuteImmediate(std::function<void(CommandContext&)> fn)
{
	UINT64 fv = m_uploadQueue->Flush();
	if (fv > 0)
		m_uploadQueue->InsertWaitOnQueue(m_commandQueue.Get(), fv);

	HR_CHECK(m_immediateAllocator->Reset());
	HR_CHECK(m_immediateCommandList->Reset(m_immediateAllocator.Get(), nullptr));
	auto originalMainList = m_commandList;
	m_commandList = m_immediateCommandList;
	m_commandContext.SetInternalCommandList(m_immediateCommandList.Get());
	m_commandContext.ResetState();

	ID3D12DescriptorHeap* heaps[] = { GetCbvSrvUavAllocator().GetHeap() };
	m_immediateCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

	if (!m_pendingTransitions.empty())
	{
		m_immediateCommandList->ResourceBarrier(
			(UINT)m_pendingTransitions.size(),
			m_pendingTransitions.data());
		m_pendingTransitions.clear();
	}

	fn(m_commandContext);

	HR_CHECK(m_immediateCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_immediateCommandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
	WaitForGpu();

	m_commandList = originalMainList;
	m_commandContext.SetInternalCommandList(m_commandList.Get());
}

CommandContext& GraphicsDevice_DX12::BeginFrame()
{
	uint64_t pendingFence = m_nextFenceValue;

	UINT64 fv = m_uploadQueue->Flush();
	if (fv > 0) m_uploadQueue->InsertWaitOnQueue(m_commandQueue.Get(), fv);

	const uint64_t completedFence = m_fence->GetCompletedValue();
	m_cbvSrvUavAllocator.FinishFrame(completedFence);
	m_rtvAllocator.FinishFrame(completedFence);
	m_dsvAllocator.FinishFrame(completedFence);

	m_cbvSrvUavAllocator.SetCurrentFence(pendingFence);
	m_rtvAllocator.SetCurrentFence(pendingFence);
	m_dsvAllocator.SetCurrentFence(pendingFence);

	HR_CHECK(m_commandAllocators[m_frameIndex]->Reset());
	HR_CHECK(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
	m_commandContext.SetInternalCommandList(m_commandList.Get());
	m_commandContext.ResetState();

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ID3D12DescriptorHeap* ppHeaps[] = { GetCbvSrvUavAllocator().GetHeap() };
	m_commandList->SetDescriptorHeaps(1, ppHeaps);

	if (!m_pendingTransitions.empty())
	{
		m_commandList->ResourceBarrier(
			static_cast<UINT>(m_pendingTransitions.size()),
			m_pendingTransitions.data());
		m_pendingTransitions.clear();
	}

	uploadHeapAllocator->ReleaseCompleted();
	ProcessCompletedResourceReleases(m_fence->GetCompletedValue());
	m_profiler.BeginFrame(m_frameIndex);

	return m_commandContext;
}

void GraphicsDevice_DX12::EndFrame()
{
	m_profiler.Resolve(m_commandList.Get());

	HR_CHECK(m_commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	HR_CHECK_DEVICE(m_swapChain->Present(1, 0), m_device.Get());

	MoveToNextFrame();
}

void GraphicsDevice_DX12::Shutdown()
{
	WaitForGpu();
	ProcessCompletedResourceReleases(UINT64_MAX);
	CloseHandle(m_fenceEvent);
}

GPUBufferHandle GraphicsDevice_DX12::CreateBuffer(const BufferDesc desc, const void* initialData)
{
	InternalBuffer internalBuffer;
	internalBuffer.desc = desc;

	ComPtr<ID3D12Resource> buffer;
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.size);

	if (desc.access == MemoryAccess::GpuOnly)
	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		HR_CHECK(m_device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));

		if (initialData)
			m_uploadQueue->UploadBuffer(buffer.Get(), 0, initialData, desc.size);
	}
	else
	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		HR_CHECK(m_device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));

		if (initialData)
		{
			UINT8* dst = nullptr;
			CD3DX12_RANGE readRange(0, 0);
			HR_CHECK(buffer->Map(0, &readRange, reinterpret_cast<void**>(&dst)));
			memcpy(dst, initialData, desc.size);
			buffer->Unmap(0, nullptr);
		}
	}

	if (HasFlag(desc.usage, BufferUsage::Structured))
	{
		internalBuffer.srv = m_cbvSrvUavAllocator.Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = desc.size / desc.stride;
		srvDesc.Buffer.StructureByteStride = desc.stride;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		m_device->CreateShaderResourceView(buffer.Get(), &srvDesc, internalBuffer.srv.cpu);
	}

	internalBuffer.resource = buffer;

	uint32_t id = m_buffers.size();
	m_buffers.push_back(internalBuffer);

	return GPUBufferHandle{ id };
}

void GraphicsDevice_DX12::UpdateBuffer(GPUBufferHandle h, const void* data, size_t size, size_t offset)
{
	auto& buf = m_buffers[h.id];
	m_uploadQueue->UploadBuffer(buf.resource.Get(), offset, data, size);
}

GPUTextureHandle GraphicsDevice_DX12::CreateTexture(const TextureInitDesc& init)
{
	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = init.desc.width;
	rd.Height = init.desc.height;
	rd.DepthOrArraySize = (UINT16)init.desc.arraySize;
	rd.MipLevels = (UINT16)init.desc.mipLevels;
	rd.Format = DX12Helpers::ToDXGIFormat(init.desc.format);
	rd.SampleDesc = { 1, 0 };
	rd.Flags = D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> resource;
	auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	HR_CHECK(m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)));

	const uint32_t subCount = init.desc.mipLevels * init.desc.arraySize;
	std::vector<D3D12_SUBRESOURCE_DATA> dxSubs(subCount);
	for (uint32_t i = 0; i < subCount; i++)
	{
		dxSubs[i].pData = init.subresources[i].data;
		dxSubs[i].RowPitch = init.subresources[i].rowPitch;
		dxSubs[i].SlicePitch = init.subresources[i].slicePitch;
	}

	m_uploadQueue->UploadTexture(resource.Get(), dxSubs.data(), (uint32_t)dxSubs.size());

	DescriptorHandle srvHandle = m_cbvSrvUavAllocator.Allocate();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = rd.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (init.desc.isCubemap)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = init.desc.mipLevels;
	}
	else if (init.desc.arraySize > 1)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MipLevels = init.desc.mipLevels;
		srvDesc.Texture2DArray.ArraySize = init.desc.arraySize;
	}
	else
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = init.desc.mipLevels;
	}
	m_device->CreateShaderResourceView(resource.Get(), &srvDesc, srvHandle.cpu);

	InternalTexture internal;
	internal.resource = resource;
	internal.srv = srvHandle;
	internal.desc = {static_cast<uint32_t>(rd.Width), static_cast<uint32_t>(rd.Height), rd.MipLevels, rd.DepthOrArraySize, init.desc.format, TextureUsage::ShaderResource, init.desc.isCubemap };

	uint32_t id = m_textures.size();
	m_textures.push_back(internal);

	return GPUTextureHandle{ id };
}

GPUTextureHandle GraphicsDevice_DX12::CreateRTTexture(const TextureDesc& desc)
{
	InternalTexture internalTexture;
	internalTexture.desc = desc;

	ComPtr<ID3D12Resource> texture;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DX12Helpers::ToDXGIFormat(desc.format);
	textureDesc.Width = std::max(desc.width, 1u);
	textureDesc.Height = std::max(desc.height, 1u);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DX12Helpers::ToDXGIFormat(desc.format);
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;

	HR_CHECK(m_device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&texture)));

	auto rtvHandle = AllocateRTV();
	m_device->CreateRenderTargetView(texture.Get(), nullptr, rtvHandle.cpu);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DX12Helpers::ToDXGIFormat(desc.format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	UINT srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	auto srvHandle = AllocateSRV();
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle.cpu);

	internalTexture.resource = texture;
	internalTexture.srv = srvHandle;
	internalTexture.rtv = rtvHandle;

	uint32_t id = m_textures.size();
	m_textures.push_back(internalTexture);

	return GPUTextureHandle{ id };
}

GPUTextureHandle GraphicsDevice_DX12::CreateDSTexture(const TextureDesc& desc)
{
	InternalTexture internalTexture;
	internalTexture.desc = desc;

	ComPtr<ID3D12Resource> texture;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	HR_CHECK(m_device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&texture)));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	auto dsvHandle = AllocateDSV();
	m_device->CreateDepthStencilView(texture.Get(), &dsvDesc, dsvHandle.cpu);


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;


	auto srvHandle = AllocateSRV();
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle.cpu);


	internalTexture.resource = texture;
	internalTexture.srv = srvHandle;
	internalTexture.dsv = dsvHandle;

	uint32_t id = m_textures.size();
	m_textures.push_back(internalTexture);

	return GPUTextureHandle{ id };
}

GPUTextureHandle GraphicsDevice_DX12::CreateUAVTexture(const TextureDesc& desc)
{
	InternalTexture internalTexture;
	internalTexture.desc = desc;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DX12Helpers::ToDXGIFormat(desc.format);
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> texture;
	HR_CHECK(m_device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&texture)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);


	auto srvHandle = AllocateSRV();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle.cpu);

	auto uavHandle = AllocateUAV();
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = textureDesc.Format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateUnorderedAccessView(texture.Get(), nullptr, &uavDesc, uavHandle.cpu);

	internalTexture.resource = texture;
	internalTexture.srv = srvHandle;
	internalTexture.uav = uavHandle;

	uint32_t id = static_cast<uint32_t>(m_textures.size());
	m_textures.push_back(internalTexture);
	return GPUTextureHandle{ id };
}

GPUTextureHandle GraphicsDevice_DX12::CreateDSCubemapTexture(const CubemapTextureDesc& desc)
{
	InternalTexture internalTexture;
	internalTexture.cubeDesc = desc;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = desc.mipLevels;
	textureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.Flags = DX12Helpers::ToResourceFlags(desc.usage);
	textureDesc.DepthOrArraySize = 6;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> texture;
	HR_CHECK(m_device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		DX12Helpers::ToResourceInitialStates(desc.usage),
		&clearValue,
		IID_PPV_ARGS(&texture)));

	for (int i = 0; i < 6; i++)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.ArraySize = 1;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;

		auto dsvHandle = AllocateDSV();
		m_device->CreateDepthStencilView(texture.Get(), &dsvDesc, dsvHandle.cpu);

		internalTexture.faceDsvs[i] = dsvHandle;
	}

	auto srvHandle = AllocateSRV();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = desc.mipLevels;
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle.cpu);

	internalTexture.resource = texture;
	internalTexture.srv = srvHandle;

	uint32_t id = static_cast<uint32_t>(m_textures.size());
	m_textures.push_back(internalTexture);
	return GPUTextureHandle{ id };
}

GPUTextureHandle GraphicsDevice_DX12::CreateUAVCubemapTexture(const CubemapTextureDesc& desc)
{
	InternalTexture internalTexture;
	internalTexture.cubeDesc = desc;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = desc.mipLevels;
	textureDesc.Format = DX12Helpers::ToDXGIFormat(desc.format);
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.Flags = DX12Helpers::ToResourceFlags(desc.usage);
	textureDesc.DepthOrArraySize = 6;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> texture;
	HR_CHECK(m_device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		DX12Helpers::ToResourceInitialStates(desc.usage),
		nullptr,
		IID_PPV_ARGS(&texture)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);


	auto srvHandle = AllocateSRV();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = desc.mipLevels;
	m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle.cpu);

	for (UINT mip = 0; mip < desc.mipLevels; mip++)
	{
		auto uavHandle = AllocateUAV();
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = textureDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize = 6;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.MipSlice = mip;

		m_device->CreateUnorderedAccessView(texture.Get(), nullptr, &uavDesc, uavHandle.cpu);
		internalTexture.uavMips.push_back(uavHandle);
	}
	internalTexture.resource = texture;
	internalTexture.srv = srvHandle;

	uint32_t id = static_cast<uint32_t>(m_textures.size());
	m_textures.push_back(internalTexture);
	return GPUTextureHandle{ id };
}

PipelineHandle GraphicsDevice_DX12::CreatePipeline(const PipelineDesc desc)
{
	InternalPipeline internalPipeline;
	ComPtr<ID3D12PipelineState> pso;
	ComPtr<ID3D12RootSignature> rs = BuildRootSignature(desc.rootSignatureDesc);

	const UINT inputElementCount = desc.vertexAttributes.size();
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
	inputElementDescs.resize(inputElementCount);
	for (UINT i = 0; i < inputElementCount; i++)
	{
		inputElementDescs[i].SemanticName = DX12Helpers::ToSemanticString(desc.vertexAttributes[i].semantic);
		inputElementDescs[i].SemanticIndex = 0;
		inputElementDescs[i].Format = DX12Helpers::ToDXGIFormat(desc.vertexAttributes[i].format);
		inputElementDescs[i].InputSlot = 0;
		inputElementDescs[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElementDescs[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		inputElementDescs[i].InstanceDataStepRate = 0;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputElementDescs.empty()
		? D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 }
		: D3D12_INPUT_LAYOUT_DESC{ inputElementDescs.data(),
									static_cast<UINT>(inputElementDescs.size()) };
	psoDesc.pRootSignature = rs.Get();
	psoDesc.VS = { desc.vs.data, desc.vs.size };

	if (desc.ps.data != nullptr)
		psoDesc.PS = { desc.ps.data, desc.ps.size };

	D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterDesc.CullMode = DX12Helpers::ToCullMode(desc.cullMode);
	psoDesc.RasterizerState = rasterDesc;
	
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = DX12Helpers::ToPrimitiveTopologyType(desc.topology);

	int numRT = 0;
	for (int i = 0; i < desc.rtvFormats.size(); i++)
	{
		if (desc.rtvFormats[i] != Format::UNKNOWN)
		{
			numRT++;
			psoDesc.RTVFormats[i] = DX12Helpers::ToDXGIFormat(desc.rtvFormats[i]);
		}
	}
	psoDesc.NumRenderTargets = numRT;

	if (desc.dsvFormat != Format::UNKNOWN)
	{
		psoDesc.DSVFormat = DX12Helpers::ToDXGIFormat(desc.dsvFormat);
	}

	psoDesc.DepthStencilState.DepthEnable = desc.depthEnable;
	psoDesc.DepthStencilState.DepthWriteMask = desc.depthWrite
		? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = DX12Helpers::ToComparisonFunc(desc.depthFunc);

	psoDesc.DepthStencilState.StencilEnable = FALSE;

	psoDesc.SampleDesc.Count = 1;
	HR_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

	internalPipeline.pso = pso;
	internalPipeline.rootSignature = rs;
	internalPipeline.topology = desc.topology;

	uint32_t id = m_pipelines.size();
	m_pipelines.push_back(internalPipeline);

	return PipelineHandle{ id };
}

PipelineHandle GraphicsDevice_DX12::CreateComputePipeline(const ComputePipelineDesc desc)
{
	InternalPipeline internalPipeline;
	ComPtr<ID3D12PipelineState> pso;

	ComPtr<ID3D12RootSignature> rs = BuildRootSignature(desc.rootSignatureDesc);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rs.Get();
	psoDesc.CS = { desc.cs.data, desc.cs.size };

	HR_CHECK(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

	internalPipeline.pso = pso;
	internalPipeline.rootSignature = rs;

	uint32_t id = m_pipelines.size();
	m_pipelines.push_back(internalPipeline);

	return PipelineHandle{ id };
}

ComPtr<ID3D12RootSignature> GraphicsDevice_DX12::BuildRootSignature(const RootSignatureDesc& desc)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	const int rootParamCount = desc.rootParamDescs.size();

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;
	ranges.reserve(rootParamCount);
	std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
	rootParameters.reserve(rootParamCount);

	for (int i = 0; i < rootParamCount; i++)
	{
		auto rootParamDesc = desc.rootParamDescs[i];

		if (rootParamDesc.type == RootParamType::RootCBV)
		{
			rootParameters.push_back({});
			rootParameters[i].InitAsConstantBufferView(
				rootParamDesc.baseRegister,
				0,
				D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
				DX12Helpers::ToVisibility(rootParamDesc.visibility));
		}
		else if (rootParamDesc.type == RootParamType::DescriptorTable)
		{
			ranges.push_back({});
			ranges.back().Init(
				DX12Helpers::ToRangeType(rootParamDesc.rangeType),
				rootParamDesc.numDescriptors,
				rootParamDesc.baseRegister,
				0,
				D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

			rootParameters.push_back({});
			rootParameters[i].InitAsDescriptorTable(
				1,
				&ranges.back(),
				DX12Helpers::ToVisibility(rootParamDesc.visibility));
		}
		else if (rootParamDesc.type == RootParamType::RootConstants)
		{
			rootParameters.push_back({});
			rootParameters[i].InitAsConstants(
				rootParamDesc.numDescriptors,
				rootParamDesc.baseRegister,
				0,
				DX12Helpers::ToVisibility(rootParamDesc.visibility));
		}
		else if (rootParamDesc.type == RootParamType::RootSRV)
		{
			rootParameters.push_back({});
			rootParameters[i].InitAsShaderResourceView(
				rootParamDesc.baseRegister,
				rootParamDesc.registerSpace,
				D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
				DX12Helpers::ToVisibility(rootParamDesc.visibility));
		}
	}


	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplerDescs;
	for (auto& samplerDesc : desc.staticSamplers)
	{
		staticSamplerDescs.push_back(DX12Helpers::ToStaticSampler(samplerDesc));
	}

	D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	if (desc.allowIA)
		flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	if (desc.cbvSrvUavHeapDirectlyIndexed)
		flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(
		rootParamCount,
		rootParameters.data(),
		staticSamplerDescs.size(),
		staticSamplerDescs.data(),
		flags);

	ComPtr<ID3D12RootSignature> rs;

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	HR_CHECK(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
	HR_CHECK(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs)));

	return rs;
}

DescriptorHandle GraphicsDevice_DX12::GetSRVHandle(GPUTextureHandle handle)
{
	return m_textures[handle.id].srv;
}

DescriptorHandle GraphicsDevice_DX12::GetSRVHandle(GPUBufferHandle handle)
{
	return m_buffers[handle.id].srv;
}

DescriptorHandle GraphicsDevice_DX12::GetUAVHandle(GPUTextureHandle handle, uint32_t mip)
{
	const auto& tex = m_textures[handle.id];

	if (!tex.uavMips.empty())
	{
		MOKO_ASSERT(mip < tex.uavMips.size() && "GetUAVHandle: mip out of range");
		return tex.uavMips[mip];
	}

	MOKO_ASSERT(tex.uav.IsValid() && "GetUAVHandle: texture has no UAV");
	MOKO_ASSERT(mip == 0 && "GetUAVHandle: non-mip UAV requested with mip > 0");
	return tex.uav;
}

void GraphicsDevice_DX12::DestroyBuffer(GPUBufferHandle handle)
{
	if (!handle.IsValid()) return;

	InternalBuffer& buf = m_buffers[handle.id];

	for (uint32_t i = 0; i < FRAMECOUNT; ++i)
	{
		if (buf.frameResources[i])
		{
			if (buf.mappedPointers[i])
			{
				buf.frameResources[i]->Unmap(0, nullptr);
				buf.mappedPointers[i] = nullptr;
			}

			EnqueueResourceRelease(std::move(buf.frameResources[i]), m_nextFenceValue);
			buf.frameResources[i].Reset();
		}
	}

	if (buf.resource)
	{
		EnqueueResourceRelease(std::move(buf.resource), m_nextFenceValue);
		buf.resource.Reset();
	}
}

void GraphicsDevice_DX12::DestroyTexture(GPUTextureHandle handle)
{
	if (!handle.IsValid()) return;

	InternalTexture* tex = &m_textures[handle.id];
	if (!tex) return;

	if (tex->srv.IsValid()) m_cbvSrvUavAllocator.FreeByCpuHandle(tex->srv.cpu);
	if (tex->rtv.IsValid()) m_rtvAllocator.FreeByCpuHandle(tex->rtv.cpu);
	if (tex->dsv.IsValid()) m_dsvAllocator.FreeByCpuHandle(tex->dsv.cpu);

	for (auto& uav : tex->uavMips)
	{
		if (uav.IsValid()) m_cbvSrvUavAllocator.FreeByCpuHandle(uav.cpu);
	}
	for (auto& dsv : tex->faceDsvs)
	{
		if (dsv.IsValid()) m_dsvAllocator.FreeByCpuHandle(dsv.cpu);
	}

	ComPtr<ID3D12Resource> resourceToRelease;
	resourceToRelease.Swap(tex->resource);
	EnqueueResourceRelease(std::move(resourceToRelease), m_nextFenceValue);
}

const ComPtr<ID3D12Resource> GraphicsDevice_DX12::GetTextureResource(GPUTextureHandle handle)
{
	return m_textures[handle.id].resource;
}

GPUTextureHandle* GraphicsDevice_DX12::GetCurrentBackBufferPtr()
{
	return &(m_backBufferHandles[m_frameIndex]);
}

GPUTextureHandle GraphicsDevice_DX12::GetCurrentBackBuffer()
{
	return m_backBufferHandles[m_frameIndex];
}

void GraphicsDevice_DX12::ResizeSwapChain(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) return;

	WaitForGpu();

	for (UINT i = 0; i < FRAMECOUNT; i++)
	{
		m_textures[m_backBufferHandles[i].id].resource.Reset();
		m_renderTargets[i].Reset();
	}

	HR_CHECK(m_swapChain->ResizeBuffers(
		FRAMECOUNT,
		width, height,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	for (UINT i = 0; i < FRAMECOUNT; i++)
	{
		HR_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));

		auto& tex = m_textures[m_backBufferHandles[i].id];
		tex.resource = m_renderTargets[i];
		tex.desc.width = width;
		tex.desc.height = height;

		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, tex.rtv.cpu);
	} 

	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));

	const UINT64 currentFence = m_fenceValues[m_frameIndex];
	for (UINT i = 0; i < FRAMECOUNT; i++)
		m_fenceValues[i] = currentFence;

	m_width = width;
	m_height = height;
}

uint32_t GraphicsDevice_DX12::GetWidth()
{
	return m_width;
}

uint32_t GraphicsDevice_DX12::GetHeight()
{
	return m_height;
}

float GraphicsDevice_DX12::GetTimestampMs(uint32_t passIndex)
{
	return m_profiler.GetTimestampMs(passIndex);
}
