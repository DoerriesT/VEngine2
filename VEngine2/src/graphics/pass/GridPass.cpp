#include "GridPass.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"

GridPass::GridPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout)
	:m_device(device)
{
	gal::PipelineColorBlendAttachmentState blendState{};
	blendState.m_blendEnable = true;
	blendState.m_srcColorBlendFactor = gal::BlendFactor::SRC_ALPHA;
	blendState.m_dstColorBlendFactor = gal::BlendFactor::ONE_MINUS_SRC_ALPHA;
	blendState.m_colorBlendOp = gal::BlendOp::ADD;
	blendState.m_srcAlphaBlendFactor = gal::BlendFactor::ZERO;
	blendState.m_dstAlphaBlendFactor = gal::BlendFactor::ONE;
	blendState.m_alphaBlendOp = gal::BlendOp::ADD;
	blendState.m_colorWriteMask = gal::ColorComponentFlags::ALL_BITS;

	gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
	gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
	builder.setVertexShader("assets/shaders/grid_vs");
	builder.setFragmentShader("assets/shaders/grid_ps");
	builder.setColorBlendAttachment(blendState);
	builder.setDepthTest(true, false, gal::CompareOp::GREATER_OR_EQUAL);
	builder.setDynamicState(gal::DynamicStateFlags::VIEWPORT_BIT | gal::DynamicStateFlags::SCISSOR_BIT);
	builder.setDepthStencilAttachmentFormat(gal::Format::D32_SFLOAT);
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

void GridPass::record(rg::RenderGraph *graph, const Data &data)
{
	rg::ResourceUsageDesc usageDescs[] =
	{
		{data.m_colorAttachment, {gal::ResourceState::WRITE_COLOR_ATTACHMENT}},
		{data.m_depthBufferAttachment, {gal::ResourceState::WRITE_DEPTH_STENCIL}},
	};
	graph->addPass("Grid", rg::QueueType::GRAPHICS, eastl::size(usageDescs), usageDescs, [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Grid");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "GridPass");
			PROFILING_ZONE_SCOPED;

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

			uint32_t allocOffset = (uint32_t)data.m_bufferAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, consts);

			gal::ColorAttachmentDescription attachmentDesc{ registry.getImageView(data.m_colorAttachment), gal::AttachmentLoadOp::LOAD, gal::AttachmentStoreOp::STORE };
			gal::DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(data.m_depthBufferAttachment), gal::AttachmentLoadOp::LOAD, gal::AttachmentStoreOp::STORE, gal::AttachmentLoadOp::DONT_CARE, gal::AttachmentStoreOp::DONT_CARE };
			gal::Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

			cmdList->beginRenderPass(1, &attachmentDesc, &depthBufferDesc, renderRect, false);
			{
				cmdList->bindPipeline(m_pipeline);

				gal::Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
				cmdList->setViewport(0, 1, &viewport);
				gal::Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
				cmdList->setScissor(0, 1, &scissor);

				cmdList->bindDescriptorSets(m_pipeline, 0, 1, &data.m_offsetBufferSet, 1, &allocOffset);

				cmdList->draw(6, 1, 0, 0);
			}
			cmdList->endRenderPass();
		});
}
