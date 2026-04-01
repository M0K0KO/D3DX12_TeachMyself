#include "stdafx.h"
#include "HRException.h"
#include "GraphicsDevice_DX12.h"
#include "CommandContext_DX12.h"
#include "Renderer.h"

void GraphicsDevice_DX12::Initialize(void* hWnd, const uint32_t width, const uint32_t height)
{
	m_width = width;
	m_height = height;
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));

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
	swapChainDesc.BufferCount = FrameCount;
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
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 100;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		HR_CHECK(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
		cbvSrvHeapDesc.NumDescriptors = 100;
		cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		HR_CHECK(m_device->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&m_cbvSrvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		HR_CHECK(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < FrameCount; n++)
		{
			HR_CHECK(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);

			InternalTexture tex = {};
			tex.resource = m_renderTargets[n];
			tex.desc = { m_width, m_height, Format::R8G8B8A8_UNORM, TextureUsage::RenderTarget };
			tex.rtvHandle = rtvHandle;

			m_backBufferHandles[n].id = static_cast<uint32_t>(m_textures.size());
			m_textures.push_back(tex);

			rtvHandle.Offset(1, m_rtvDescriptorSize);
			HR_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}
		m_rtvHeapNextSlot = FrameCount;
	}

	HR_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	HR_CHECK(m_commandList->Close());
	
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
}

void GraphicsDevice_DX12::WaitForGpu()
{
	HR_CHECK(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));
	HR_CHECK(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	m_fenceValues[m_frameIndex]++;
}

void GraphicsDevice_DX12::MoveToNextFrame()
{
	UINT64 currentFenceValue = m_fenceValues[m_frameIndex];

	HR_CHECK(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		HR_CHECK(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

CommandContext& GraphicsDevice_DX12::BeginFrame()
{
	HR_CHECK(m_commandAllocators[m_frameIndex]->Reset());
	HR_CHECK(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
	m_commandList->SetDescriptorHeaps(1, ppHeaps);

	return m_commandContext;
}

void GraphicsDevice_DX12::EndFrame()
{
	HR_CHECK(m_commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	HR_CHECK_DEVICE(m_swapChain->Present(1, 0), m_device.Get());

	MoveToNextFrame();
}

void GraphicsDevice_DX12::Shutdown()
{
	WaitForGpu();
	CloseHandle(m_fenceEvent);
}

BufferHandle GraphicsDevice_DX12::CreateBuffer(const BufferDesc desc, const void* initialData)
{
	if (desc.usage == BufferUsage::Constant)
	{
		InternalBuffer internalBuffer;
		internalBuffer.desc = desc;

		UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		internalBuffer.heapSlot = m_cbvSrvHeapNextSlot;
		m_cbvSrvHeapNextSlot += FrameCount;

		for (UINT n = 0; n < FrameCount; n++)
		{
			ComPtr<ID3D12Resource> constantBuffer;
			UINT8* bufferDataBegin;

			UINT alignedSize = Align256(desc.size);

			auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

			HR_CHECK(m_device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&constantBuffer)));

			CD3DX12_RANGE readRange(0, 0);
			HR_CHECK(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&bufferDataBegin)));

			internalBuffer.frameResources[n] = constantBuffer;
			internalBuffer.mappedPointers[n] = bufferDataBegin;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = alignedSize;

			CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), internalBuffer.heapSlot + n, descriptorSize);
			m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);
		}

		uint32_t id = m_buffers.size();
		m_buffers.push_back(internalBuffer);

		return BufferHandle{ id };
	}
	else
	{
		InternalBuffer internalBuffer;
		internalBuffer.desc = desc;

		ComPtr<ID3D12Resource> buffer;
		UINT8* bufferDataBegin = nullptr;

		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.size);

		HR_CHECK(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer)));

		internalBuffer.resource = buffer;

		if (initialData)
		{
			CD3DX12_RANGE readRange(0, 0);
			HR_CHECK(buffer->Map(0, &readRange, reinterpret_cast<void**>(&bufferDataBegin)));
			memcpy(bufferDataBegin, initialData, desc.size);
			buffer->Unmap(0, nullptr);
		}

		uint32_t id = m_buffers.size();
		m_buffers.push_back(internalBuffer);

		return BufferHandle{ id };
	}
}

