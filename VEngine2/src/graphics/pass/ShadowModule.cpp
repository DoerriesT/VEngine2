#include "ShadowModule.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "graphics/Mesh.h"
#include <EASTL/iterator.h> // eastl::size()
#include "utility/Utility.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include <EASTL/fixed_vector.h>
#include "graphics/RenderData.h"

using namespace gal;

ShadowModule::ShadowModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	for (size_t skinned = 0; skinned < 2; ++skinned)
	{
		for (size_t alphaTested = 0; alphaTested < 2; ++alphaTested)
		{
			const bool isSkinned = skinned != 0;
			const bool isAlphaTested = alphaTested != 0;

			uint32_t curVertexBufferIndex = 0;

			eastl::fixed_vector<VertexInputAttributeDescription, 6> attributeDescs;
			eastl::fixed_vector<VertexInputBindingDescription, 6> bindingDescs;

			attributeDescs.push_back({ "POSITION", curVertexBufferIndex, curVertexBufferIndex, Format::R32G32B32_SFLOAT, 0 });
			bindingDescs.push_back({ curVertexBufferIndex, sizeof(float) * 3, VertexInputRate::VERTEX });
			++curVertexBufferIndex;

			if (isAlphaTested)
			{
				attributeDescs.push_back({ "TEXCOORD", curVertexBufferIndex, curVertexBufferIndex, Format::R32G32_SFLOAT, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(float) * 2, VertexInputRate::VERTEX });
				++curVertexBufferIndex;
			}
			if (isSkinned)
			{
				attributeDescs.push_back({ "JOINT_INDICES", curVertexBufferIndex, curVertexBufferIndex, Format::R16G16B16A16_UINT, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(uint32_t) * 2, VertexInputRate::VERTEX });
				++curVertexBufferIndex;

				attributeDescs.push_back({ "JOINT_WEIGHTS", curVertexBufferIndex, curVertexBufferIndex, Format::R8G8B8A8_UNORM, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(uint32_t), VertexInputRate::VERTEX });
				++curVertexBufferIndex;
			}

			GraphicsPipelineCreateInfo pipelineCreateInfo{};
			GraphicsPipelineBuilder builder(pipelineCreateInfo);

			if (isAlphaTested)
			{
				builder.setVertexShader(isSkinned ? "assets/shaders/shadow_skinned_alpha_tested_vs" : "assets/shaders/shadow_alpha_tested_vs");
				builder.setFragmentShader("assets/shaders/shadow_ps");
			}
			else
			{
				builder.setVertexShader(isSkinned ? "assets/shaders/shadow_skinned_vs" : "assets/shaders/shadow_vs");
			}

			builder.setVertexBindingDescriptions(static_cast<uint32_t>(bindingDescs.size()), bindingDescs.data());
			builder.setVertexAttributeDescriptions(static_cast<uint32_t>(attributeDescs.size()), attributeDescs.data());
			builder.setDepthTest(true, true, CompareOp::LESS_OR_EQUAL);
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D16_UNORM);
			builder.setPolygonModeCullMode(gal::PolygonMode::FILL, isAlphaTested ? gal::CullModeFlags::NONE : gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::VERTEX_BIT };
			DescriptorSetLayoutBinding usedBindlessBindings[] =
			{
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1, ShaderStageFlags::VERTEX_BIT), // skinning matrices
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 2, ShaderStageFlags::VERTEX_BIT), // material buffer
			};

			DescriptorSetLayoutDeclaration layoutDecls[]
			{
				{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
				{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
			};

			gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticAnisotropicRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT);

			uint32_t pushConstSize = sizeof(float) * 16;
			pushConstSize += isSkinned ? sizeof(uint32_t) : 0;
			pushConstSize += isAlphaTested ? sizeof(uint32_t) : 0;
			builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT, 1, &staticSamplerDesc, 2);

			GraphicsPipeline *pipeline = nullptr;
			device->createGraphicsPipelines(1, &pipelineCreateInfo, &pipeline);
			if (isSkinned)
			{
				if (isAlphaTested)
				{
					m_shadowSkinnedAlphaTestedPipeline = pipeline;
				}
				else
				{
					m_shadowSkinnedPipeline = pipeline;
				}
			}
			else
			{
				if (isAlphaTested)
				{
					m_shadowAlphaTestedPipeline = pipeline;
				}
				else
				{
					m_shadowPipeline = pipeline;
				}
			}
		}
	}
}

