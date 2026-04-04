#pragma once
#include "RHITypes.h"
#include <d3d12.h>


namespace DX12Helpers
{
    DXGI_FORMAT                 ToDXGIFormat(Format format);
    Format                      ToRHIFormat(DXGI_FORMAT format);
    D3D12_COMPARISON_FUNC       ToComparisonFunc(ComparisonFunc func);
    D3D12_CULL_MODE             ToCullMode(CullMode mode);
    D3D12_DESCRIPTOR_RANGE_TYPE ToRangeType(RangeType type);
    D3D12_SHADER_VISIBILITY     ToVisibility(ShaderVisibility vis);
    D3D12_STATIC_SAMPLER_DESC   ToStaticSampler(const StaticSamplerDesc& desc);
    UINT                        GetBytesPerPixel(Format format);
    const char*                 ToSemanticString(Semantic semantic);
}