TextureHandle GraphicsDevice_DX12::CreateTexture(const TextureDesc desc, const void* initialData)
{
	InternalTexture internalTexture;
	internalTexture.desc = desc;

	ComPtr<ID3D12Resource> texture;

	if (desc.usage == TextureUsage::RenderTarget)
	{
		// TO DO : PHASE 4
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = GetDXGIFormat(desc.format);
		textureDesc.Width = desc.width;
		textureDesc.Height = desc.height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		HR_CHECK(m_device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			nullptr,
			IID_PPV_ARGS(&texture)));

		UINT rtvHeapSlot = m_rtvHeapNextSlot++;
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
			rtvHeapSlot,
			m_rtvDescriptorSize);
		m_device->CreateRenderTargetView(texture.Get(), nullptr, rtvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = GetDXGIFormat(desc.format);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		UINT srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		UINT srvHeapSlot = m_cbvSrvHeapNextSlot++;
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
			m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
			srvHeapSlot, 
			srvDescriptorSize);
		m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle);

		internalTexture.resource = texture;
		internalTexture.srvHeapSlot = srvHeapSlot;
		internalTexture.rtvHandle = rtvHandle;

		uint32_t id = m_textures.size();
		m_textures.push_back(internalTexture);

		return TextureHandle{ id };
	}
	else if (desc.usage == TextureUsage::ShaderResource)
	{
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = desc.width;
		textureDesc.Height = desc.height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		HR_CHECK(m_device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

		ComPtr<ID3D12Resource> textureUploadHeap;

		auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

		HR_CHECK(m_device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap)));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = initialData;
		textureData.RowPitch = desc.width * 4U;
		textureData.SlicePitch = textureData.RowPitch * desc.height;

		HR_CHECK(m_commandAllocators[m_frameIndex]->Reset());
		HR_CHECK(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

		UpdateSubresources(m_commandList.Get(), texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		
		auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		
		m_commandList->ResourceBarrier(1, &resourceBarrier);
	
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		UINT slot = m_cbvSrvHeapNextSlot++;
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), slot, descriptorSize);
		m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle);
		
		internalTexture.resource = texture;
		internalTexture.srvHeapSlot = slot;

		HR_CHECK(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		WaitForGpu();

		uint32_t id = m_textures.size();
		m_textures.push_back(internalTexture);

		return TextureHandle{ id };
	}
	else if (desc.usage == TextureUsage::DepthStencil)
	{
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

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
			m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
			0, m_dsvDescriptorSize);
		m_device->CreateDepthStencilView(texture.Get(), &dsvDesc, dsvHandle);


		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		UINT srvHeapSlot = m_cbvSrvHeapNextSlot++;
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
			m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
			srvHeapSlot, descriptorSize);
		m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle);


		internalTexture.resource = texture;
		// TO DO : allocator 패턴을 이용할 것
		internalTexture.srvHeapSlot = srvHeapSlot;
		internalTexture.dsvHandle = dsvHandle;

		uint32_t id = m_textures.size();
		m_textures.push_back(internalTexture);

		return TextureHandle{ id };
	}

	return TextureHandle();
}

