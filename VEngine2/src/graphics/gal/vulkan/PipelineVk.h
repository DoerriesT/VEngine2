#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace gal
{
	struct GraphicsPipelineCreateInfo;
	struct ComputePipelineCreateInfo;
	class GraphicsDeviceVk;

	class GraphicsPipelineVk : public GraphicsPipeline
	{
	public:
		explicit GraphicsPipelineVk(GraphicsDeviceVk &device, const GraphicsPipelineCreateInfo &createInfo);
		GraphicsPipelineVk(GraphicsPipelineVk &) = delete;
		GraphicsPipelineVk(GraphicsPipelineVk &&) = delete;
		GraphicsPipelineVk &operator= (const GraphicsPipelineVk &) = delete;
		GraphicsPipelineVk &operator= (const GraphicsPipelineVk &&) = delete;
		~GraphicsPipelineVk();
		void *getNativeHandle() const override;
		uint32_t getDescriptorSetLayoutCount() const override;
		const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
		VkPipelineLayout getLayout() const;
	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		GraphicsDeviceVk *m_device;
	};

	class ComputePipelineVk : public ComputePipeline
	{
	public:
		explicit ComputePipelineVk(GraphicsDeviceVk &device, const ComputePipelineCreateInfo &createInfo);
		ComputePipelineVk(ComputePipelineVk &) = delete;
		ComputePipelineVk(ComputePipelineVk &&) = delete;
		ComputePipelineVk &operator= (const ComputePipelineVk &) = delete;
		ComputePipelineVk &operator= (const ComputePipelineVk &&) = delete;
		~ComputePipelineVk();
		void *getNativeHandle() const override;
		uint32_t getDescriptorSetLayoutCount() const override;
		const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const override;
		VkPipelineLayout getLayout() const;
	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		GraphicsDeviceVk *m_device;
	};
}