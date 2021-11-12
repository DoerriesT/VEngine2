#include "PostProcessModule.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "graphics/Mesh.h"
#include <EASTL/iterator.h> // eastl::size()
#include "utility/Utility.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "graphics/RenderData.h"

using namespace gal;

namespace
{
	struct PushConsts
	{
		uint32_t resolution[2];
		float texelSize[2];
		float time;
		uint32_t inputImageIndex;
		uint32_t outputImageIndex;
	};
}

PostProcessModule::PostProcessModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	// tonemap
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			{ DescriptorType::TEXTURE, 0, 0, 65536, ShaderStageFlags::PIXEL_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
			{ DescriptorType::RW_TEXTURE, 65536, 0, 65536, ShaderStageFlags::VERTEX_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/tonemap_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(PushConsts), ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_tonemapPipeline);
	}

	// debug normals
	for (size_t skinned = 0; skinned < 2; ++skinned)
	{
		VertexInputAttributeDescription attributeDescs[]
		{
			{ "POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 },
			{ "NORMAL", 1, 1, Format::R32G32B32_SFLOAT, 0 },
			{ "JOINT_INDICES", 2, 2, Format::R16G16B16A16_UINT, 0 },
			{ "JOINT_WEIGHTS", 3, 3, Format::R8G8B8A8_UNORM, 0 },
		};

		VertexInputBindingDescription bindingDescs[]
		{
			{ 0, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 1, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 2, sizeof(uint32_t) * 2, VertexInputRate::VERTEX },
			{ 3, sizeof(uint32_t), VertexInputRate::VERTEX },
		};

		bool isSkinned = skinned != 0;
		const size_t bindingDescCount = isSkinned ? eastl::size(bindingDescs) : eastl::size(bindingDescs) - 2;
		const size_t attributeDescCount = isSkinned ? eastl::size(attributeDescs) : eastl::size(attributeDescs) - 2;

		GraphicsPipelineCreateInfo pipelineCreateInfo{};
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader(isSkinned ? "assets/shaders/debugNormals_skinned_vs" : "assets/shaders/debugNormals_vs");
		builder.setGeometryShader("assets/shaders/debugNormals_gs");
		builder.setFragmentShader("assets/shaders/debugNormals_ps");
		builder.setVertexBindingDescriptions(bindingDescCount, bindingDescs);
		builder.setVertexAttributeDescriptions(attributeDescCount, attributeDescs);
		builder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
		builder.setDepthTest(true, false, CompareOp::GREATER_OR_EQUAL);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
		builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
		builder.setColorAttachmentFormat(Format::B8G8R8A8_UNORM);
		builder.setPolygonModeCullMode(gal::PolygonMode::FILL, gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			{ DescriptorType::STRUCTURED_BUFFER, 262144, 0, 65536, ShaderStageFlags::VERTEX_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT }, // skinning matrices
		};


		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		uint32_t pushConstSize = sizeof(float) * 16;
		pushConstSize += isSkinned ? sizeof(uint32_t) : 0;
		builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, isSkinned ? &m_debugNormalsSkinnedPipeline : &m_debugNormalsPipeline);
	}
}

PostProcessModule::~PostProcessModule() noexcept
{
	m_device->destroyComputePipeline(m_tonemapPipeline);
	m_device->destroyGraphicsPipeline(m_debugNormalsPipeline);
	m_device->destroyGraphicsPipeline(m_debugNormalsSkinnedPipeline);
}

