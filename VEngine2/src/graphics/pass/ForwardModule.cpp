#include "ForwardModule.h"
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
#include "graphics/RendererResources.h"
#include <glm/trigonometric.hpp>
#include "VolumetricFogModule.h"
#include "graphics/LightManager.h"

using namespace gal;

ForwardModule::ForwardModule(GraphicsDevice *device, DescriptorSetLayout *offsetBufferSetLayout, DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	// prepass
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
				builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
				builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
				builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
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
						m_depthPrepassSkinnedAlphaTestedPipeline = pipeline;
					}
					else
					{
						m_depthPrepassSkinnedPipeline = pipeline;
					}
				}
				else
				{
					if (isAlphaTested)
					{
						m_depthPrepassAlphaTestedPipeline = pipeline;
					}
					else
					{
						m_depthPrepassPipeline = pipeline;
					}
				}
			}
		}
	}

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

	// forward
	{
		for (size_t skinned = 0; skinned < 2; ++skinned)
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

			bool isSkinned = skinned != 0;
			const size_t bindingDescCount = isSkinned ? eastl::size(bindingDescs) : eastl::size(bindingDescs) - 2;
			const size_t attributeDescCount = isSkinned ? eastl::size(attributeDescs) : eastl::size(attributeDescs) - 2;

			GraphicsPipelineCreateInfo pipelineCreateInfo{};
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader(isSkinned ? "assets/shaders/forward_skinned_vs" : "assets/shaders/forward_vs");
			builder.setFragmentShader("assets/shaders/forward_ps");
			builder.setVertexBindingDescriptions(bindingDescCount, bindingDescs);
			builder.setVertexAttributeDescriptions(attributeDescCount, attributeDescs);
			builder.setColorBlendAttachments(renderTargetFormatCount, blendStates);
			builder.setDepthTest(true, false, CompareOp::EQUAL);
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
			builder.setColorAttachmentFormats(renderTargetFormatCount, renderTargetFormats);
			//builder.setPolygonModeCullMode(gal::PolygonMode::FILL, gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
			DescriptorSetLayoutBinding usedBindlessBindings[] =
			{
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1, ShaderStageFlags::VERTEX_BIT), // skinning matrices
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 2, ShaderStageFlags::PIXEL_BIT), // material buffer
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 3, ShaderStageFlags::PIXEL_BIT), // directional lights
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 4, ShaderStageFlags::PIXEL_BIT), // shadowed directional lights
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 5, ShaderStageFlags::PIXEL_BIT), // array textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 6, ShaderStageFlags::PIXEL_BIT), // exposure buffer, depth bins buffers
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_BYTE_BUFFER, 7, ShaderStageFlags::PIXEL_BIT), // picking buffer
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 8, ShaderStageFlags::PIXEL_BIT), // punctual lights
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 9, ShaderStageFlags::PIXEL_BIT), // punctual lights shadowed
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 10, ShaderStageFlags::PIXEL_BIT), // array textures UI
			};

			DescriptorSetLayoutDeclaration layoutDecls[]
			{
				{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
				{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
			};

			gal::StaticSamplerDescription staticSamplerDescs[]
			{
				gal::Initializers::staticAnisotropicRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT),
				gal::Initializers::staticLinearClampSampler(1, 0, gal::ShaderStageFlags::PIXEL_BIT),
				gal::Initializers::staticLinearClampSampler(2, 0, gal::ShaderStageFlags::PIXEL_BIT),
			};
			staticSamplerDescs[2].m_compareEnable = true;
			staticSamplerDescs[2].m_compareOp = CompareOp::GREATER_OR_EQUAL;

			uint32_t pushConstSize = sizeof(float) * 16 + sizeof(uint32_t) * 2;
			pushConstSize += isSkinned ? sizeof(uint32_t) : 0;
			builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, static_cast<uint32_t>(eastl::size(staticSamplerDescs)), staticSamplerDescs, 2);

			device->createGraphicsPipelines(1, &pipelineCreateInfo, isSkinned ? &m_forwardSkinnedPipeline : &m_forwardPipeline);
		}
	}

	// sky
	{
		gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
		gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/sky_vs");
		builder.setFragmentShader("assets/shaders/sky_ps");
		builder.setColorBlendAttachments(renderTargetFormatCount, blendStates);
		builder.setDepthTest(true, false, CompareOp::EQUAL);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
		builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
		builder.setColorAttachmentFormats(renderTargetFormatCount, renderTargetFormats);

		DescriptorSetLayoutBinding usedBindlessBindings[]
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 0, ShaderStageFlags::PIXEL_BIT), // exposure buffer
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		uint32_t pushConstSize = sizeof(float) * 16 + sizeof(uint32_t);
		builder.setPipelineLayoutDescription(1, layoutDecls, pushConstSize, ShaderStageFlags::ALL_STAGES, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_skyPipeline);
	}

	// gtao
	{
		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 0),
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		gal::StaticSamplerDescription staticSamplerDescs[]
		{
			gal::Initializers::staticLinearClampSampler(0, 0, gal::ShaderStageFlags::ALL_STAGES),
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/gtao_cs");
		builder.setPipelineLayoutDescription((uint32_t)eastl::size(layoutDecls), layoutDecls, 0, ShaderStageFlags::ALL_STAGES, 1, staticSamplerDescs, 2);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_gtaoPipeline);
	}

	// gtao blur
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 0),
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		gal::StaticSamplerDescription staticSamplerDescs[]
		{
			gal::Initializers::staticLinearClampSampler(0, 0, gal::ShaderStageFlags::ALL_STAGES),
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/gtaoBlur_cs");

		uint32_t pushConstSize = sizeof(uint32_t) * 6;
		builder.setPipelineLayoutDescription((uint32_t)eastl::size(layoutDecls), layoutDecls, pushConstSize, ShaderStageFlags::ALL_STAGES, 1, staticSamplerDescs, 1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_gtaoBlurPipeline);
	}

	// indirect lighting
	{
		gal::PipelineColorBlendAttachmentState additiveBlendState{};
		additiveBlendState.m_blendEnable = true;
		additiveBlendState.m_srcColorBlendFactor = gal::BlendFactor::ONE;
		additiveBlendState.m_dstColorBlendFactor = gal::BlendFactor::ONE;
		additiveBlendState.m_colorBlendOp = gal::BlendOp::ADD;
		additiveBlendState.m_srcAlphaBlendFactor = gal::BlendFactor::ZERO;
		additiveBlendState.m_dstAlphaBlendFactor = gal::BlendFactor::ONE;
		additiveBlendState.m_alphaBlendOp = gal::BlendOp::ADD;
		additiveBlendState.m_colorWriteMask = gal::ColorComponentFlags::ALL_BITS;

		gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
		gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/fullscreenTriangle_vs");
		builder.setFragmentShader("assets/shaders/indirectLighting_ps");
		builder.setColorBlendAttachment(additiveBlendState);
		builder.setDepthTest(true, false, CompareOp::NOT_EQUAL);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
		builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
		builder.setColorAttachmentFormat(Format::R16G16B16A16_SFLOAT);

		DescriptorSetLayoutBinding usedBindlessBindings[]
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 1, ShaderStageFlags::PIXEL_BIT), // 3d textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 2, ShaderStageFlags::PIXEL_BIT), // exposure buffer
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		gal::StaticSamplerDescription staticSamplerDescs[]
		{
			gal::Initializers::staticLinearClampSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT),
		};

		uint32_t pushConstSize = 48;
		builder.setPipelineLayoutDescription(1, layoutDecls, pushConstSize, ShaderStageFlags::PIXEL_BIT, 1, staticSamplerDescs, 1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_indirectLightingPipeline);
	}

	m_volumetricFogModule = new VolumetricFogModule(m_device, offsetBufferSetLayout, bindlessSetLayout);
}

