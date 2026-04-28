#include "stdafx.h"
#include "GraphicsDevice.h"
#include "MeshManager.h"

MeshManager::MeshManager(GraphicsDevice* device)
	: m_device(device)
{}

MeshManager::~MeshManager()
{
    m_pool.ForEach([this](MeshHandle, MeshAsset& m) {
        m_device->DestroyBuffer(m.vb);
        m_device->DestroyBuffer(m.ib);
        });
}

MeshHandle MeshManager::Create(const CreateDesc& desc)
{
    if (!desc.vertices || desc.vertexCount == 0 ||
        !desc.indices || desc.indexCount == 0)
    {
        MOKOLOG_ERROR("MeshManager::Create invalid input");
        return {};
    }

    BufferDesc vbDesc{
        .size = desc.vertexCount * desc.vertexStride,
        .stride = desc.vertexStride,
        .usage = BufferUsage::Vertex | BufferUsage::Structured,
        .access = MemoryAccess::GpuOnly,
    };
    GPUBufferHandle vb = m_device->CreateBuffer(vbDesc, desc.vertices);
    if (!vb.IsValid())
    {
        MOKOLOG_ERROR("MeshManager::Create VB creation failed");
        return {};
    }

    // IB
    BufferDesc ibDesc{
        .size = desc.indexCount * (uint32_t)sizeof(uint32_t),
        .stride = sizeof(uint32_t),
        .usage = BufferUsage::Index,
        .access = MemoryAccess::GpuOnly,
    };
    GPUBufferHandle ib = m_device->CreateBuffer(ibDesc, desc.indices);
    if (!ib.IsValid())
    {
        m_device->DestroyBuffer(vb);
        MOKOLOG_ERROR("MeshManager::Create IB creation failed");
        return {};
    }

    MeshAsset mesh{
        .vb = vb,
        .ib = ib,
        .vertexCount = desc.vertexCount,
        .indexCount = desc.indexCount,
        .submeshes = desc.submeshes,
        .bounds = desc.bounds,

        .vbSRVIndex = m_device->GetSRVHandle(vb).index,
    };
    return m_pool.Create(std::move(mesh));
}

void MeshManager::Destroy(MeshHandle h)
{
    if (MeshAsset* mesh = m_pool.Get(h))
    {
        m_device->DestroyBuffer(mesh->vb);
        m_device->DestroyBuffer(mesh->ib);
        m_pool.Destroy(h);
    }
}
