#include "UtilityDx12.h"
#include "assert.h"
#include "Utility/Utility.h"

HRESULT gal::UtilityDx12::checkResult(HRESULT result, const char *errorMsg, bool exitOnError)
{
	if (SUCCEEDED(result))
	{
		return result;
	}

	constexpr size_t maxErrorMsgLength = 256;
	constexpr size_t maxResultLength = 128;
	constexpr size_t msgBufferLength = maxErrorMsgLength + maxResultLength;

	char msgBuffer[msgBufferLength];
	msgBuffer[0] = '\0';

	if (errorMsg)
	{
		snprintf(msgBuffer, maxErrorMsgLength, errorMsg);
	}

	bool error = false;

	switch (result)
	{
	case S_OK:
		break;
	case S_FALSE:
		break;
	case E_UNEXPECTED:
		strcat_s(msgBuffer, " E_UNEXPECTED\n");
		error = true;
		break;
	case E_NOTIMPL:
		strcat_s(msgBuffer, " E_NOTIMPL\n");
		error = true;
		break;
	case E_OUTOFMEMORY:
		strcat_s(msgBuffer, " E_OUTOFMEMORY\n");
		error = true;
		break;
	case E_INVALIDARG:
		strcat_s(msgBuffer, " E_INVALIDARG\n");
		error = true;
		break;
	case E_NOINTERFACE:
		strcat_s(msgBuffer, " E_NOINTERFACE\n");
		error = true;
		break;
	case E_POINTER:
		strcat_s(msgBuffer, " E_POINTER\n");
		error = true;
		break;
	case E_HANDLE:
		strcat_s(msgBuffer, " E_HANDLE\n");
		error = true;
		break;
	case E_ABORT:
		strcat_s(msgBuffer, " E_ABORT\n");
		error = true;
		break;
	case E_FAIL:
		strcat_s(msgBuffer, " E_FAIL\n");
		error = true;
		break;
	case E_ACCESSDENIED:
		strcat_s(msgBuffer, " E_ACCESSDENIED\n");
		error = true;
		break;
	case E_PENDING:
		strcat_s(msgBuffer, " E_PENDING\n");
		error = true;
		break;
	case E_BOUNDS:
		strcat_s(msgBuffer, " E_BOUNDS\n");
		error = true;
		break;
	case E_CHANGED_STATE:
		strcat_s(msgBuffer, " E_CHANGED_STATE\n");
		error = true;
		break;
	case E_ILLEGAL_STATE_CHANGE:
		strcat_s(msgBuffer, " E_ILLEGAL_STATE_CHANGE\n");
		error = true;
		break;
	case E_ILLEGAL_METHOD_CALL:
		strcat_s(msgBuffer, " E_ILLEGAL_METHOD_CALL\n");
		error = true;
		break;
	case DXGI_ERROR_WAS_STILL_DRAWING:
		strcat_s(msgBuffer, " DXGI_ERROR_WAS_STILL_DRAWING\n");
		error = true;
		break;
	case DXGI_ERROR_INVALID_CALL:
		strcat_s(msgBuffer, " DXGI_ERROR_INVALID_CALL\n");
		error = true;
		break;
	//case D3D12_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS:
	//	strcat_s(msgBuffer, " D3D12_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS\n");
	//	error = true;
	//	break;
	//case D3D12_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS:
	//	strcat_s(msgBuffer, " D3D12_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS\n");
	//	error = true;
	//	break;
	//case D3D12_ERROR_FILE_NOT_FOUND:
	//	strcat_s(msgBuffer, " D3D12_ERROR_FILE_NOT_FOUND\n");
	//	error = true;
	//	break;
	case DXGI_ERROR_ACCESS_DENIED:
		strcat_s(msgBuffer, " DXGI_ERROR_ACCESS_DENIED\n");
		error = true;
		break;
	case DXGI_ERROR_ACCESS_LOST:
		strcat_s(msgBuffer, " DXGI_ERROR_ACCESS_LOST\n");
		error = true;
		break;
	case DXGI_ERROR_ALREADY_EXISTS:
		strcat_s(msgBuffer, " DXGI_ERROR_ALREADY_EXISTS\n");
		error = true;
		break;
	case DXGI_ERROR_CANNOT_PROTECT_CONTENT:
		strcat_s(msgBuffer, " DXGI_ERROR_CANNOT_PROTECT_CONTENT\n");
		error = true;
		break;
	case DXGI_ERROR_DEVICE_HUNG:
		strcat_s(msgBuffer, " DXGI_ERROR_DEVICE_HUNG\n");
		error = true;
		break;
	case DXGI_ERROR_DEVICE_REMOVED:
		strcat_s(msgBuffer, " DXGI_ERROR_DEVICE_REMOVED\n");
		error = true;
		break;
	case DXGI_ERROR_DEVICE_RESET:
		strcat_s(msgBuffer, " DXGI_ERROR_DEVICE_RESET\n");
		error = true;
		break;
	case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
		strcat_s(msgBuffer, " DXGI_ERROR_DRIVER_INTERNAL_ERROR\n");
		error = true;
		break;
	case DXGI_ERROR_FRAME_STATISTICS_DISJOINT:
		strcat_s(msgBuffer, " DXGI_ERROR_FRAME_STATISTICS_DISJOINT\n");
		error = true;
		break;
	case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE:
		strcat_s(msgBuffer, " DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE\n");
		error = true;
		break;
	case DXGI_ERROR_MORE_DATA:
		strcat_s(msgBuffer, " DXGI_ERROR_MORE_DATA\n");
		error = true;
		break;
	case DXGI_ERROR_NAME_ALREADY_EXISTS:
		strcat_s(msgBuffer, " DXGI_ERROR_NAME_ALREADY_EXISTS\n");
		error = true;
		break;
	case DXGI_ERROR_NONEXCLUSIVE:
		strcat_s(msgBuffer, " DXGI_ERROR_NONEXCLUSIVE\n");
		error = true;
		break;
	case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:
		strcat_s(msgBuffer, " DXGI_ERROR_NOT_CURRENTLY_AVAILABLE\n");
		error = true;
		break;
	case DXGI_ERROR_NOT_FOUND:
		strcat_s(msgBuffer, " DXGI_ERROR_NOT_FOUND\n");
		error = true;
		break;
	case DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE:
		strcat_s(msgBuffer, " DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE\n");
		error = true;
		break;
	case DXGI_ERROR_SDK_COMPONENT_MISSING:
		strcat_s(msgBuffer, " DXGI_ERROR_SDK_COMPONENT_MISSING\n");
		error = true;
		break;
	case DXGI_ERROR_SESSION_DISCONNECTED:
		strcat_s(msgBuffer, " DXGI_ERROR_SESSION_DISCONNECTED\n");
		error = true;
		break;
	case DXGI_ERROR_UNSUPPORTED:
		strcat_s(msgBuffer, " DXGI_ERROR_UNSUPPORTED\n");
		error = true;
		break;
	case DXGI_ERROR_WAIT_TIMEOUT:
		strcat_s(msgBuffer, " DXGI_ERROR_WAIT_TIMEOUT\n");
		error = true;
		break;
	default:
		break;
	}

	if (error)
	{
		printf(msgBuffer);
	}

	if (error && exitOnError)
	{
		util::fatalExit(msgBuffer, EXIT_FAILURE);
	}

	return result;
}

