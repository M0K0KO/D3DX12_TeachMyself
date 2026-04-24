#include "stdafx.h"
#include "DX12Helpers.h"

namespace DX12Helpers
{
	UINT Align256(UINT size)
	{
		return (size + 255) & ~255;
	}

	const char* ToSemanticString(Semantic semantic)
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
		default:
			return "ERROR";
		}
	}

	DXGI_FORMAT ToDXGIFormat(Format format)
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

	Format ToRHIFormat(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R8_UNORM:             return Format::R8_UNORM;
		case DXGI_FORMAT_R8G8B8A8_UNORM:       return Format::R8G8B8A8_UNORM;
		case DXGI_FORMAT_R16G16_FLOAT:         return Format::R16G16_FLOAT;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:   return Format::R16G16B16A16_FLOAT;
		case DXGI_FORMAT_R16G16B16A16_SNORM:   return Format::R16G16B16A16_SNORM;
		case DXGI_FORMAT_R32_FLOAT:            return Format::R32_FLOAT;
		case DXGI_FORMAT_R32G32_FLOAT:         return Format::R32G32_FLOAT;
		case DXGI_FORMAT_R32G32B32_FLOAT:      return Format::R32G32B32_FLOAT;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:   return Format::R32G32B32A32_FLOAT;
		case DXGI_FORMAT_R16_UINT:             return Format::R16_UINT;
		case DXGI_FORMAT_R32_UINT:             return Format::R32_UINT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:    return Format::D24_UNORM_S8_UINT;
		case DXGI_FORMAT_D32_FLOAT:            return Format::D32_FLOAT;
		case DXGI_FORMAT_R32_TYPELESS:         return Format::R32_TYPELESS;
		default:                               return Format::UNKNOWN;
		}
	}

	D3D12_COMPARISON_FUNC ToComparisonFunc(ComparisonFunc func)
	{
		switch (func)
		{
		case ComparisonFunc::Less:         return D3D12_COMPARISON_FUNC_LESS;
		case ComparisonFunc::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case ComparisonFunc::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
		default:                           return D3D12_COMPARISON_FUNC_NEVER;
		}
	}

	D3D12_CULL_MODE ToCullMode(CullMode mode)
	{
		switch (mode)
		{
		case CullMode::None:  return D3D12_CULL_MODE_NONE;
		case CullMode::Front: return D3D12_CULL_MODE_FRONT;
		case CullMode::Back:  return D3D12_CULL_MODE_BACK;
		default:              return D3D12_CULL_MODE_BACK;
		}
	}

	D3D12_DESCRIPTOR_RANGE_TYPE ToRangeType(RangeType type)
	{
		switch (type)
		{
		case RangeType::SRV:		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case RangeType::UAV:		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case RangeType::CBV:		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case RangeType::Sampler:	return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		default:					return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
	}

	D3D12_SHADER_VISIBILITY ToVisibility(ShaderVisibility visibility)
	{
		switch (visibility)
		{
		case ShaderVisibility::All: return D3D12_SHADER_VISIBILITY_ALL;
		case ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
		case ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
		default:  return D3D12_SHADER_VISIBILITY_ALL;
		}
	}

	D3D12_STATIC_SAMPLER_DESC ToStaticSampler(const StaticSamplerDesc& desc)
	{
		D3D12_STATIC_SAMPLER_DESC out = {};
		switch (desc.filter)
		{
		case SamplerFilter::Point:
			out.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; break;
		case SamplerFilter::Bilinear:
			out.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; break;
		case SamplerFilter::Anisotropic:
			out.Filter = D3D12_FILTER_ANISOTROPIC; break;
		case SamplerFilter::Comparison:
			out.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		}

		auto mode = [](SamplerAddressMode m) {
			switch (m)
			{
			case SamplerAddressMode::Wrap:   return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			case SamplerAddressMode::Clamp:  return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			case SamplerAddressMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			}
			};

		out.AddressU = mode(desc.addressMode);
		out.AddressV = mode(desc.addressMode);
		out.AddressW = mode(desc.addressMode);
		out.MaxAnisotropy = (desc.filter == SamplerFilter::Anisotropic) ? 16 : 1;
		out.ComparisonFunc = (desc.filter == SamplerFilter::Comparison) ? D3D12_COMPARISON_FUNC_LESS_EQUAL : D3D12_COMPARISON_FUNC_NEVER;
		out.MaxLOD = D3D12_FLOAT32_MAX;
		out.ShaderRegister = desc.shaderRegister;
		out.ShaderVisibility = ToVisibility(desc.visibility);
		return out;
	}

	UINT GetBytesPerPixel(Format format)
	{
		switch (format)
		{
		case Format::R8G8B8A8_UNORM:      return 4;
		case Format::R16G16B16A16_FLOAT:   return 8;
		case Format::R32G32B32A32_FLOAT:   return 16;
		default: return 4;
		}
	}

	D3D12_RESOURCE_FLAGS ToResourceFlags(TextureUsage usage)
	{
		switch (usage)
		{
		case TextureUsage::ShaderResource:		return D3D12_RESOURCE_FLAG_NONE;
		case TextureUsage::DepthStencil:		return D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		case TextureUsage::RenderTarget:		return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		case TextureUsage::UnorderedAccess:		return D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		default:								return D3D12_RESOURCE_FLAG_NONE;
		}
	}

	D3D12_RESOURCE_STATES ToResourceInitialStates(TextureUsage usage)
	{
		switch (usage)
		{
		case TextureUsage::ShaderResource:		return D3D12_RESOURCE_STATE_COMMON;
		case TextureUsage::DepthStencil:		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case TextureUsage::RenderTarget:		return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case TextureUsage::UnorderedAccess:		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		default:								return D3D12_RESOURCE_STATE_COMMON;
		}
	}
}