ForwardModule::~ForwardModule() noexcept
{
	m_device->destroyGraphicsPipeline(m_depthPrepassPipeline);
	m_device->destroyGraphicsPipeline(m_depthPrepassSkinnedPipeline);
	m_device->destroyGraphicsPipeline(m_depthPrepassAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_depthPrepassSkinnedAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_forwardPipeline);
	m_device->destroyGraphicsPipeline(m_forwardSkinnedPipeline);
	m_device->destroyGraphicsPipeline(m_skyPipeline);
	m_device->destroyComputePipeline(m_gtaoPipeline);
	m_device->destroyComputePipeline(m_gtaoBlurPipeline);
	m_device->destroyGraphicsPipeline(m_indirectLightingPipeline);
	delete m_volumetricFogModule;
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

	// gtao
	rg::ResourceHandle gtaoImageHandle = graph->createImage(rg::ImageDesc::create("GTAO Image", Format::R16G16_SFLOAT, ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle gtaoImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("GTAO Image", gtaoImageHandle, graph));

	// gtao result
	rg::ResourceHandle gtaoResultImageHandle = graph->createImage(rg::ImageDesc::create("GTAO Result Image", Format::R16G16_SFLOAT, ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
	rg::ResourceViewHandle gtaoResultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("GTAO Result Image", gtaoResultImageHandle, graph));

	// depth prepass
	rg::ResourceUsageDesc prepassUsageDescs[] = { {depthBufferImageViewHandle, {ResourceState::WRITE_DEPTH_STENCIL}} };
	graph->addPass("Depth Prepass", rg::QueueType::GRAPHICS, eastl::size(prepassUsageDescs), prepassUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Depth Prepass");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Depth Prepass");
			PROFILING_ZONE_SCOPED;

			DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(depthBufferImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE };
			Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

			cmdList->beginRenderPass(0, nullptr, &depthBufferDesc, renderRect, false);
			{
				Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
				cmdList->setViewport(0, 1, &viewport);
				Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
				cmdList->setScissor(0, 1, &scissor);

				struct PassConstants
				{
					float viewProjectionMatrix[16];
					uint32_t skinningMatricesBufferIndex;
					uint32_t materialBufferIndex;
				};

				PassConstants passConsts;
				memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
				passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
				passConsts.materialBufferIndex = data.m_materialsBufferHandle;

				uint32_t passConstsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

				const eastl::vector<SubMeshInstanceData> *instancesArr[]
				{
					&data.m_renderList->m_opaque,
					&data.m_renderList->m_opaqueAlphaTested,
					&data.m_renderList->m_opaqueSkinned,
					&data.m_renderList->m_opaqueSkinnedAlphaTested,
				};
				GraphicsPipeline *pipelines[]
				{
					m_depthPrepassPipeline,
					m_depthPrepassAlphaTestedPipeline,
					m_depthPrepassSkinnedPipeline,
					m_depthPrepassSkinnedAlphaTestedPipeline,
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
		});

	VolumetricFogModule::ResultData volumetricFogResultData{};
	VolumetricFogModule::Data volumetricFogData{};
	volumetricFogData.m_profilingCtx = data.m_profilingCtx;
	volumetricFogData.m_bufferAllocator = data.m_bufferAllocator;
	volumetricFogData.m_offsetBufferSet = data.m_offsetBufferSet;
	volumetricFogData.m_bindlessSet = data.m_bindlessSet;
	volumetricFogData.m_width = data.m_width;
	volumetricFogData.m_height = data.m_height;
	volumetricFogData.m_frame = data.m_frame;
	volumetricFogData.m_fovy = data.m_fovy;
	volumetricFogData.m_nearPlane = data.m_nearPlane;
	volumetricFogData.m_globalMediaBufferHandle = data.m_globalMediaBufferHandle;
	volumetricFogData.m_localMediaBufferHandle = {}; // TODO;
	volumetricFogData.m_localMediaDepthBinsBufferHandle = {}; // TODO
	volumetricFogData.m_localMediaTileTextureViewHandle = {}; // TODO
	volumetricFogData.m_exposureBufferHandle = data.m_exposureBufferHandle;
	volumetricFogData.m_globalMediaCount = data.m_globalMediaCount;
	volumetricFogData.m_localMediaCount = 0; // TODO
	volumetricFogData.m_viewMatrix = data.m_viewMatrix;
	volumetricFogData.m_cameraPosition = data.m_cameraPosition;
	volumetricFogData.m_ignoreHistory = data.m_frame < 2;
	volumetricFogData.m_lightRecordData = data.m_lightRecordData;

	m_volumetricFogModule->record(graph, volumetricFogData, &volumetricFogResultData);

	// forward
	eastl::fixed_vector<rg::ResourceUsageDesc, 32 + 5> forwardUsageDescs;
	forwardUsageDescs.push_back({ lightingImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} });
	forwardUsageDescs.push_back({ normalImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} });
	forwardUsageDescs.push_back({ albedoImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} });
	forwardUsageDescs.push_back({ velocityImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} });
	forwardUsageDescs.push_back({ depthBufferImageViewHandle, {ResourceState::READ_DEPTH_STENCIL} });
	forwardUsageDescs.push_back({ data.m_exposureBufferHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
	forwardUsageDescs.push_back({ data.m_pickingBufferHandle, {ResourceState::RW_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
	if (data.m_lightRecordData->m_punctualLightCount > 0)
	{
		forwardUsageDescs.push_back({ data.m_lightRecordData->m_punctualLightsTileTextureViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
	}
	if (data.m_lightRecordData->m_punctualLightShadowedCount > 0)
	{
		forwardUsageDescs.push_back({ data.m_lightRecordData->m_punctualLightsShadowedTileTextureViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
	}
	for (size_t i = 0; i < data.m_lightRecordData->m_shadowMapViewHandleCount; ++i)
	{
		forwardUsageDescs.push_back({ data.m_lightRecordData->m_shadowMapViewHandles[i], {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
	}

	graph->addPass("Forward", rg::QueueType::GRAPHICS, forwardUsageDescs.size(), forwardUsageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Forward");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Forward");
			PROFILING_ZONE_SCOPED;

			ColorAttachmentDescription attachmentDescs[]
			{
				 { registry.getImageView(lightingImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE },
				 { registry.getImageView(normalImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE },
				 { registry.getImageView(albedoImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE },
				 { registry.getImageView(velocityImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE },
			};
			DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(depthBufferImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
			Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

			cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, true);
			{
				Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
				cmdList->setViewport(0, 1, &viewport);
				Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
				cmdList->setScissor(0, 1, &scissor);

				struct PassConstants
				{
					float viewProjectionMatrix[16];
					float viewMatrixDepthRow[4];
					float cameraPosition[3];
					uint32_t skinningMatricesBufferIndex;
					uint32_t materialBufferIndex;
					uint32_t directionalLightCount;
					uint32_t directionalLightBufferIndex;
					uint32_t directionalLightShadowedCount;
					uint32_t directionalLightShadowedBufferIndex;
					uint32_t punctualLightCount;
					uint32_t punctualLightBufferIndex;
					uint32_t punctualLightTileTextureIndex;
					uint32_t punctualLightDepthBinsBufferIndex;
					uint32_t punctualLightShadowedCount;
					uint32_t punctualLightShadowedBufferIndex;
					uint32_t punctualLightShadowedTileTextureIndex;
					uint32_t punctualLightShadowedDepthBinsBufferIndex;
					uint32_t exposureBufferIndex;
					uint32_t pickingBufferIndex;
					uint32_t pickingPosX;
					uint32_t pickingPosY;
				};

				PassConstants passConsts{};
				memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
				passConsts.viewMatrixDepthRow[0] = data.m_viewMatrix[0][2];
				passConsts.viewMatrixDepthRow[1] = data.m_viewMatrix[1][2];
				passConsts.viewMatrixDepthRow[2] = data.m_viewMatrix[2][2];
				passConsts.viewMatrixDepthRow[3] = data.m_viewMatrix[3][2];
				memcpy(passConsts.cameraPosition, &data.m_cameraPosition[0], sizeof(passConsts.cameraPosition));
				passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
				passConsts.materialBufferIndex = data.m_materialsBufferHandle;
				passConsts.directionalLightCount = data.m_lightRecordData->m_directionalLightCount;
				passConsts.directionalLightBufferIndex = data.m_lightRecordData->m_directionalLightsBufferViewHandle;
				passConsts.directionalLightShadowedCount = data.m_lightRecordData->m_directionalLightShadowedCount;
				passConsts.directionalLightShadowedBufferIndex = data.m_lightRecordData->m_directionalLightsShadowedBufferViewHandle;
				passConsts.punctualLightCount = data.m_lightRecordData->m_punctualLightCount;
				passConsts.punctualLightBufferIndex = data.m_lightRecordData->m_punctualLightsBufferViewHandle;
				passConsts.punctualLightTileTextureIndex = data.m_lightRecordData->m_punctualLightCount > 0 ? registry.getBindlessHandle(data.m_lightRecordData->m_punctualLightsTileTextureViewHandle, DescriptorType::TEXTURE) : 0;
				passConsts.punctualLightDepthBinsBufferIndex = data.m_lightRecordData->m_punctualLightsDepthBinsBufferViewHandle;
				passConsts.punctualLightShadowedCount = data.m_lightRecordData->m_punctualLightShadowedCount;
				passConsts.punctualLightShadowedBufferIndex = data.m_lightRecordData->m_punctualLightsShadowedBufferViewHandle;
				passConsts.punctualLightShadowedTileTextureIndex = data.m_lightRecordData->m_punctualLightShadowedCount > 0 ? registry.getBindlessHandle(data.m_lightRecordData->m_punctualLightsShadowedTileTextureViewHandle, DescriptorType::TEXTURE) : 0;
				passConsts.punctualLightShadowedDepthBinsBufferIndex = data.m_lightRecordData->m_punctualLightsShadowedDepthBinsBufferViewHandle;
				passConsts.exposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferHandle, DescriptorType::BYTE_BUFFER);
				passConsts.pickingBufferIndex = registry.getBindlessHandle(data.m_pickingBufferHandle, DescriptorType::RW_BYTE_BUFFER);
				passConsts.pickingPosX = data.m_pickingPosX;
				passConsts.pickingPosY = data.m_pickingPosY;

				uint32_t passConstsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

				const eastl::vector<SubMeshInstanceData> *instancesArr[]
				{
					&data.m_renderList->m_opaque,
					&data.m_renderList->m_opaqueAlphaTested,
					&data.m_renderList->m_opaqueSkinned,
					&data.m_renderList->m_opaqueSkinnedAlphaTested,
				};
				GraphicsPipeline *pipelines[]
				{
					m_forwardPipeline,
					m_forwardPipeline,
					m_forwardSkinnedPipeline,
					m_forwardSkinnedPipeline,
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
							uint32_t materialIndex;
							uint32_t entityID;
						};

						struct SkinnedMeshConstants
						{
							float modelMatrix[16];
							uint32_t materialIndex;
							uint32_t entityID;
							uint32_t skinningMatricesOffset;
						};

						MeshConstants consts{};
						SkinnedMeshConstants skinnedConsts{};

						if (skinned)
						{
							memcpy(skinnedConsts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
							skinnedConsts.materialIndex = instance.m_materialHandle;
							skinnedConsts.entityID = static_cast<uint32_t>(instance.m_entityID);
							skinnedConsts.skinningMatricesOffset = instance.m_skinningMatricesOffset;
						}
						else
						{
							memcpy(consts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
							consts.materialIndex = instance.m_materialHandle;
							consts.entityID = static_cast<uint32_t>(instance.m_entityID);
						}

						cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, skinned ? sizeof(skinnedConsts) : sizeof(consts), skinned ? (void *)&skinnedConsts : (void *)&consts);


						Buffer *vertexBuffers[]
						{
							data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
							data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
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
							alignedPositionsBufferSize + alignedNormalsBufferSize, // tangents
							alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize, // texcoords
							alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize, // joint indices
							alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize + alignedJointIndicesBufferSize, // joint weights
						};

						cmdList->bindIndexBuffer(data.m_meshBufferHandles[instance.m_subMeshHandle].m_indexBuffer, 0, IndexType::UINT16);
						cmdList->bindVertexBuffers(0, skinned ? 6 : 4, vertexBuffers, vertexBufferOffsets);
						cmdList->drawIndexed(data.m_meshDrawInfo[instance.m_subMeshHandle].m_indexCount, 1, 0, 0, 0);
					}
				}

				// sky
				{
					struct SkyPushConsts
					{
						float invModelViewProjection[16];
						uint32_t exposureBufferIndex;
					};

					cmdList->bindPipeline(m_skyPipeline);
					cmdList->bindDescriptorSets(m_skyPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

					SkyPushConsts skyPushConsts{};
					memcpy(skyPushConsts.invModelViewProjection, &data.m_invViewProjectionMatrix[0][0], sizeof(skyPushConsts.invModelViewProjection));
					skyPushConsts.exposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferHandle, DescriptorType::BYTE_BUFFER);

					cmdList->pushConstants(m_skyPipeline, ShaderStageFlags::ALL_STAGES, 0, sizeof(skyPushConsts), &skyPushConsts);

					cmdList->draw(3, 1, 0, 0);
				}
			}
			cmdList->endRenderPass();
		});

	rg::ResourceUsageDesc gtaoUsageDescs[] =
	{
		{normalImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{depthBufferImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{gtaoImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("GTAO", rg::QueueType::GRAPHICS, eastl::size(gtaoUsageDescs), gtaoUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "GTAO");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "GTAO");
			PROFILING_ZONE_SCOPED;

			struct GTAOConstants
			{
				glm::mat4 viewMatrix;
				glm::mat4 invProjectionMatrix;
				uint32_t resolution[2];
				float texelSize[2];
				float radiusScale;
				float falloffScale;
				float falloffBias;
				float maxTexelRadius;
				int sampleCount;
				uint32_t frame;
				uint32_t depthTextureIndex;
				uint32_t normalTextureIndex;
				uint32_t resultTextureIndex;
			};

			const float radius = 2.0f;

			GTAOConstants consts{};
			consts.viewMatrix = data.m_viewMatrix;
			consts.invProjectionMatrix = data.m_invProjectionMatrix;
			consts.resolution[0] = data.m_width;
			consts.resolution[1] = data.m_height;
			consts.texelSize[0] = 1.0f / data.m_width;
			consts.texelSize[1] = 1.0f / data.m_height;
			consts.radiusScale = 0.5f * radius * (1.0f / tanf(glm::radians(data.m_fovy) * 0.5f) * (data.m_height / static_cast<float>(data.m_width)));
			consts.falloffScale = 1.0f / radius;
			consts.falloffBias = -0.75f;
			consts.maxTexelRadius = 256.0f;
			consts.sampleCount = 12;
			consts.frame = 0;// data.m_frame;
			consts.depthTextureIndex = registry.getBindlessHandle(depthBufferImageViewHandle, DescriptorType::TEXTURE);
			consts.normalTextureIndex = registry.getBindlessHandle(normalImageViewHandle, DescriptorType::TEXTURE);
			consts.resultTextureIndex = registry.getBindlessHandle(gtaoImageViewHandle, DescriptorType::RW_TEXTURE);

			uint32_t constsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, consts);

			cmdList->bindPipeline(m_gtaoPipeline);

			gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
			cmdList->bindDescriptorSets(m_gtaoPipeline, 0, 2, sets, 1, &constsAddress);

			cmdList->dispatch((data.m_width + 7) / 8, (data.m_height + 7) / 8, 1);
		});

	rg::ResourceUsageDesc gtaoBlurUsageDescs[] =
	{
		{gtaoImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{gtaoResultImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("GTAO Blur", rg::QueueType::GRAPHICS, eastl::size(gtaoUsageDescs), gtaoUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "GTAO Blur");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "GTAO Blur");
			PROFILING_ZONE_SCOPED;

			struct GTAOPushConsts
			{
				uint32_t resolution[2];
				float texelSize[2];
				uint32_t inputTextureIndex;
				uint32_t resultTextureIndex;
			};

			GTAOPushConsts consts{};
			consts.resolution[0] = data.m_width;
			consts.resolution[1] = data.m_height;
			consts.texelSize[0] = 1.0f / data.m_width;
			consts.texelSize[1] = 1.0f / data.m_height;
			consts.inputTextureIndex = registry.getBindlessHandle(gtaoImageViewHandle, DescriptorType::TEXTURE);
			consts.resultTextureIndex = registry.getBindlessHandle(gtaoResultImageViewHandle, DescriptorType::RW_TEXTURE);

			cmdList->bindPipeline(m_gtaoBlurPipeline);

			cmdList->bindDescriptorSets(m_gtaoBlurPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

			cmdList->pushConstants(m_gtaoBlurPipeline, ShaderStageFlags::ALL_STAGES, 0, sizeof(consts), &consts);

			cmdList->dispatch((data.m_width + 7) / 8, (data.m_height + 7) / 8, 1);
		});

	rg::ResourceUsageDesc indirectLightingUsageDescs[]
	{
		{ albedoImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
		{ gtaoResultImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
		{ data.m_exposureBufferHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
		{ volumetricFogResultData.m_volumetricFogImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
		{ lightingImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} },
		{ depthBufferImageViewHandle, {ResourceState::READ_DEPTH_STENCIL | ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
	};
	graph->addPass("Indirect Lighting", rg::QueueType::GRAPHICS, eastl::size(indirectLightingUsageDescs), indirectLightingUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Indirect Lighting");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Indirect Lighting");
			PROFILING_ZONE_SCOPED;

			ColorAttachmentDescription attachmentDescs[]
			{
				 { registry.getImageView(lightingImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE },
			};
			DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(depthBufferImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
			Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

			cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, true);
			{
				cmdList->bindPipeline(m_indirectLightingPipeline);

				gal::Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
				cmdList->setViewport(0, 1, &viewport);
				gal::Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
				cmdList->setScissor(0, 1, &scissor);

				cmdList->bindDescriptorSets(m_indirectLightingPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

				struct PushConsts
				{
					glm::vec3 volumetricFogTexelSize;
					float volumetricFogNear;
					float depthUnprojectParams[2];
					float volumetricFogFar;
					uint32_t exposureBufferIndex;
					uint32_t albedoTextureIndex;
					uint32_t gtaoTextureIndex;
					uint32_t depthBufferIndex;
					uint32_t volumetricFogTextureIndex;
				};

				PushConsts pushConsts{};
				pushConsts.volumetricFogTexelSize = 1.0f / glm::vec3(240.0f, 135.0f, 128.0f);
				pushConsts.volumetricFogNear = 0.5f;
				pushConsts.volumetricFogFar = 64.0f;
				pushConsts.depthUnprojectParams[0] = data.m_invProjectionMatrix[2][3];
				pushConsts.depthUnprojectParams[1] = data.m_invProjectionMatrix[3][3];
				pushConsts.exposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferHandle, DescriptorType::BYTE_BUFFER);
				pushConsts.albedoTextureIndex = registry.getBindlessHandle(albedoImageViewHandle, DescriptorType::TEXTURE);
				pushConsts.gtaoTextureIndex = registry.getBindlessHandle(gtaoResultImageViewHandle, DescriptorType::TEXTURE);
				pushConsts.depthBufferIndex = registry.getBindlessHandle(depthBufferImageViewHandle, DescriptorType::TEXTURE);
				pushConsts.volumetricFogTextureIndex = registry.getBindlessHandle(volumetricFogResultData.m_volumetricFogImageViewHandle, DescriptorType::TEXTURE);

				cmdList->pushConstants(m_indirectLightingPipeline, ShaderStageFlags::PIXEL_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->draw(3, 1, 0, 0);
			}
			cmdList->endRenderPass();
		});
}
