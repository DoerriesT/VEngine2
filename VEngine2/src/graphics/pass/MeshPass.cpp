#include "MeshPass.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "graphics/Mesh.h"
#include <EASTL/iterator.h> // eastl::size()
#include <optick.h>
#include "utility/Utility.h"

MeshPass::MeshPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout)
	:m_device(device)
{
	gal::VertexInputAttributeDescription attributeDescs[]
	{
		{ "POSITION", 0, 0, gal::Format::R32G32B32_SFLOAT, 0 },
		{ "NORMAL", 1, 1, gal::Format::R32G32B32_SFLOAT, 0 },
		{ "TANGENT", 2, 2, gal::Format::R32G32B32A32_SFLOAT, 0 },
		{ "TEXCOORD", 3, 3, gal::Format::R32G32_SFLOAT, 0 },
	};

	gal::VertexInputBindingDescription bindingDescs[]
	{
		{ 0, sizeof(float) * 3, gal::VertexInputRate::VERTEX },
		{ 1, sizeof(float) * 3, gal::VertexInputRate::VERTEX },
		{ 2, sizeof(float) * 4, gal::VertexInputRate::VERTEX },
		{ 3, sizeof(float) * 2, gal::VertexInputRate::VERTEX },
	};

	gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
	gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
	builder.setVertexShader("assets/shaders/mesh_vs");
	builder.setFragmentShader("assets/shaders/mesh_ps");
	builder.setVertexBindingDescriptions(eastl::size(bindingDescs), bindingDescs);
	builder.setVertexAttributeDescriptions((uint32_t)eastl::size(attributeDescs), attributeDescs);
	builder.setColorBlendAttachment(gal::GraphicsPipelineBuilder::s_defaultBlendAttachment);
	builder.setDepthTest(true, true, gal::CompareOp::GREATER_OR_EQUAL);
	builder.setDynamicState(gal::DynamicStateFlags::VIEWPORT_BIT | gal::DynamicStateFlags::SCISSOR_BIT);
	builder.setDepthStencilAttachmentFormat(gal::Format::D32_SFLOAT);
	builder.setColorAttachmentFormat(gal::Format::B8G8R8A8_UNORM);

	gal::DescriptorSetLayoutBinding usedOffsetBufferBinding = { gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, gal::ShaderStageFlags::ALL_STAGES };
	gal::DescriptorSetLayoutBinding usedBindlessBindings = { gal::DescriptorType::TEXTURE, 0, 0, 65536, gal::ShaderStageFlags::PIXEL_BIT, gal::DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | gal::DescriptorBindingFlags::PARTIALLY_BOUND_BIT };


	gal::DescriptorSetLayoutDeclaration layoutDecls[]
	{
		{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
		{ bindlessSetLayout, 1, &usedBindlessBindings },
	};

	builder.setPipelineLayoutDescription(2, layoutDecls, 0, (gal::ShaderStageFlags)0, 0, nullptr, -1);

	device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_pipeline);
}

MeshPass::~MeshPass()
{
	m_device->destroyGraphicsPipeline(m_pipeline);
}

void MeshPass::record(gal::CommandList *cmdList, const Data &data)
{
	OPTICK_GPU_EVENT("MeshPass");
	cmdList->beginDebugLabel("Mesh");

	gal::ClearColorValue clearColor{};
	clearColor.m_float32[0] = 1.0f;
	clearColor.m_float32[1] = 1.0f;
	clearColor.m_float32[2] = 1.0f;
	clearColor.m_float32[3] = 1.0f;

	gal::ColorAttachmentDescription attachmentDesc{ data.m_colorAttachment, gal::AttachmentLoadOp::CLEAR, gal::AttachmentStoreOp::STORE, clearColor };
	gal::DepthStencilAttachmentDescription depthBufferDesc{data.m_depthBufferAttachment, gal::AttachmentLoadOp::CLEAR, gal::AttachmentStoreOp::STORE, gal::AttachmentLoadOp::DONT_CARE, gal::AttachmentStoreOp::DONT_CARE };
	gal::Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

	cmdList->beginRenderPass(1, &attachmentDesc, &depthBufferDesc, renderRect, false);
	{
		cmdList->bindPipeline(m_pipeline);

		gal::Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
		cmdList->setViewport(0, 1, &viewport);
		gal::Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
		cmdList->setScissor(0, 1, &scissor);


		for (size_t i = 0; i < data.m_meshCount; ++i)
		{
			struct MeshConstants
			{
				float modelMatrix[16];
				float viewProjectionMatrix[16];
			};

			MeshConstants consts{};
			memcpy(consts.modelMatrix, &data.m_modelMatrices[i][0][0], sizeof(float) * 16);
			memcpy(consts.viewProjectionMatrix, &data.m_viewProjectionMatrix, sizeof(consts.viewProjectionMatrix));

			uint64_t allocSize = sizeof(consts);
			uint64_t allocOffset = 0;
			auto *mappedPtr = data.m_bufferAllocator->allocate(m_device->getBufferAlignment(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0), &allocSize, &allocOffset);
			memcpy(mappedPtr, &consts, sizeof(consts));

			gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
			uint32_t allocOffset32 = (uint32_t)allocOffset;
			cmdList->bindDescriptorSets(m_pipeline, 0, 2, sets, 1, &allocOffset32);

			cmdList->bindIndexBuffer(data.m_meshBufferHandles[i].m_indexBuffer, 0, gal::IndexType::UINT16);

			gal::Buffer *vertexBuffers[]
			{
				data.m_meshBufferHandles[i].m_vertexBuffer,
				data.m_meshBufferHandles[i].m_vertexBuffer,
				data.m_meshBufferHandles[i].m_vertexBuffer,
				data.m_meshBufferHandles[i].m_vertexBuffer,
			};

			const uint32_t vertexCount = data.m_meshDrawInfo[i].m_vertexCount;

			const size_t alignedPositionsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
			const size_t alignedNormalsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
			const size_t alignedTangentsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 4, sizeof(float) * 4);
			const size_t alignedTexCoordsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 2, sizeof(float) * 4);

			uint64_t vertexBufferOffsets[]
			{
				0, // positions
				alignedPositionsBufferSize, // normals
				alignedPositionsBufferSize + alignedNormalsBufferSize, // tangents
				alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize, // texcoords
			};

			cmdList->bindVertexBuffers(0, 4, vertexBuffers, vertexBufferOffsets);

			cmdList->drawIndexed(data.m_meshDrawInfo[i].m_indexCount, 1, 0, 0, 0);
		}
	}
	cmdList->endRenderPass();

	cmdList->endDebugLabel();
}
