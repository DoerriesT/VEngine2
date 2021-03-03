#include "GridPass.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include <optick.h>

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
	OPTICK_GPU_EVENT("GridPass");
	struct GridConstants
	{
		float modelMatrix[16];
		float viewProjectionMatrix[16];
		float thinLineColor[4];
		float thickLineColor[4];
		float cameraPos[3];
		float cellSize;
		float gridNormal[3];
		float gridSize;
	};

	glm::vec3 gridNormal = glm::normalize(glm::vec3(data.m_modelMatrix[1]));
	GridConstants consts{};
	memcpy(consts.modelMatrix, &data.m_modelMatrix, sizeof(consts.modelMatrix));
	memcpy(consts.viewProjectionMatrix, &data.m_viewProjectionMatrix, sizeof(consts.viewProjectionMatrix));
	memcpy(consts.thinLineColor, &data.m_thinLineColor, sizeof(consts.thinLineColor));
	memcpy(consts.thickLineColor, &data.m_thickLineColor, sizeof(consts.thickLineColor));
	memcpy(consts.cameraPos, &data.m_cameraPos, sizeof(consts.cameraPos));
	consts.cellSize = data.m_cellSize;
	memcpy(consts.gridNormal, &gridNormal, sizeof(consts.gridNormal));
	consts.gridSize = data.m_gridSize;

	uint64_t allocSize = sizeof(consts);
	uint64_t allocOffset = 0;
	auto *mappedPtr = data.m_bufferAllocator->allocate(m_device->getBufferAlignment(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0), &allocSize, &allocOffset);
	memcpy(mappedPtr, &consts, sizeof(consts));

	gal::ClearColorValue clearColor{};
	clearColor.m_float32[0] = 1.0f;
	clearColor.m_float32[1] = 1.0f;
	clearColor.m_float32[2] = 1.0f;
	clearColor.m_float32[3] = 1.0f;

	gal::ColorAttachmentDescription attachmentDesc{ data.m_colorAttachment, gal::AttachmentLoadOp::CLEAR, gal::AttachmentStoreOp::STORE, clearColor };
	gal::Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

	cmdList->beginRenderPass(1, &attachmentDesc, nullptr, renderRect, false);
	{
		cmdList->bindPipeline(m_pipeline);

		gal::Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
		cmdList->setViewport(0, 1, &viewport);
		gal::Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
		cmdList->setScissor(0, 1, &scissor);
		
		uint32_t allocOffset32 = (uint32_t)allocOffset;
		cmdList->bindDescriptorSets(m_pipeline, 0, 1, &data.m_offsetBufferSet, 1, &allocOffset32);
		
		cmdList->draw(6, 1, 0, 0);
	}
	cmdList->endRenderPass();
}