PipelineHandle GraphicsDevice_DX12::CreatePipeline(const PipelineDesc desc)
{
	InternalPipeline internalPipeline;
	ComPtr<ID3D12PipelineState> pso;
	ComPtr<ID3D12RootSignature> rs;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	const int rootParamCount = desc.rootSignatureDesc.rootParamDescs.size();

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;
	ranges.reserve(rootParamCount);
	std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
	rootParameters.reserve(rootParamCount);

	for (int i = 0; i < rootParamCount; i++)
	{
		auto rootParamDesc = desc.rootSignatureDesc.rootParamDescs[i];

		ranges.push_back({});
		ranges[i].Init(
			GetDX12DescriptorRangeType(rootParamDesc.rangeType),
			rootParamDesc.numDescriptors,
			rootParamDesc.baseRegister,
			0, 
			D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		
		rootParameters.push_back({});
		rootParameters[i].InitAsDescriptorTable(
			1,
			&ranges[i], 
			GetDX12ShaderVisibility(rootParamDesc.visibility));
	}

	// TODO : SAMPLER ABSTRACTION
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 2;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(rootParamCount, rootParameters.data(), 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	HR_CHECK(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
	HR_CHECK(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs)));



	const UINT inputElementCount = desc.vertexAttributes.size();
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
	inputElementDescs.resize(inputElementCount);
	for (UINT i = 0; i < inputElementCount; i++)
	{
		inputElementDescs[i].SemanticName = GetSemanticString(desc.vertexAttributes[i].semantic);
		inputElementDescs[i].SemanticIndex = 0;
		inputElementDescs[i].Format = GetDXGIFormat(desc.vertexAttributes[i].format);
		inputElementDescs[i].InputSlot = 0;
		inputElementDescs[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElementDescs[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		inputElementDescs[i].InstanceDataStepRate = 0;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs.data(), inputElementCount};
	psoDesc.pRootSignature = rs.Get();
	psoDesc.VS = { desc.vs.data, desc.vs.size };

	if (desc.ps.data != nullptr)
		psoDesc.PS = { desc.ps.data, desc.ps.size };

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	int numRT = 0;
	for (int i = 0; i < desc.rtvFormats.size(); i++)
	{
		if (desc.rtvFormats[i] != Format::UNKNOWN)
		{
			numRT++;
			psoDesc.RTVFormats[i] = GetDXGIFormat(desc.rtvFormats[i]);
		}
	}
	psoDesc.NumRenderTargets = numRT;

	if (desc.dsvFormat != Format::UNKNOWN)
	{
		psoDesc.DSVFormat = GetDXGIFormat(desc.dsvFormat);
	}

	psoDesc.DepthStencilState.DepthEnable = desc.depthEnable;
	psoDesc.DepthStencilState.DepthWriteMask = desc.depthWrite
		? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = GetDX12ComparisonFunc(desc.depthFunc);

	psoDesc.DepthStencilState.StencilEnable = FALSE;

	psoDesc.SampleDesc.Count = 1;
	HR_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

	internalPipeline.pso = pso;
	internalPipeline.rootSignature = rs;

	uint32_t id = m_pipelines.size();
	m_pipelines.push_back(internalPipeline);

	return PipelineHandle{ id };
}

inline UINT GraphicsDevice_DX12::Align256(UINT size)
{
	return (size + 255) & ~255;
}

inline const char* GraphicsDevice_DX12::GetSemanticString(Semantic semantic)
{
	switch (semantic)
	{
	case Semantic::POSITION:
		return "POSITION";
	case Semantic::NORMAL:
		return "NORMAL";
	case Semantic::TANGENT:
		return "TANGENT";
	case Semantic::COLOR:
		return "COLOR";
	case Semantic::TEXCOORD:
		return "TEXCOORD";
	}
}

inline DXGI_FORMAT GraphicsDevice_DX12::GetDXGIFormat(Format format)
{
	switch (format)
	{
	case Format::R8_UNORM:             return DXGI_FORMAT_R8_UNORM;
	case Format::R8G8B8A8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Format::R16G16_FLOAT:         return DXGI_FORMAT_R16G16_FLOAT;
	case Format::R16G16B16A16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case Format::R16G16B16A16_SNORM:   return DXGI_FORMAT_R16G16B16A16_SNORM;
	case Format::R32_FLOAT:            return DXGI_FORMAT_R32_FLOAT;
	case Format::R32G32_FLOAT:         return DXGI_FORMAT_R32G32_FLOAT;
	case Format::R32G32B32_FLOAT:      return DXGI_FORMAT_R32G32B32_FLOAT;
	case Format::R32G32B32A32_FLOAT:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case Format::R16_UINT:             return DXGI_FORMAT_R16_UINT;
	case Format::R32_UINT:             return DXGI_FORMAT_R32_UINT;
	case Format::D24_UNORM_S8_UINT:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case Format::D32_FLOAT:            return DXGI_FORMAT_D32_FLOAT;
	case Format::R32_TYPELESS:		   return DXGI_FORMAT_R32_TYPELESS;
	default:                           return DXGI_FORMAT_UNKNOWN;
	}
}

inline D3D12_COMPARISON_FUNC GraphicsDevice_DX12::GetDX12ComparisonFunc(ComparisonFunc func)
{
	switch (func)
	{
	case ComparisonFunc::Less:         return D3D12_COMPARISON_FUNC_LESS;
	case ComparisonFunc::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case ComparisonFunc::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
	default:                           return D3D12_COMPARISON_FUNC_NEVER;
	}
}

void GraphicsDevice_DX12::UpdateBuffer(const BufferHandle handle, const void* data, const uint32_t size)
{
	auto mappedPointer = m_buffers[handle.id].mappedPointers[m_frameIndex];
	memcpy(mappedPointer, data, size);
}

const ComPtr<ID3D12Resource> GraphicsDevice_DX12::GetTextureResource(TextureHandle handle)
{
	return m_textures[handle.id].resource;
}

inline D3D12_DESCRIPTOR_RANGE_TYPE GraphicsDevice_DX12::GetDX12DescriptorRangeType(DescriptorRangeType type)
{
	switch (type)
	{
	case DescriptorRangeType::SRV:		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case DescriptorRangeType::UAV:		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	case DescriptorRangeType::CBV:		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	case DescriptorRangeType::Sampler:  return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;;
	}
}

inline D3D12_SHADER_VISIBILITY GraphicsDevice_DX12::GetDX12ShaderVisibility(ShaderVisibility visibility)
{
	switch (visibility)
	{
	case ShaderVisibility::All: return D3D12_SHADER_VISIBILITY_ALL;
	case ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
	case ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
	}
}

TextureHandle* GraphicsDevice_DX12::GetCurrentBackBufferPtr()
{
	return &(m_backBufferHandles[m_frameIndex]);
}

TextureHandle GraphicsDevice_DX12::GetCurrentBackBuffer()
{
	return m_backBufferHandles[m_frameIndex];
}

int GraphicsDevice_DX12::GetWidth()
{
	return m_width;
}

int GraphicsDevice_DX12::GetHeight()
{
	return m_height;
}
