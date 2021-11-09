#include "ForwardModule.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "graphics/Mesh.h"
#include <EASTL/iterator.h> // eastl::size()
#include "utility/Utility.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"

using namespace gal;

ForwardModule::ForwardModule(GraphicsDevice *device, DescriptorSetLayout *offsetBufferSetLayout, DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	Format renderTargetFormats[]
	{
		Format::R16G16B16A16_SFLOAT,
		Format::R8G8B8A8_UNORM,
		Format::R8G8B8A8_SRGB,
		Format::R16G16_SFLOAT
	};
	PipelineColorBlendAttachmentState blendStates[]
	{
		GraphicsPipelineBuilder::s_defaultBlendAttachment,
		GraphicsPipelineBuilder::s_defaultBlendAttachment,
		GraphicsPipelineBuilder::s_defaultBlendAttachment,
		GraphicsPipelineBuilder::s_defaultBlendAttachment,
	};
	const uint32_t renderTargetFormatCount = static_cast<uint32_t>(eastl::size(renderTargetFormats));

	// non-skinned
	{
		VertexInputAttributeDescription attributeDescs[]
		{
			{ "POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 },
			{ "NORMAL", 1, 1, Format::R32G32B32_SFLOAT, 0 },
			{ "TANGENT", 2, 2, Format::R32G32B32A32_SFLOAT, 0 },
			{ "TEXCOORD", 3, 3, Format::R32G32_SFLOAT, 0 },
		};

		VertexInputBindingDescription bindingDescs[]
		{
			{ 0, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 1, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 2, sizeof(float) * 4, VertexInputRate::VERTEX },
			{ 3, sizeof(float) * 2, VertexInputRate::VERTEX },
		};

		GraphicsPipelineCreateInfo pipelineCreateInfo{};
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/forward_vs");
		builder.setFragmentShader("assets/shaders/forward_ps");
		builder.setVertexBindingDescriptions(eastl::size(bindingDescs), bindingDescs);
		builder.setVertexAttributeDescriptions((uint32_t)eastl::size(attributeDescs), attributeDescs);
		builder.setColorBlendAttachments(renderTargetFormatCount, blendStates);
		builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
		builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
		builder.setColorAttachmentFormats(renderTargetFormatCount, renderTargetFormats);

		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			{ DescriptorType::TEXTURE, 0, 0, 65536, ShaderStageFlags::PIXEL_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
			{ DescriptorType::STRUCTURED_BUFFER, 262144, 1, 65536, ShaderStageFlags::VERTEX_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
		};


		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		builder.setPipelineLayoutDescription(2, layoutDecls, sizeof(float) * 16, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_forwardPipeline);
	}

	// skinned
	{
		VertexInputAttributeDescription attributeDescs[]
		{
			{ "POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 },
			{ "NORMAL", 1, 1, Format::R32G32B32_SFLOAT, 0 },
			{ "TANGENT", 2, 2, Format::R32G32B32A32_SFLOAT, 0 },
			{ "TEXCOORD", 3, 3, Format::R32G32_SFLOAT, 0 },
			{ "JOINT_INDICES", 4, 4, Format::R16G16B16A16_UINT, 0 },
			{ "JOINT_WEIGHTS", 5, 5, Format::R8G8B8A8_UNORM, 0 },
		};

		VertexInputBindingDescription bindingDescs[]
		{
			{ 0, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 1, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 2, sizeof(float) * 4, VertexInputRate::VERTEX },
			{ 3, sizeof(float) * 2, VertexInputRate::VERTEX },
			{ 4, sizeof(uint32_t) * 2, VertexInputRate::VERTEX },
			{ 5, sizeof(uint32_t), VertexInputRate::VERTEX },
		};

		GraphicsPipelineCreateInfo pipelineCreateInfo{};
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/forward_skinned_vs");
		builder.setFragmentShader("assets/shaders/forward_skinned_ps");
		builder.setVertexBindingDescriptions(eastl::size(bindingDescs), bindingDescs);
		builder.setVertexAttributeDescriptions((uint32_t)eastl::size(attributeDescs), attributeDescs);
		builder.setColorBlendAttachments(renderTargetFormatCount, blendStates);
		builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
		builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
		builder.setColorAttachmentFormats(renderTargetFormatCount, renderTargetFormats);

		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			{ DescriptorType::TEXTURE, 0, 0, 65536, ShaderStageFlags::PIXEL_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
			{ DescriptorType::STRUCTURED_BUFFER, 262144, 1, 65536, ShaderStageFlags::VERTEX_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
		};


		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		builder.setPipelineLayoutDescription(2, layoutDecls, sizeof(float) * 16 + sizeof(uint32_t), ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_forwardSkinnedPipeline);
	}
}

ForwardModule::~ForwardModule() noexcept
{
	m_device->destroyGraphicsPipeline(m_forwardPipeline);
	m_device->destroyGraphicsPipeline(m_forwardSkinnedPipeline);
}

void ForwardModule::record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept
{
	// depth buffer
	rg::ResourceHandle depthBufferImageHandle = graph->createImage(rg::ImageDesc::create("Depth Buffer", Format::D32_SFLOAT, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle depthBufferImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Depth Buffer", depthBufferImageHandle, graph));
	resultData->m_depthBufferImageViewHandle = depthBufferImageViewHandle;

	// lighting texture
	rg::ResourceHandle lightingImageHandle = graph->createImage(rg::ImageDesc::create("Lighting Image", Format::R16G16B16A16_SFLOAT, ImageUsageFlags::COLOR_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle lightingImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Lighting Image", lightingImageHandle, graph));
	resultData->m_lightingImageViewHandle = lightingImageViewHandle;

	// normal/roughness
	rg::ResourceHandle normalImageHandle = graph->createImage(rg::ImageDesc::create("Normal/Roughness Image", Format::R8G8B8A8_UNORM, ImageUsageFlags::COLOR_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle normalImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Normal/Roughness Image", normalImageHandle, graph));
	resultData->m_normalRoughnessImageViewHandle = normalImageViewHandle;

	// albedo/metalness
	rg::ResourceHandle albedoImageHandle = graph->createImage(rg::ImageDesc::create("Albedo/Metalness Image", Format::R8G8B8A8_SRGB, ImageUsageFlags::COLOR_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle albedoImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Albedo/Metalness Image", albedoImageHandle, graph));
	resultData->m_albedoMetalnessImageViewHandle = albedoImageViewHandle;

	// velocity
	rg::ResourceHandle velocityImageHandle = graph->createImage(rg::ImageDesc::create("Velocity Image", Format::R16G16_SFLOAT, ImageUsageFlags::COLOR_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle velocityImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Velocity Image", velocityImageHandle, graph));
	resultData->m_velocityImageViewHandle = velocityImageViewHandle;

	rg::ResourceUsageDesc usageDescs[] =
	{
		{lightingImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
		{normalImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
		{albedoImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
		{velocityImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
		{depthBufferImageViewHandle, {ResourceState::WRITE_DEPTH_STENCIL}},
	};
	graph->addPass("Mesh", rg::QueueType::GRAPHICS, eastl::size(usageDescs), usageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "MeshPass");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "MeshPass");
			PROFILING_ZONE_SCOPED;

			ClearColorValue clearColor{};
			clearColor.m_float32[0] = 1.0f;
			clearColor.m_float32[1] = 1.0f;
			clearColor.m_float32[2] = 1.0f;
			clearColor.m_float32[3] = 1.0f;

			ColorAttachmentDescription attachmentDescs[]
			{
				 { registry.getImageView(lightingImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, clearColor },
				 { registry.getImageView(normalImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
				 { registry.getImageView(albedoImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
				 { registry.getImageView(velocityImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
			};
			DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(depthBufferImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE };
			Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

			cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, false);
			{
				Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
				cmdList->setViewport(0, 1, &viewport);
				Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
				cmdList->setScissor(0, 1, &scissor);

				struct PassConstants
				{
					float viewProjectionMatrix[16];
					float cameraPosition[3];
					uint32_t skinningMatricesBufferIndex;
				};

				PassConstants passConsts;
				memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
				memcpy(passConsts.cameraPosition, &data.m_cameraPosition[0], sizeof(passConsts.cameraPosition));
				passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferIndex;

				uint64_t allocSize = sizeof(passConsts);
				uint64_t allocOffset = 0;
				auto *mappedPtr = data.m_bufferAllocator->allocate(m_device->getBufferAlignment(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0), &allocSize, &allocOffset);
				memcpy(mappedPtr, &passConsts, sizeof(passConsts));
				uint32_t passConstsAddress = (uint32_t)allocOffset;
				

				for (size_t skinned = 0; skinned < 2; ++skinned)
				{
					auto *pipeline = skinned ? m_forwardSkinnedPipeline : m_forwardPipeline;
					cmdList->bindPipeline(pipeline);

					gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

					const size_t meshCount = skinned ? data.m_skinnedMeshCount : data.m_meshCount;
					for (size_t i = 0; i < meshCount; ++i)
					{
						struct MeshConstants
						{
							float modelMatrix[16];
						};

						struct SkinnedMeshConstants
						{
							float modelMatrix[16];
							uint32_t skinningMatricesOffset;
						};

						const size_t curMeshOffset = skinned ? i + data.m_meshCount : i;

						MeshConstants consts{};
						SkinnedMeshConstants skinnedConsts{};

						if (skinned)
						{
							memcpy(skinnedConsts.modelMatrix, &data.m_modelMatrices[curMeshOffset][0][0], sizeof(float) * 16);
							skinnedConsts.skinningMatricesOffset = data.m_skinningMatrixOffsets[i];
						}
						else
						{
							memcpy(consts.modelMatrix, &data.m_modelMatrices[curMeshOffset][0][0], sizeof(float) * 16);
						}

						cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, skinned ? sizeof(skinnedConsts) : sizeof(consts), skinned ? (void *)&skinnedConsts : (void *)&consts);


						Buffer *vertexBuffers[]
						{
							data.m_meshBufferHandles[curMeshOffset].m_vertexBuffer,
							data.m_meshBufferHandles[curMeshOffset].m_vertexBuffer,
							data.m_meshBufferHandles[curMeshOffset].m_vertexBuffer,
							data.m_meshBufferHandles[curMeshOffset].m_vertexBuffer,
							data.m_meshBufferHandles[curMeshOffset].m_vertexBuffer,
							data.m_meshBufferHandles[curMeshOffset].m_vertexBuffer,
						};

						const uint32_t vertexCount = data.m_meshDrawInfo[curMeshOffset].m_vertexCount;

						const size_t alignedPositionsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
						const size_t alignedNormalsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
						const size_t alignedTangentsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 4, sizeof(float) * 4);
						const size_t alignedTexCoordsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 2, sizeof(float) * 4);
						const size_t alignedJointIndicesBufferSize = util::alignUp<size_t>(vertexCount * sizeof(uint32_t) * 2, sizeof(float) * 4);
						const size_t alignedJointWeightsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(uint32_t), sizeof(float) * 4);

						uint64_t vertexBufferOffsets[]
						{
							0, // positions
							alignedPositionsBufferSize, // normals
							alignedPositionsBufferSize + alignedNormalsBufferSize, // tangents
							alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize, // texcoords
							alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize, // joint indices
							alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize + alignedJointIndicesBufferSize, // joint weights
						};

						cmdList->bindIndexBuffer(data.m_meshBufferHandles[curMeshOffset].m_indexBuffer, 0, IndexType::UINT16);
						cmdList->bindVertexBuffers(0, skinned ? 6 : 4, vertexBuffers, vertexBufferOffsets);
						cmdList->drawIndexed(data.m_meshDrawInfo[curMeshOffset].m_indexCount, 1, 0, 0, 0);
					}
				}
			}
			cmdList->endRenderPass();
		});
}
