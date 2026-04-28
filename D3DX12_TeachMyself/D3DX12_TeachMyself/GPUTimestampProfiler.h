#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <array>

enum PassID
{
    DepthPrePass = 0,
    GBufferPass = 1,
    GBufferAlphaPass = 2,
    DirectionalShadowPass = 3,
    PointShadowPass = 4,
    SSAOPass = 5,
    GTAOPass = 6,
    SSAOBilateralBlurHorizontalPass = 7,
    SSAOBilateralBlurVerticalPass = 8,
    GTAOBilateralBlurHorizontalPass = 9,
    GTAOBilateralBlurVerticalPass = 10,
    PBRLightingPass = 11,
    SkyboxPass = 12,
    PresentPass = 13,
    DebugLinePass = 14,
    Count
};

enum class ProfilerPassStatus
{
    Timed,
    Culled,
    Unused
};

struct ProfilerResult
{
    std::string name;
    float ms;
    ProfilerPassStatus status;
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
        std::fill(m_passBegan[m_frameIndex].begin(), m_passBegan[m_frameIndex].end(), false);
        std::fill(m_passEnded[m_frameIndex].begin(), m_passEnded[m_frameIndex].end(), false);
        ReadbackResults();
    }

    void BeginTimestamp(ID3D12GraphicsCommandList* cmd, uint32_t passIndex)
    {
        const uint32_t query = passIndex * 2;
        cmd->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query);
        m_activePassCount = std::max(m_activePassCount, passIndex + 1);
        m_passBegan[m_frameIndex][passIndex] = true;
    }

    void EndTimestamp(ID3D12GraphicsCommandList* cmd, uint32_t passIndex)
    {
        const uint32_t query = passIndex * 2 + 1;
        cmd->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query);
        m_passEnded[m_frameIndex][passIndex] = true;
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
        m_readFrameIndex = readFrame;
        uint64_t* data = nullptr;
        D3D12_RANGE range = { 0, sizeof(uint64_t) * QueryCount };
        m_readbackBuffers[readFrame]->Map(0, &range, reinterpret_cast<void**>(&data));
        memcpy(m_timestamps, data, sizeof(uint64_t) * QueryCount);
        m_readbackBuffers[readFrame]->Unmap(0, nullptr);
    }

    const float GetTimestampMs(uint32_t passIndex) const
    {
        uint64_t start = m_timestamps[passIndex * 2];
        uint64_t end = m_timestamps[passIndex * 2 + 1];
        if (end <= start || m_frequency == 0)
            return 0.0f;
        return float(double(end - start) * 1000.0 / double(m_frequency));
    }

    const std::vector<ProfilerResult> GetLastFrameResults() const
    {
        constexpr float alpha = 0.1f;

        std::vector<ProfilerResult> results;
        results.reserve((size_t)PassID::Count);

        auto smooth = [&](PassID id, float current) {
            float& s = m_smoothedMs[(size_t)id];
            s = s * (1.0f - alpha) + current * alpha;
            return s;
            };

        auto addResult = [&](PassID id, const char* name) {
            const uint32_t index = (uint32_t)id;
            const bool began = m_passBegan[m_readFrameIndex][index];
            const bool ended = m_passEnded[m_readFrameIndex][index];

            if (!began)
            {
                results.push_back({ name, 0.0f, ProfilerPassStatus::Unused });
                return;
            }

            uint64_t start = m_timestamps[index * 2];
            uint64_t end = m_timestamps[index * 2 + 1];
            if (!ended || end <= start || m_frequency == 0)
            {
                results.push_back({ name, 0.0f, ProfilerPassStatus::Culled });
                return;
            }

            results.push_back({ name, smooth(id, GetTimestampMs(index)), ProfilerPassStatus::Timed });
        };

        addResult(PassID::DepthPrePass, "DepthPrePass");
        addResult(PassID::GBufferPass, "GBufferPass");
        addResult(PassID::GBufferAlphaPass, "GBufferAlphaPass");
        addResult(PassID::DirectionalShadowPass, "DirectionalShadowPass");
        addResult(PassID::PointShadowPass, "PointShadowPass");
        addResult(PassID::SSAOPass, "SSAOPass");
        addResult(PassID::GTAOPass, "GTAOPass");
        addResult(PassID::SSAOBilateralBlurHorizontalPass, "SSAOBilateralBlurHorizontalPass");
        addResult(PassID::SSAOBilateralBlurVerticalPass, "SSAOBilateralBlurVerticalPass");
        addResult(PassID::GTAOBilateralBlurHorizontalPass, "GTAOBilateralBlurHorizontalPass");
        addResult(PassID::GTAOBilateralBlurVerticalPass, "GTAOBilateralBlurVerticalPass");
        addResult(PassID::PBRLightingPass, "PBRLightingPass");
        addResult(PassID::SkyboxPass, "SkyboxPass");
        addResult(PassID::PresentPass, "PresentPass");
        addResult(PassID::DebugLinePass, "DebugLinePass");

        return results;
    }

private:
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_queue = nullptr;

    uint64_t m_frequency = 0;
    uint32_t m_frameIndex = 0;
    uint32_t m_readFrameIndex = 0;

    uint32_t m_activePassCount = 0;

    uint64_t m_timestamps[QueryCount] = {};
    mutable std::array<float, (size_t)PassID::Count> m_smoothedMs{};
    std::array<std::array<bool, MaxPasses>, BufferedFrames> m_passBegan{};
    std::array<std::array<bool, MaxPasses>, BufferedFrames> m_passEnded{};

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffers[BufferedFrames];
};