void PostProcessModule::record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept
{
	rg::ResourceUsageDesc tonemapUsageDescs[] =
	{
		{data.m_lightingImageView, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{data.m_resultImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("Post-Process", rg::QueueType::GRAPHICS, eastl::size(tonemapUsageDescs), tonemapUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Post-Process");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Post-Process");
			PROFILING_ZONE_SCOPED;

			cmdList->bindPipeline(m_tonemapPipeline);

			cmdList->bindDescriptorSets(m_tonemapPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.resolution[0] = data.m_width;
			pushConsts.resolution[1] = data.m_height;
			pushConsts.texelSize[0] = 1.0f / data.m_width;
			pushConsts.texelSize[1] = 1.0f / data.m_height;
			pushConsts.time = 0.0f; // TODO
			pushConsts.inputImageIndex = registry.getBindlessHandle(data.m_lightingImageView, DescriptorType::TEXTURE);
			pushConsts.outputImageIndex = registry.getBindlessHandle(data.m_resultImageViewHandle, DescriptorType::RW_TEXTURE);

			cmdList->pushConstants(m_tonemapPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((data.m_width + 7) / 8, (data.m_height + 7) / 8, 1);
		});

	if (data.m_debugNormals)
	{
		rg::ResourceUsageDesc debugNormalsUsageDescs[] =
		{
			{data.m_resultImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
			{data.m_depthBufferImageViewHandle, {ResourceState::READ_DEPTH_STENCIL}},
		};
		graph->addPass("Debug Normals", rg::QueueType::GRAPHICS, eastl::size(debugNormalsUsageDescs), debugNormalsUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Debug Normals");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Debug Normals");
				PROFILING_ZONE_SCOPED;

				ColorAttachmentDescription attachmentDescs[]{{ registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE }};
				DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(data.m_depthBufferImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
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
						uint32_t skinningMatricesBufferIndex;
						float normalsLength;
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
					passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
					passConsts.normalsLength = 0.1f;

					uint64_t allocSize = sizeof(passConsts);
					uint64_t allocOffset = 0;
					auto *mappedPtr = data.m_bufferAllocator->allocate(m_device->getBufferAlignment(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0), &allocSize, &allocOffset);
					memcpy(mappedPtr, &passConsts, sizeof(passConsts));
					uint32_t passConstsAddress = (uint32_t)allocOffset;

					const eastl::vector<SubMeshInstanceData> *instancesArr[]
					{
						&data.m_renderList->m_opaque,
						&data.m_renderList->m_opaqueAlphaTested,
						&data.m_renderList->m_opaqueSkinned,
						&data.m_renderList->m_opaqueSkinnedAlphaTested,
					};
					GraphicsPipeline *pipelines[]
					{
						m_debugNormalsPipeline,
						m_debugNormalsPipeline,
						m_debugNormalsSkinnedPipeline,
						m_debugNormalsSkinnedPipeline,
					};

					GraphicsPipeline *prevPipeline = nullptr;
					for (size_t listType = 0; listType < eastl::size(instancesArr); ++listType)
					{
						if (instancesArr[listType]->empty())
						{
							continue;
						}

						const bool skinned = listType >= 2;
						auto *pipeline = pipelines[listType];

						if (pipeline != prevPipeline)
						{
							cmdList->bindPipeline(pipeline);

							gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
							cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

							prevPipeline = pipeline;
						}

						for (const auto &instance : *instancesArr[listType])
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

							MeshConstants consts{};
							SkinnedMeshConstants skinnedConsts{};

							if (skinned)
							{
								memcpy(skinnedConsts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
								skinnedConsts.skinningMatricesOffset = instance.m_skinningMatricesOffset;
							}
							else
							{
								memcpy(consts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
							}

							cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT, 0, skinned ? sizeof(skinnedConsts) : sizeof(consts), skinned ? (void *)&skinnedConsts : (void *)&consts);


							Buffer *vertexBuffers[]
							{
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
							};

							const uint32_t vertexCount = data.m_meshDrawInfo[instance.m_subMeshHandle].m_vertexCount;

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
								alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize, // joint indices
								alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize + alignedJointIndicesBufferSize, // joint weights
							};

							cmdList->bindIndexBuffer(data.m_meshBufferHandles[instance.m_subMeshHandle].m_indexBuffer, 0, IndexType::UINT16);
							cmdList->bindVertexBuffers(0, skinned ? 4 : 2, vertexBuffers, vertexBufferOffsets);
							cmdList->drawIndexed(data.m_meshDrawInfo[instance.m_subMeshHandle].m_indexCount, 1, 0, 0, 0);
						}
					}
				}
				cmdList->endRenderPass();
			});
	}
}
