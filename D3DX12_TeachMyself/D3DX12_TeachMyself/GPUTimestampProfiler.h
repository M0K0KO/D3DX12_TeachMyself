#pragma once
#include "stdafx.h"

#undef max

enum PassID
{
    DepthPrePass = 0,
    GBufferPass = 1,
    GBufferAlphaPass = 2,
    DirectionalShadowPass = 3,
    PointShadowPass = 4,
    PBRLightingPass = 5,
    SkyboxPass = 6
};


class GPUTimestampProfiler
{
public:
    static constexpr uint32_t MaxPasses = 32;
    static constexpr uint32_t QueryCount = MaxPasses * 2;
    static constexpr uint32_t BufferedFrames = 3;

    void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue)
    {
        m_device = device;
        m_queue = queue;

        queue->GetTimestampFrequency(&m_frequency);

        D3D12_QUERY_HEAP_DESC heapDesc = {};
        heapDesc.Count = QueryCount;
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

        device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));

        const UINT64 bufferSize = sizeof(uint64_t) * QueryCount;

        for (uint32_t i = 0; i < BufferedFrames; ++i)
        {
            D3D12_HEAP_PROPERTIES props = {};
            props.Type = D3D12_HEAP_TYPE_READBACK;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = bufferSize;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_readbackBuffers[i]));
        }
    }
    
    void BeginFrame(uint32_t frameIndex)
    {
        m_frameIndex = frameIndex % BufferedFrames;
        ReadbackResults();
    }

    void BeginTimestamp(ID3D12GraphicsCommandList* cmd, uint32_t passIndex)
    {
        const uint32_t query = passIndex * 2;
        cmd->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query);
        m_activePassCount = std::max(m_activePassCount, passIndex + 1);
    }

    void EndTimestamp(ID3D12GraphicsCommandList* cmd, uint32_t passIndex)
    {
        const uint32_t query = passIndex * 2 + 1;
        cmd->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query);
    }

    void Resolve(ID3D12GraphicsCommandList* cmd)
    {
        cmd->ResolveQueryData(
            m_queryHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            m_activePassCount * 2,
            m_readbackBuffers[m_frameIndex].Get(),
            0);

        m_activePassCount = 0;
    }

    void ReadbackResults()
    {
        const uint32_t readFrame = (m_frameIndex + BufferedFrames - 1) % BufferedFrames;
        uint64_t* data = nullptr;
        D3D12_RANGE range = { 0, sizeof(uint64_t) * QueryCount };
        m_readbackBuffers[readFrame]->Map(0, &range, reinterpret_cast<void**>(&data));
        memcpy(m_timestamps, data, sizeof(uint64_t) * QueryCount);
        m_readbackBuffers[readFrame]->Unmap(0, nullptr);
    }

    float GetTimestampMs(uint32_t passIndex)
    {
        uint64_t start = m_timestamps[passIndex * 2];
        uint64_t end = m_timestamps[passIndex * 2 + 1];
        if (end <= start || m_frequency == 0)
            return 0.0f;
        return float(double(end - start) * 1000.0 / double(m_frequency));
    }

private:
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_queue = nullptr;

    uint64_t m_frequency = 0;
    uint32_t m_frameIndex = 0;

    uint32_t m_activePassCount = 0;

    uint64_t m_timestamps[QueryCount] = {};

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffers[BufferedFrames];
};
