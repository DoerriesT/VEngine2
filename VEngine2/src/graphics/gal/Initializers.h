#pragma once
#include "GraphicsAbstractionLayer.h"

namespace gal
{
	namespace Initializers
	{
		struct FormatInfo
		{
			uint32_t m_bytes;
			bool m_float;
			bool m_compressed;
		};

		DescriptorSetUpdate sampler(const Sampler *const *samplers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate sampler(const Sampler *sampler, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate texture(const ImageView *const *images, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate texture(const ImageView *image, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate depthStencilTexture(const ImageView *const *images, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate depthStencilTexture(const ImageView *image, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate rwTexture(const ImageView *const *images, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate rwTexture(const ImageView *image, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate typedBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate typedBuffer(const BufferView *buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate rwTypedBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate rwTypedBuffer(const BufferView *buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate constantBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate constantBuffer(const DescriptorBufferInfo &buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate byteBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate byteBuffer(const DescriptorBufferInfo &buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate rwByteBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate rwByteBuffer(const DescriptorBufferInfo &buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate structuredBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate structuredBuffer(const DescriptorBufferInfo &buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate rwStructuredBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		DescriptorSetUpdate rwStructuredBuffer(const DescriptorBufferInfo &buffer, uint32_t binding, uint32_t arrayElement = 0);
		DescriptorSetUpdate offsetConstantBuffer(const DescriptorBufferInfo *buffer, uint32_t binding);
		Barrier imageBarrier(const Image *image, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter, const ImageSubresourceRange &subresourceRange = { 0, 1, 0, 1 });
		Barrier bufferBarrier(const Buffer *buffer, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter);
		void submitSingleTimeCommands(Queue *queue, CommandList *cmdList);
		bool isReadAccess(ResourceState state);
		bool isWriteAccess(ResourceState state);
		uint32_t getUsageFlags(ResourceState state, bool isImage);
		bool isDepthFormat(Format format);
		bool isStencilFormat(Format format);
		FormatInfo getFormatInfo(Format format);
	}

	class GraphicsPipelineBuilder
	{
	public:
		static const PipelineColorBlendAttachmentState s_defaultBlendAttachment;

		explicit GraphicsPipelineBuilder(GraphicsPipelineCreateInfo &createInfo);
		void setVertexShader(const char *path);
		void setTessControlShader(const char *path);
		void setTessEvalShader(const char *path);
		void setGeometryShader(const char *path);
		void setFragmentShader(const char *path);
		void setVertexBindingDescriptions(size_t count, const VertexInputBindingDescription *bindingDescs);
		void setVertexBindingDescription(const VertexInputBindingDescription &bindingDesc);
		void setVertexAttributeDescriptions(size_t count, const VertexInputAttributeDescription *attributeDescs);
		void setVertexAttributeDescription(const VertexInputAttributeDescription &attributeDesc);
		void setInputAssemblyState(PrimitiveTopology topology, bool primitiveRestartEnable);
		void setTesselationState(uint32_t patchControlPoints);
		void setViewportScissors(size_t count, const Viewport *viewports, const Rect *scissors);
		void setViewportScissor(const Viewport &viewport, const Rect &scissor);
		void setDepthClampEnable(bool depthClampEnable);
		void setRasterizerDiscardEnable(bool rasterizerDiscardEnable);
		void setPolygonModeCullMode(PolygonMode polygonMode, CullModeFlags cullMode, FrontFace frontFace);
		void setDepthBias(bool enable, float constantFactor, float clamp, float slopeFactor);
		void setLineWidth(float lineWidth);
		void setMultisampleState(SampleCount rasterizationSamples, bool sampleShadingEnable, float minSampleShading, uint32_t sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable);
		void setDepthTest(bool depthTestEnable, bool depthWriteEnable, CompareOp depthCompareOp);
		void setStencilTest(bool stencilTestEnable, const StencilOpState &front, const StencilOpState &back);
		void setDepthBoundsTest(bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds);
		void setBlendStateLogicOp(bool logicOpEnable, LogicOp logicOp);
		void setBlendConstants(float blendConst0, float blendConst1, float blendConst2, float blendConst3);
		void setColorBlendAttachments(size_t count, const PipelineColorBlendAttachmentState *colorBlendAttachments);
		void setColorBlendAttachment(const PipelineColorBlendAttachmentState &colorBlendAttachment);
		void setDynamicState(DynamicStateFlags dynamicStateFlags);
		void setColorAttachmentFormats(uint32_t count, Format *formats);
		void setColorAttachmentFormat(Format format);
		void setDepthStencilAttachmentFormat(Format format);
		void setPipelineLayoutDescription(uint32_t setLayoutCount, DescriptorSetLayoutDeclaration *setLayoutDeclarations, uint32_t pushConstRange, ShaderStageFlags pushConstStageFlags);

	private:
		GraphicsPipelineCreateInfo &m_createInfo;
	};

	class ComputePipelineBuilder
	{
	public:
		explicit ComputePipelineBuilder(ComputePipelineCreateInfo &createInfo);
		void setComputeShader(const char *path);
		void setPipelineLayoutDescription(uint32_t setLayoutCount, DescriptorSetLayoutDeclaration *setLayoutDeclarations, uint32_t pushConstRange, ShaderStageFlags pushConstStageFlags);

	private:
		ComputePipelineCreateInfo &m_createInfo;
	};
}