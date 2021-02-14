#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>
#include "Utility/ObjectPool.h"
#include "DescriptorSetDx12.h"

namespace gal
{
	struct GraphicsPipelineCreateInfo;
	struct ComputePipelineCreateInfo;


	class GraphicsPipelineDx12 : public GraphicsPipeline
	{
	public:
		explicit GraphicsPipelineDx12(ID3D12Device *device, const GraphicsPipelineCreateInfo &createInfo);
		GraphicsPipelineDx12(GraphicsPipelineDx12 &) = delete;
		GraphicsPipelineDx12(GraphicsPipelineDx12 &&) = delete;
		GraphicsPipelineDx12 &operator= (const GraphicsPipelineDx12 &) = delete;
		GraphicsPipelineDx12 &operator= (const GraphicsPipelineDx12 &&) = delete;
		~GraphicsPipelineDx12();
		void *getNativeHandle() const override;
		uint32_t getDescriptorSetLayoutCount() const override;
		const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
		ID3D12RootSignature *getRootSignature() const;
		uint32_t getDescriptorTableOffset() const;
		uint32_t getVertexBufferStride(uint32_t bufferBinding) const;
		void initializeState(ID3D12GraphicsCommandList *cmdList) const;
		void getRootDescriptorInfo(uint32_t &setIndex, uint32_t &descriptorCount) const;

	private:
		ID3D12PipelineState *m_pipeline;
		ID3D12RootSignature *m_rootSignature;
		uint32_t m_descriptorTableOffset;
		D3D12_PRIMITIVE_TOPOLOGY m_primitiveTopology;
		UINT m_vertexBufferStrides[32];
		float m_blendFactors[4];
		UINT m_stencilRef;
		uint32_t m_rootDescriptorSetIndex;
		uint32_t m_rootDescriptorCount;
	};

	class ComputePipelineDx12 : public ComputePipeline
	{
	public:
		explicit ComputePipelineDx12(ID3D12Device *device, const ComputePipelineCreateInfo &createInfo);
		ComputePipelineDx12(ComputePipelineDx12 &) = delete;
		ComputePipelineDx12(ComputePipelineDx12 &&) = delete;
		ComputePipelineDx12 &operator= (const ComputePipelineDx12 &) = delete;
		ComputePipelineDx12 &operator= (const ComputePipelineDx12 &&) = delete;
		~ComputePipelineDx12();
		void *getNativeHandle() const override;
		uint32_t getDescriptorSetLayoutCount() const override;
		const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
		ID3D12RootSignature *getRootSignature() const;
		uint32_t getDescriptorTableOffset() const;
		void getRootDescriptorInfo(uint32_t &setIndex, uint32_t &descriptorCount) const;

	private:
		ID3D12PipelineState *m_pipeline;
		ID3D12RootSignature *m_rootSignature;
		uint32_t m_descriptorTableOffset;
		uint32_t m_rootDescriptorSetIndex;
		uint32_t m_rootDescriptorCount;
	};
}