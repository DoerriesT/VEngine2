#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace gal
{
	namespace UtilityDx12
	{
		HRESULT checkResult(HRESULT result, const char *errorMsg = nullptr, bool exitOnError = true);
		D3D12_COMPARISON_FUNC translate(CompareOp compareOp);
		D3D12_TEXTURE_ADDRESS_MODE translate(SamplerAddressMode addressMode);
		D3D12_FILTER translate(Filter magFilter, Filter minFilter, SamplerMipmapMode mipmapMode, bool compareEnable, bool anisotropyEnable);
		UINT translate(ComponentMapping mapping);
		D3D12_BLEND translate(BlendFactor factor);
		D3D12_BLEND_OP translate(BlendOp blendOp);
		D3D12_LOGIC_OP translate(LogicOp logicOp);
		D3D12_STENCIL_OP translate(StencilOp stencilOp);
		D3D12_PRIMITIVE_TOPOLOGY translate(PrimitiveTopology topology, uint32_t patchControlPoints);
		D3D12_QUERY_HEAP_TYPE translate(QueryType queryType);
		DXGI_FORMAT translate(Format format);
		D3D12_SHADER_VISIBILITY translate(ShaderStageFlags shaderStageFlags);
		D3D12_RESOURCE_FLAGS translateImageUsageFlags(ImageUsageFlags flags);
		D3D12_RESOURCE_FLAGS translateBufferUsageFlags(BufferUsageFlags flags);
		UINT formatByteSize(Format format);
		DXGI_FORMAT getTypeless(DXGI_FORMAT format);
		D3D12_PRIMITIVE_TOPOLOGY_TYPE getTopologyType(PrimitiveTopology topology);
	}
}