#include "TextureLoader.h"
#include "HRException.h"
#include "DirectXTex/DirectXTex.h"

TextureLoader::TextureLoader(ID3D12Device* device)
    :
    m_device(device)
{

}

DDSTextureHandle TextureLoader::LoadDDS(
    ID3D12GraphicsCommandList* cmdList,
    const std::wstring& path)
{
    DirectX::ScratchImage image;
    DirectX::TexMetadata meta;
    HR_CHECK(DirectX::LoadFromDDSFile(
        path.c_str(),
        DirectX::DDS_FLAGS_NONE,
        &meta, image));

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = meta.width;
    texDesc.Height = (UINT)meta.height;
    texDesc.DepthOrArraySize = (UINT16)meta.arraySize;
    texDesc.MipLevels = (UINT16)meta.mipLevels;
    texDesc.Format = meta.format;
    texDesc.SampleDesc = { 1, 0 };
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ComPtr<ID3D12Resource> texResource;
    auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HR_CHECK(m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texResource)));

    const UINT subresourceCount = (UINT)(meta.mipLevels * meta.arraySize);
    const UINT64 uploadSize = GetRequiredIntermediateSize(
        texResource.Get(), 0, subresourceCount);

    ComPtr<ID3D12Resource> uploadBuffer;
    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    HR_CHECK(m_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)));

    std::vector<D3D12_SUBRESOURCE_DATA> subresources(subresourceCount);
    for (UINT i = 0; i < subresourceCount; i++)
    {
        const DirectX::Image* img = image.GetImage(
            i % meta.mipLevels,
            i / meta.mipLevels, 0);

        subresources[i].pData = img->pixels;
        subresources[i].RowPitch = img->rowPitch;
        subresources[i].SlicePitch = img->slicePitch;
    }

    UpdateSubresources(cmdList,
        texResource.Get(),
        uploadBuffer.Get(),
        0, 0, subresourceCount,
        subresources.data());

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier);

    {
        std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
        m_pendingUploads.push_back(uploadBuffer);
    }

    DDSTextureHandle handle;
    handle.resource = texResource;
    handle.format = meta.format;
    handle.width = (UINT)meta.width;
    handle.height = (UINT)meta.height;
    handle.mipLevels = (UINT)meta.mipLevels;

    return handle;
}

void TextureLoader::CleanupUploads()
{
    std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
    m_pendingUploads.clear();
}