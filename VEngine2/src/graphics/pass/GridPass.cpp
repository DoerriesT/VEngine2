#include "GridPass.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"

namespace
{
	struct GridConstants
	{
		glm::mat4 modelMatrix;
		glm::mat4 viewProjectionMatrix;
		glm::vec4 thinLineColor;
		glm::vec4 thickLineColor;
		glm::vec3 cameraPos;
		float cellSize;
		glm::vec3 gridNormal;
		float gridSize;
	};
}

GridPass::GridPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout)
	:m_device(device)
{
	gal::PipelineColorBlendAttachmentState blendState{};
	blendState.m_blendEnable = true;
	blendState.m_srcColorBlendFactor = gal::BlendFactor::SRC_ALPHA;
	blendState.m_dstColorBlendFactor = gal::BlendFactor::ONE_MINUS_SRC_ALPHA;
	blendState.m_colorBlendOp = gal::BlendOp::ADD;
	blendState.m_srcAlphaBlendFactor = gal::BlendFactor::SRC_ALPHA;
	blendState.m_dstAlphaBlendFactor = gal::BlendFactor::ONE_MINUS_SRC_ALPHA;
	blendState.m_alphaBlendOp = gal::BlendOp::ADD;
	blendState.m_colorWriteMask = gal::ColorComponentFlags::ALL_BITS;

	gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
	gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
	builder.setVertexShader("assets/shaders/grid_vs");
	builder.setFragmentShader("assets/shaders/grid_ps");
	builder.setColorBlendAttachment(blendState);
	builder.setDynamicState(gal::DynamicStateFlags::VIEWPORT_BIT | gal::DynamicStateFlags::SCISSOR_BIT);
	//builder.setDepthStencilAttachmentFormat(gal::Format::D32_SFLOAT);
	builder.setColorAttachmentFormat(gal::Format::B8G8R8A8_UNORM);
	
	gal::DescriptorSetLayoutBinding usedBinding{ gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, gal::ShaderStageFlags::ALL_STAGES };
	gal::DescriptorSetLayoutDeclaration layoutDecl{ offsetBufferSetLayout, 1, &usedBinding };
	builder.setPipelineLayoutDescription(1, &layoutDecl, 0, {}, 0, nullptr, 0);

	device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_pipeline);
}

GridPass::~GridPass()
{
	m_device->destroyGraphicsPipeline(m_pipeline);
}

void GridPass::record(gal::CommandList *cmdList, const Data &data)
{
	GridConstants consts{};
	consts.modelMatrix = data.m_modelMatrix;
	consts.viewProjectionMatrix = data.m_viewProjectionMatrix;
	consts.thinLineColor = data.m_thinLineColor;
	consts.thickLineColor = data.m_thickLineColor;
	consts.cameraPos = data.m_cameraPos;
	consts.cellSize = data.m_cellSize;
	consts.gridNormal = glm::normalize(glm::vec3(data.m_modelMatrix[1]));
	consts.gridSize = data.m_gridSize;

	uint64_t allocSize = sizeof(consts);
	uint64_t allocOffset = 0;
	auto *mappedPtr = data.m_bufferAllocator->allocate(m_device->getBufferAlignment(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0), &allocSize, &allocOffset);
	memcpy(mappedPtr, &consts, sizeof(consts));

	gal::ColorAttachmentDescription attachmentDesc{ data.m_colorAttachment, gal::AttachmentLoadOp::CLEAR, gal::AttachmentStoreOp::STORE };
	gal::Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

	cmdList->beginRenderPass(1, &attachmentDesc, nullptr, renderRect, false);
	{
		cmdList->bindPipeline(m_pipeline);

		gal::Viewport viewport{}
		cmdList->setViewport(0, 1, )
		
		uint32_t allocOffset32 = (uint32_t)allocOffset;
		cmdList->bindDescriptorSets(m_pipeline, 0, 1, &data.m_offsetBufferSet, 1, &allocOffset32);
		
		cmdList->draw(6, 1, 0, 0);
	}
	cmdList->endRenderPass();
}