D3D12_COMPARISON_FUNC gal::UtilityDx12::translate(CompareOp compareOp)
{
	switch (compareOp)
	{
	case CompareOp::NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case CompareOp::LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case CompareOp::EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case CompareOp::LESS_OR_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case CompareOp::GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case CompareOp::NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case CompareOp::GREATER_OR_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case CompareOp::ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		assert(false);
		return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

D3D12_TEXTURE_ADDRESS_MODE gal::UtilityDx12::translate(SamplerAddressMode addressMode)
{
	switch (addressMode)
	{
	case SamplerAddressMode::REPEAT:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case SamplerAddressMode::MIRRORED_REPEAT:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SamplerAddressMode::CLAMP_TO_EDGE:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SamplerAddressMode::CLAMP_TO_BORDER:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	case SamplerAddressMode::MIRROR_CLAMP_TO_EDGE:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
	default:
		assert(false);
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

D3D12_FILTER gal::UtilityDx12::translate(Filter magFilter, Filter minFilter, SamplerMipmapMode mipmapMode, bool compareEnable, bool anisotropyEnable)
{
	if (!compareEnable)
	{
		if (anisotropyEnable)
		{
			return D3D12_FILTER_ANISOTROPIC;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}
	else
	{
		if (anisotropyEnable)
		{
			return D3D12_FILTER_COMPARISON_ANISOTROPIC;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
	}
	assert(false);
	return D3D12_FILTER();
}

UINT gal::UtilityDx12::translate(ComponentMapping mapping)
{
	auto translateSwizzle = [](ComponentSwizzle swizzle, D3D12_SHADER_COMPONENT_MAPPING identity)
	{
		switch (swizzle)
		{
		case ComponentSwizzle::IDENTITY:
			return identity;
		case ComponentSwizzle::ZERO:
			return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
		case ComponentSwizzle::ONE:
			return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
		case ComponentSwizzle::R:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
		case ComponentSwizzle::G:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
		case ComponentSwizzle::B:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
		case ComponentSwizzle::A:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
		default:
			assert(false);
			break;
		}
		return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
	};

	return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
		translateSwizzle(mapping.m_r, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0),
		translateSwizzle(mapping.m_g, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1),
		translateSwizzle(mapping.m_b, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2),
		translateSwizzle(mapping.m_a, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3));
}

D3D12_BLEND gal::UtilityDx12::translate(BlendFactor factor)
{
	switch (factor)
	{
	case BlendFactor::ZERO:
		return D3D12_BLEND_ZERO;
	case BlendFactor::ONE:
		return D3D12_BLEND_ONE;
	case BlendFactor::SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case BlendFactor::ONE_MINUS_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case BlendFactor::DST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case BlendFactor::ONE_MINUS_DST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case BlendFactor::SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case BlendFactor::ONE_MINUS_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case BlendFactor::DST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case BlendFactor::ONE_MINUS_DST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case BlendFactor::CONSTANT_COLOR:
		return D3D12_BLEND_BLEND_FACTOR;
	case BlendFactor::ONE_MINUS_CONSTANT_COLOR:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case BlendFactor::CONSTANT_ALPHA:
		return D3D12_BLEND_BLEND_FACTOR; // TODO
	case BlendFactor::ONE_MINUS_CONSTANT_ALPHA:
		return D3D12_BLEND_INV_BLEND_FACTOR; // TODO
	case BlendFactor::SRC_ALPHA_SATURATE:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case BlendFactor::SRC1_COLOR:
		return D3D12_BLEND_SRC1_COLOR;
	case BlendFactor::ONE_MINUS_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case BlendFactor::SRC1_ALPHA:
		return D3D12_BLEND_SRC1_ALPHA;
	case BlendFactor::ONE_MINUS_SRC1_ALPHA:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		assert(false);
		break;
	}
	return D3D12_BLEND();
}

D3D12_BLEND_OP gal::UtilityDx12::translate(BlendOp blendOp)
{
	switch (blendOp)
	{
	case BlendOp::ADD:
		return D3D12_BLEND_OP_ADD;
	case BlendOp::SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case BlendOp::REVERSE_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case BlendOp::MIN:
		return D3D12_BLEND_OP_MIN;
	case BlendOp::MAX:
		return D3D12_BLEND_OP_MAX;
	default:
		assert(false);
		break;
	}
	return D3D12_BLEND_OP();
}

D3D12_LOGIC_OP gal::UtilityDx12::translate(LogicOp logicOp)
{
	switch (logicOp)
	{
	case LogicOp::CLEAR:
		return D3D12_LOGIC_OP_CLEAR;
	case LogicOp::AND:
		return D3D12_LOGIC_OP_AND;
	case LogicOp::AND_REVERSE:
		return D3D12_LOGIC_OP_AND_REVERSE;
	case LogicOp::COPY:
		return D3D12_LOGIC_OP_COPY;
	case LogicOp::AND_INVERTED:
		return D3D12_LOGIC_OP_AND_INVERTED;
	case LogicOp::NO_OP:
		return D3D12_LOGIC_OP_NOOP;
	case LogicOp::XOR:
		return D3D12_LOGIC_OP_XOR;
	case LogicOp::OR:
		return D3D12_LOGIC_OP_OR;
	case LogicOp::NOR:
		return D3D12_LOGIC_OP_NOR;
	case LogicOp::EQUIVALENT:
		return D3D12_LOGIC_OP_EQUIV;
	case LogicOp::INVERT:
		return D3D12_LOGIC_OP_INVERT;
	case LogicOp::OR_REVERSE:
		return D3D12_LOGIC_OP_OR_REVERSE;
	case LogicOp::COPY_INVERTED:
		return D3D12_LOGIC_OP_COPY_INVERTED;
	case LogicOp::OR_INVERTED:
		return D3D12_LOGIC_OP_OR_INVERTED;
	case LogicOp::NAND:
		return D3D12_LOGIC_OP_NAND;
	case LogicOp::SET:
		return D3D12_LOGIC_OP_SET;
	default:
		assert(false);
		break;
	}
	return D3D12_LOGIC_OP();
}

D3D12_STENCIL_OP gal::UtilityDx12::translate(StencilOp stencilOp)
{
	switch (stencilOp)
	{
	case StencilOp::KEEP:
		return D3D12_STENCIL_OP_KEEP;
	case StencilOp::ZERO:
		return D3D12_STENCIL_OP_ZERO;
	case StencilOp::REPLACE:
		return D3D12_STENCIL_OP_REPLACE;
	case StencilOp::INCREMENT_AND_CLAMP:
		return D3D12_STENCIL_OP_INCR_SAT;
	case StencilOp::DECREMENT_AND_CLAMP:
		return D3D12_STENCIL_OP_DECR_SAT;
	case StencilOp::INVERT:
		return D3D12_STENCIL_OP_INVERT;
	case StencilOp::INCREMENT_AND_WRAP:
		return D3D12_STENCIL_OP_INCR;
	case StencilOp::DECREMENT_AND_WRAP:
		return D3D12_STENCIL_OP_DECR;
	default:
		assert(false);
		break;
	}
	return D3D12_STENCIL_OP();
}

D3D12_PRIMITIVE_TOPOLOGY gal::UtilityDx12::translate(PrimitiveTopology topology, uint32_t patchControlPoints)
{
	switch (topology)
	{
	case PrimitiveTopology::POINT_LIST:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case PrimitiveTopology::LINE_LIST:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PrimitiveTopology::LINE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case PrimitiveTopology::TRIANGLE_LIST:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PrimitiveTopology::TRIANGLE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PrimitiveTopology::TRIANGLE_FAN:
		break;
	case PrimitiveTopology::LINE_LIST_WITH_ADJACENCY:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case PrimitiveTopology::LINE_STRIP_WITH_ADJACENCY:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
	case PrimitiveTopology::TRIANGLE_LIST_WITH_ADJACENCY:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case PrimitiveTopology::TRIANGLE_STRIP_WITH_ADJACENCY:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	case PrimitiveTopology::PATCH_LIST:
		assert(patchControlPoints >= 1 && patchControlPoints <= 32);
		return static_cast<D3D_PRIMITIVE_TOPOLOGY>(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (patchControlPoints - 1));
	default:
		assert(false);
		break;
	}
	return D3D12_PRIMITIVE_TOPOLOGY();
}

D3D12_QUERY_HEAP_TYPE gal::UtilityDx12::translate(QueryType queryType)
{
	switch (queryType)
	{
	case QueryType::OCCLUSION:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case QueryType::PIPELINE_STATISTICS:
		return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	case QueryType::TIMESTAMP:
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	default:
		assert(false);
		break;
	}
	return D3D12_QUERY_HEAP_TYPE();
}

DXGI_FORMAT gal::UtilityDx12::translate(Format format)
{
	switch (format)
	{
	case Format::UNDEFINED:
		return DXGI_FORMAT_UNKNOWN;
	case Format::R4G4_UNORM_PACK8:
		break;
	case Format::R4G4B4A4_UNORM_PACK16:
		break;
	case Format::B4G4R4A4_UNORM_PACK16:
		break;
	case Format::R5G6B5_UNORM_PACK16:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case Format::B5G6R5_UNORM_PACK16:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case Format::R5G5B5A1_UNORM_PACK16:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Format::B5G5R5A1_UNORM_PACK16:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Format::A1R5G5B5_UNORM_PACK16:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Format::R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case Format::R8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case Format::R8_USCALED:
		break;
	case Format::R8_SSCALED:
		break;
	case Format::R8_UINT:
		return DXGI_FORMAT_R8_UINT;
	case Format::R8_SINT:
		return DXGI_FORMAT_R8_SINT;
	case Format::R8_SRGB:
		break;
	case Format::R8G8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case Format::R8G8_SNORM:
		return DXGI_FORMAT_R8G8_SNORM;
	case Format::R8G8_USCALED:
		break;
	case Format::R8G8_SSCALED:
		break;
	case Format::R8G8_UINT:
		return DXGI_FORMAT_R8G8_UINT;
	case Format::R8G8_SINT:
		return DXGI_FORMAT_R8G8_SINT;
	case Format::R8G8_SRGB:
		break;
	case Format::R8G8B8_UNORM:
		break;
	case Format::R8G8B8_SNORM:
		break;
	case Format::R8G8B8_USCALED:
		break;
	case Format::R8G8B8_SSCALED:
		break;
	case Format::R8G8B8_UINT:
		break;
	case Format::R8G8B8_SINT:
		break;
	case Format::R8G8B8_SRGB:
		break;
	case Format::B8G8R8_UNORM:
		break;
	case Format::B8G8R8_SNORM:
		break;
	case Format::B8G8R8_USCALED:
		break;
	case Format::B8G8R8_SSCALED:
		break;
	case Format::B8G8R8_UINT:
		break;
	case Format::B8G8R8_SINT:
		break;
	case Format::B8G8R8_SRGB:
		break;
	case Format::R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Format::R8G8B8A8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case Format::R8G8B8A8_USCALED:
		break;
	case Format::R8G8B8A8_SSCALED:
		break;
	case Format::R8G8B8A8_UINT:
		return DXGI_FORMAT_R8G8B8A8_UINT;
	case Format::R8G8B8A8_SINT:
		return DXGI_FORMAT_R8G8B8A8_SINT;
	case Format::R8G8B8A8_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case Format::B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case Format::B8G8R8A8_SNORM:
		break;
	case Format::B8G8R8A8_USCALED:
		break;
	case Format::B8G8R8A8_SSCALED:
		break;
	case Format::B8G8R8A8_UINT:
		break;
	case Format::B8G8R8A8_SINT:
		break;
	case Format::B8G8R8A8_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case Format::A8B8G8R8_UNORM_PACK32:
		break;
	case Format::A8B8G8R8_SNORM_PACK32:
		break;
	case Format::A8B8G8R8_USCALED_PACK32:
		break;
	case Format::A8B8G8R8_SSCALED_PACK32:
		break;
	case Format::A8B8G8R8_UINT_PACK32:
		break;
	case Format::A8B8G8R8_SINT_PACK32:
		break;
	case Format::A8B8G8R8_SRGB_PACK32:
		break;
	case Format::A2R10G10B10_UNORM_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case Format::A2R10G10B10_SNORM_PACK32:
		break;
	case Format::A2R10G10B10_USCALED_PACK32:
		break;
	case Format::A2R10G10B10_SSCALED_PACK32:
		break;
	case Format::A2R10G10B10_UINT_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UINT;
	case Format::A2R10G10B10_SINT_PACK32:
		break;
	case Format::A2B10G10R10_UNORM_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case Format::A2B10G10R10_SNORM_PACK32:
		break;
	case Format::A2B10G10R10_USCALED_PACK32:
		break;
	case Format::A2B10G10R10_SSCALED_PACK32:
		break;
	case Format::A2B10G10R10_UINT_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UINT;
	case Format::A2B10G10R10_SINT_PACK32:
		break;
	case Format::R16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case Format::R16_SNORM:
		return DXGI_FORMAT_R16_SNORM;
	case Format::R16_USCALED:
		break;
	case Format::R16_SSCALED:
		break;
	case Format::R16_UINT:
		return DXGI_FORMAT_R16_UINT;
	case Format::R16_SINT:
		return DXGI_FORMAT_R16_SINT;
	case Format::R16_SFLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case Format::R16G16_UNORM:
		return DXGI_FORMAT_R16G16_UNORM;
	case Format::R16G16_SNORM:
		return DXGI_FORMAT_R16G16_SNORM;
	case Format::R16G16_USCALED:
		break;
	case Format::R16G16_SSCALED:
		break;
	case Format::R16G16_UINT:
		return DXGI_FORMAT_R16G16_UINT;
	case Format::R16G16_SINT:
		return DXGI_FORMAT_R16G16_SINT;
	case Format::R16G16_SFLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case Format::R16G16B16_UNORM:
		break;
	case Format::R16G16B16_SNORM:
		break;
	case Format::R16G16B16_USCALED:
		break;
	case Format::R16G16B16_SSCALED:
		break;
	case Format::R16G16B16_UINT:
		break;
	case Format::R16G16B16_SINT:
		break;
	case Format::R16G16B16_SFLOAT:
		break;
	case Format::R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case Format::R16G16B16A16_SNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case Format::R16G16B16A16_USCALED:
		break;
	case Format::R16G16B16A16_SSCALED:
		break;
	case Format::R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_UINT;
	case Format::R16G16B16A16_SINT:
		return DXGI_FORMAT_R16G16B16A16_SINT;
	case Format::R16G16B16A16_SFLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case Format::R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case Format::R32_SINT:
		return DXGI_FORMAT_R32_SINT;
	case Format::R32_SFLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case Format::R32G32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case Format::R32G32_SINT:
		return DXGI_FORMAT_R32G32_SINT;
	case Format::R32G32_SFLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case Format::R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case Format::R32G32B32_SINT:
		return DXGI_FORMAT_R32G32B32_SINT;
	case Format::R32G32B32_SFLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case Format::R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case Format::R32G32B32A32_SINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case Format::R32G32B32A32_SFLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case Format::R64_UINT:
		break;
	case Format::R64_SINT:
		break;
	case Format::R64_SFLOAT:
		break;
	case Format::R64G64_UINT:
		break;
	case Format::R64G64_SINT:
		break;
	case Format::R64G64_SFLOAT:
		break;
	case Format::R64G64B64_UINT:
		break;
	case Format::R64G64B64_SINT:
		break;
	case Format::R64G64B64_SFLOAT:
		break;
	case Format::R64G64B64A64_UINT:
		break;
	case Format::R64G64B64A64_SINT:
		break;
	case Format::R64G64B64A64_SFLOAT:
		break;
	case Format::B10G11R11_UFLOAT_PACK32:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case Format::E5B9G9R9_UFLOAT_PACK32:
		return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
	case Format::D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case Format::X8_D24_UNORM_PACK32:
		break;
	case Format::D32_SFLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case Format::S8_UINT:
		break;
	case Format::D16_UNORM_S8_UINT:
		break;
	case Format::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case Format::D32_SFLOAT_S8_UINT:
		break;
	case Format::BC1_RGB_UNORM_BLOCK:
		break;
	case Format::BC1_RGB_SRGB_BLOCK:
		break;
	case Format::BC1_RGBA_UNORM_BLOCK:
		return DXGI_FORMAT_BC1_UNORM;
	case Format::BC1_RGBA_SRGB_BLOCK:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
	case Format::BC2_UNORM_BLOCK:
		return DXGI_FORMAT_BC2_UNORM;
	case Format::BC2_SRGB_BLOCK:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
	case Format::BC3_UNORM_BLOCK:
		return DXGI_FORMAT_BC3_UNORM;
	case Format::BC3_SRGB_BLOCK:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
	case Format::BC4_UNORM_BLOCK:
		return DXGI_FORMAT_BC4_UNORM;
	case Format::BC4_SNORM_BLOCK:
		return DXGI_FORMAT_BC4_SNORM;
	case Format::BC5_UNORM_BLOCK:
		return DXGI_FORMAT_BC5_UNORM;
	case Format::BC5_SNORM_BLOCK:
		return DXGI_FORMAT_BC5_SNORM;
	case Format::BC6H_UFLOAT_BLOCK:
		return DXGI_FORMAT_BC6H_UF16;
	case Format::BC6H_SFLOAT_BLOCK:
		return DXGI_FORMAT_BC6H_SF16;
	case Format::BC7_UNORM_BLOCK:
		return DXGI_FORMAT_BC7_UNORM;
	case Format::BC7_SRGB_BLOCK:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	default:
		break;
	}
	assert(false);
	return DXGI_FORMAT();
}

D3D12_SHADER_VISIBILITY gal::UtilityDx12::translate(ShaderStageFlags shaderStageFlags)
{
	switch (shaderStageFlags)
	{
	case ShaderStageFlags::VERTEX_BIT:
		return D3D12_SHADER_VISIBILITY_VERTEX;
	case ShaderStageFlags::HULL_BIT:
		return D3D12_SHADER_VISIBILITY_HULL;
	case ShaderStageFlags::DOMAIN_BIT:
		return D3D12_SHADER_VISIBILITY_DOMAIN;
	case ShaderStageFlags::GEOMETRY_BIT:
		return D3D12_SHADER_VISIBILITY_GEOMETRY;
	case ShaderStageFlags::PIXEL_BIT:
		return D3D12_SHADER_VISIBILITY_PIXEL;
	case ShaderStageFlags::COMPUTE_BIT:
		return D3D12_SHADER_VISIBILITY_ALL;
	default:
		return D3D12_SHADER_VISIBILITY_ALL;
	}
	return D3D12_SHADER_VISIBILITY_ALL;
}

template<typename T>
static bool testFlagBit(T flags, T bit)
{
	return (flags & bit) == bit;
}

D3D12_RESOURCE_FLAGS gal::UtilityDx12::translateImageUsageFlags(ImageUsageFlags flags)
{
	D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;

	if (testFlagBit(flags, ImageUsageFlags::TRANSFER_SRC_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, ImageUsageFlags::TRANSFER_DST_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, ImageUsageFlags::TEXTURE_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, ImageUsageFlags::RW_TEXTURE_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (testFlagBit(flags, ImageUsageFlags::COLOR_ATTACHMENT_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	if (testFlagBit(flags, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		if (!testFlagBit(flags, ImageUsageFlags::TEXTURE_BIT))
		{
			result |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}
	}
	if (testFlagBit(flags, ImageUsageFlags::CLEAR_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	return result;
}

D3D12_RESOURCE_FLAGS gal::UtilityDx12::translateBufferUsageFlags(BufferUsageFlags flags)
{
	D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;

	if (testFlagBit(flags, BufferUsageFlags::TRANSFER_SRC_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::TRANSFER_DST_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::TYPED_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::RW_TYPED_BUFFER_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (testFlagBit(flags, BufferUsageFlags::CONSTANT_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::BYTE_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::RW_BYTE_BUFFER_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (testFlagBit(flags, BufferUsageFlags::STRUCTURED_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::RW_STRUCTURED_BUFFER_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (testFlagBit(flags, BufferUsageFlags::INDEX_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::VERTEX_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::INDIRECT_BUFFER_BIT))
	{
		// no D3D12 equivalent
	}
	if (testFlagBit(flags, BufferUsageFlags::CLEAR_BIT))
	{
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (testFlagBit(flags, BufferUsageFlags::SHADER_DEVICE_ADDRESS_BIT))
	{
		// no D3D12 equivalent
	}

	return result;
}

UINT gal::UtilityDx12::formatByteSize(Format format)
{
	switch (format)
	{
	case Format::UNDEFINED:
		break;
	case Format::R4G4_UNORM_PACK8:
		return 1;
	case Format::R4G4B4A4_UNORM_PACK16:
		return 2;
	case Format::B4G4R4A4_UNORM_PACK16:
		break;
	case Format::R5G6B5_UNORM_PACK16:
		return 2;
	case Format::B5G6R5_UNORM_PACK16:
		return 2;
	case Format::R5G5B5A1_UNORM_PACK16:
		return 2;
	case Format::B5G5R5A1_UNORM_PACK16:
		return 2;
	case Format::A1R5G5B5_UNORM_PACK16:
		return 2;
	case Format::R8_UNORM:
		return 1;
	case Format::R8_SNORM:
		return 1;
	case Format::R8_USCALED:
		return 1;
	case Format::R8_SSCALED:
		return 1;
	case Format::R8_UINT:
		return 1;
	case Format::R8_SINT:
		return 1;
	case Format::R8_SRGB:
		break;
	case Format::R8G8_UNORM:
		return 2;
	case Format::R8G8_SNORM:
		return 2;
	case Format::R8G8_USCALED:
		break;
	case Format::R8G8_SSCALED:
		break;
	case Format::R8G8_UINT:
		return 2;
	case Format::R8G8_SINT:
		return 2;
	case Format::R8G8_SRGB:
		break;
	case Format::R8G8B8_UNORM:
		break;
	case Format::R8G8B8_SNORM:
		break;
	case Format::R8G8B8_USCALED:
		break;
	case Format::R8G8B8_SSCALED:
		break;
	case Format::R8G8B8_UINT:
		break;
	case Format::R8G8B8_SINT:
		break;
	case Format::R8G8B8_SRGB:
		break;
	case Format::B8G8R8_UNORM:
		break;
	case Format::B8G8R8_SNORM:
		break;
	case Format::B8G8R8_USCALED:
		break;
	case Format::B8G8R8_SSCALED:
		break;
	case Format::B8G8R8_UINT:
		break;
	case Format::B8G8R8_SINT:
		break;
	case Format::B8G8R8_SRGB:
		break;
	case Format::R8G8B8A8_UNORM:
		return 4;
	case Format::R8G8B8A8_SNORM:
		return 4;
	case Format::R8G8B8A8_USCALED:
		break;
	case Format::R8G8B8A8_SSCALED:
		break;
	case Format::R8G8B8A8_UINT:
		return 4;
	case Format::R8G8B8A8_SINT:
		return 4;
	case Format::R8G8B8A8_SRGB:
		return 4;
	case Format::B8G8R8A8_UNORM:
		return 4;
	case Format::B8G8R8A8_SNORM:
		break;
	case Format::B8G8R8A8_USCALED:
		break;
	case Format::B8G8R8A8_SSCALED:
		break;
	case Format::B8G8R8A8_UINT:
		break;
	case Format::B8G8R8A8_SINT:
		break;
	case Format::B8G8R8A8_SRGB:
		return 4;
	case Format::A8B8G8R8_UNORM_PACK32:
		break;
	case Format::A8B8G8R8_SNORM_PACK32:
		break;
	case Format::A8B8G8R8_USCALED_PACK32:
		break;
	case Format::A8B8G8R8_SSCALED_PACK32:
		break;
	case Format::A8B8G8R8_UINT_PACK32:
		break;
	case Format::A8B8G8R8_SINT_PACK32:
		break;
	case Format::A8B8G8R8_SRGB_PACK32:
		break;
	case Format::A2R10G10B10_UNORM_PACK32:
		return 4;
	case Format::A2R10G10B10_SNORM_PACK32:
		break;
	case Format::A2R10G10B10_USCALED_PACK32:
		break;
	case Format::A2R10G10B10_SSCALED_PACK32:
		break;
	case Format::A2R10G10B10_UINT_PACK32:
		return 4;
	case Format::A2R10G10B10_SINT_PACK32:
		break;
	case Format::A2B10G10R10_UNORM_PACK32:
		return 4;
	case Format::A2B10G10R10_SNORM_PACK32:
		break;
	case Format::A2B10G10R10_USCALED_PACK32:
		break;
	case Format::A2B10G10R10_SSCALED_PACK32:
		break;
	case Format::A2B10G10R10_UINT_PACK32:
		return 4;
	case Format::A2B10G10R10_SINT_PACK32:
		break;
	case Format::R16_UNORM:
		return 2;
	case Format::R16_SNORM:
		return 2;
	case Format::R16_USCALED:
		break;
	case Format::R16_SSCALED:
		break;
	case Format::R16_UINT:
		return 2;
	case Format::R16_SINT:
		return 2;
	case Format::R16_SFLOAT:
		return 2;
	case Format::R16G16_UNORM:
		return 4;
	case Format::R16G16_SNORM:
		return 4;
	case Format::R16G16_USCALED:
		break;
	case Format::R16G16_SSCALED:
		break;
	case Format::R16G16_UINT:
		return 4;
	case Format::R16G16_SINT:
		return 4;
	case Format::R16G16_SFLOAT:
		return 4;
	case Format::R16G16B16_UNORM:
		break;
	case Format::R16G16B16_SNORM:
		break;
	case Format::R16G16B16_USCALED:
		break;
	case Format::R16G16B16_SSCALED:
		break;
	case Format::R16G16B16_UINT:
		break;
	case Format::R16G16B16_SINT:
		break;
	case Format::R16G16B16_SFLOAT:
		break;
	case Format::R16G16B16A16_UNORM:
		return 8;
	case Format::R16G16B16A16_SNORM:
		return 8;
	case Format::R16G16B16A16_USCALED:
		break;
	case Format::R16G16B16A16_SSCALED:
		break;
	case Format::R16G16B16A16_UINT:
		return 8;
	case Format::R16G16B16A16_SINT:
		return 8;
	case Format::R16G16B16A16_SFLOAT:
		return 8;
	case Format::R32_UINT:
		return 4;
	case Format::R32_SINT:
		return 4;
	case Format::R32_SFLOAT:
		return 4;
	case Format::R32G32_UINT:
		return 8;
	case Format::R32G32_SINT:
		return 8;
	case Format::R32G32_SFLOAT:
		return 8;
	case Format::R32G32B32_UINT:
		return 12;
	case Format::R32G32B32_SINT:
		return 12;
	case Format::R32G32B32_SFLOAT:
		return 12;
	case Format::R32G32B32A32_UINT:
		return 16;
	case Format::R32G32B32A32_SINT:
		return 16;
	case Format::R32G32B32A32_SFLOAT:
		return 16;
	case Format::R64_UINT:
		break;
	case Format::R64_SINT:
		break;
	case Format::R64_SFLOAT:
		break;
	case Format::R64G64_UINT:
		break;
	case Format::R64G64_SINT:
		break;
	case Format::R64G64_SFLOAT:
		break;
	case Format::R64G64B64_UINT:
		break;
	case Format::R64G64B64_SINT:
		break;
	case Format::R64G64B64_SFLOAT:
		break;
	case Format::R64G64B64A64_UINT:
		break;
	case Format::R64G64B64A64_SINT:
		break;
	case Format::R64G64B64A64_SFLOAT:
		break;
	case Format::B10G11R11_UFLOAT_PACK32:
		return 4;
	case Format::E5B9G9R9_UFLOAT_PACK32:
		return 4;
	case Format::D16_UNORM:
		return 2;
	case Format::X8_D24_UNORM_PACK32:
		break;
	case Format::D32_SFLOAT:
		return 4;
	case Format::S8_UINT:
		break;
	case Format::D16_UNORM_S8_UINT:
		break;
	case Format::D24_UNORM_S8_UINT:
		return 4;
	case Format::D32_SFLOAT_S8_UINT:
		break;
	case Format::BC1_RGB_UNORM_BLOCK:
		break;
	case Format::BC1_RGB_SRGB_BLOCK:
		break;
	case Format::BC1_RGBA_UNORM_BLOCK:
		return 8;
	case Format::BC1_RGBA_SRGB_BLOCK:
		return 8;
	case Format::BC2_UNORM_BLOCK:
		return 16;
	case Format::BC2_SRGB_BLOCK:
		return 16;
	case Format::BC3_UNORM_BLOCK:
		return 16;
	case Format::BC3_SRGB_BLOCK:
		return 16;
	case Format::BC4_UNORM_BLOCK:
		return 8;
	case Format::BC4_SNORM_BLOCK:
		return 8;
	case Format::BC5_UNORM_BLOCK:
		return 16;
	case Format::BC5_SNORM_BLOCK:
		return 16;
	case Format::BC6H_UFLOAT_BLOCK:
		return 16;
	case Format::BC6H_SFLOAT_BLOCK:
		return 16;
	case Format::BC7_UNORM_BLOCK:
		return 16;
	case Format::BC7_SRGB_BLOCK:
		return 16;
	default:
		break;
	}
	assert(false);
	return 1;
}

DXGI_FORMAT gal::UtilityDx12::getTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_UNKNOWN:
		break;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return DXGI_FORMAT_R32G32B32A32_TYPELESS;
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return DXGI_FORMAT_R32G32B32_TYPELESS;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		return DXGI_FORMAT_R32G32_TYPELESS;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
		return DXGI_FORMAT_R10G10B10A2_TYPELESS;
	case DXGI_FORMAT_R11G11B10_FLOAT:
		break;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
		return DXGI_FORMAT_R16G16_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		return DXGI_FORMAT_R8G8_TYPELESS;
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return DXGI_FORMAT_R8_TYPELESS;
	case DXGI_FORMAT_R1_UNORM:
		break;
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		break;
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
		break;
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
		break;
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		return DXGI_FORMAT_BC1_TYPELESS;
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		return DXGI_FORMAT_BC2_TYPELESS;
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		return DXGI_FORMAT_BC3_TYPELESS;
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return DXGI_FORMAT_BC4_TYPELESS;
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		return DXGI_FORMAT_BC5_TYPELESS;
	case DXGI_FORMAT_B5G6R5_UNORM:
		break;
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		break;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
		return DXGI_FORMAT_BC6H_TYPELESS;
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return DXGI_FORMAT_BC7_TYPELESS;
	case DXGI_FORMAT_AYUV:
		break;
	case DXGI_FORMAT_Y410:
		break;
	case DXGI_FORMAT_Y416:
		break;
	case DXGI_FORMAT_NV12:
		break;
	case DXGI_FORMAT_P010:
		break;
	case DXGI_FORMAT_P016:
		break;
	case DXGI_FORMAT_420_OPAQUE:
		break;
	case DXGI_FORMAT_YUY2:
		break;
	case DXGI_FORMAT_Y210:
		break;
	case DXGI_FORMAT_Y216:
		break;
	case DXGI_FORMAT_NV11:
		break;
	case DXGI_FORMAT_AI44:
		break;
	case DXGI_FORMAT_IA44:
		break;
	case DXGI_FORMAT_P8:
		break;
	case DXGI_FORMAT_A8P8:
		break;
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		break;
	case DXGI_FORMAT_P208:
		break;
	case DXGI_FORMAT_V208:
		break;
	case DXGI_FORMAT_V408:
		break;
	case DXGI_FORMAT_FORCE_UINT:
		break;
	default:
		assert(false);
		break;
	}

	return DXGI_FORMAT();
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE gal::UtilityDx12::getTopologyType(PrimitiveTopology topology)
{
	switch (topology)
	{
	case PrimitiveTopology::POINT_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case PrimitiveTopology::LINE_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::LINE_STRIP:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::TRIANGLE_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::TRIANGLE_STRIP:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::TRIANGLE_FAN:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::LINE_LIST_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::LINE_STRIP_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::TRIANGLE_LIST_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::TRIANGLE_STRIP_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::PATCH_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	default:
		assert(false);
		break;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE();
}