ShadowModule::~ShadowModule() noexcept
{
	m_device->destroyGraphicsPipeline(m_shadowPipeline);
	m_device->destroyGraphicsPipeline(m_shadowSkinnedPipeline);
	m_device->destroyGraphicsPipeline(m_shadowAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_shadowSkinnedAlphaTestedPipeline);
}

void ShadowModule::record(rg::RenderGraph *graph, const Data &data) noexcept
{
	eastl::fixed_vector<rg::ResourceUsageDesc, 32> usageDescs;

	for (size_t i = 0; i < data.m_shadowJobCount; ++i)
	{
		usageDescs.push_back({ data.m_shadowTextureViewHandles[i], {ResourceState::WRITE_DEPTH_STENCIL} });
	}

	graph->addPass("Shadows", rg::QueueType::GRAPHICS, usageDescs.size(), usageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Shadows");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Shadows");
			PROFILING_ZONE_SCOPED;

			for (size_t i = 0; i < data.m_shadowJobCount; ++i)
			{
				DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(data.m_shadowTextureViewHandles[i]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {1.0f} };
				const auto &imageDesc = registry.getImage(data.m_shadowTextureViewHandles[i])->getDescription();
				Rect renderRect{ {0, 0}, {imageDesc.m_width, imageDesc.m_height} };

				cmdList->beginRenderPass(0, nullptr, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)imageDesc.m_width, (float)imageDesc.m_height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {imageDesc.m_width, imageDesc.m_height} };
					cmdList->setScissor(0, 1, &scissor);

					struct PassConstants
					{
						float viewProjectionMatrix[16];
						uint32_t skinningMatricesBufferIndex;
						uint32_t materialBufferIndex;
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &data.m_shadowMatrices[i][0][0], sizeof(passConsts.viewProjectionMatrix));
					passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
					passConsts.materialBufferIndex = data.m_materialsBufferHandle;

					uint32_t passConstsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

					const eastl::vector<SubMeshInstanceData> *instancesArr[]
					{
						&data.m_renderList->m_opaque,
						&data.m_renderList->m_opaqueAlphaTested,
						&data.m_renderList->m_opaqueSkinned,
						&data.m_renderList->m_opaqueSkinnedAlphaTested,
					};
					GraphicsPipeline *pipelines[]
					{
						m_shadowPipeline,
						m_shadowAlphaTestedPipeline,
						m_shadowSkinnedPipeline,
						m_shadowSkinnedAlphaTestedPipeline,
					};

					for (size_t listType = 0; listType < eastl::size(instancesArr); ++listType)
					{
						if (instancesArr[listType]->empty())
						{
							continue;
						}
						const bool skinned = listType >= 2;
						const bool alphaTested = (listType & 1) != 0;
						auto *pipeline = pipelines[listType];

						cmdList->bindPipeline(pipeline);

						gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

						for (const auto &instance : *instancesArr[listType])
						{
							struct MeshConstants
							{
								float modelMatrix[16];
								uint32_t uintData[2];
							};

							MeshConstants consts{};
							memcpy(consts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);

							size_t uintDataOffset = 0;
							if (skinned)
							{
								consts.uintData[uintDataOffset++] = instance.m_skinningMatricesOffset;
							}
							if (alphaTested)
							{
								consts.uintData[uintDataOffset++] = instance.m_materialHandle;
							}

							cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT, 0, static_cast<uint32_t>(sizeof(float) * 16 + sizeof(uint32_t) * uintDataOffset), &consts);


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

							eastl::fixed_vector<uint64_t, 4> vertexBufferOffsets;
							vertexBufferOffsets.push_back(0); // positions
							if (alphaTested)
							{
								vertexBufferOffsets.push_back(alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize); // texcoords
							}
							if (skinned)
							{
								vertexBufferOffsets.push_back(alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize); // joint indices
								vertexBufferOffsets.push_back(alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize + alignedJointIndicesBufferSize); // joint weights
							}

							cmdList->bindIndexBuffer(data.m_meshBufferHandles[instance.m_subMeshHandle].m_indexBuffer, 0, IndexType::UINT16);
							cmdList->bindVertexBuffers(0, static_cast<uint32_t>(vertexBufferOffsets.size()), vertexBuffers, vertexBufferOffsets.data());
							cmdList->drawIndexed(data.m_meshDrawInfo[instance.m_subMeshHandle].m_indexCount, 1, 0, 0, 0);
						}
					}
				}
				cmdList->endRenderPass();
			}
		});
}
