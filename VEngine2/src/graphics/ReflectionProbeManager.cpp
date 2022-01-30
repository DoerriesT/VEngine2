#include "ReflectionProbeManager.h"
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <EASTL/sort.h>
#include "gal/Initializers.h"
#include "component/ReflectionProbeComponent.h"
#include "component/TransformComponent.h"
#include "component/LightComponent.h"
#include "RenderData.h"
#include "RenderGraph.h"
#include "Mesh.h"
#include "RendererResources.h"
#include "ResourceViewRegistry.h"
#include "LinearGPUBufferAllocator.h"
#include "RendererResources.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "utility/Utility.h"
#include "ecs/ECS.h"
#include "LightManager.h"

#define A_CPU
#include <ffx_a.h>
#include <ffx_spd.h>

using namespace gal;

namespace
{
	struct ProbeFilterPushConsts
	{
		uint32_t resolution;
		float texelSize;
		float mip0Resolution;
		float roughness;
		float mipCount;
		uint32_t copyOnly;
		uint32_t inputTextureIndex;
		uint32_t resultTextureIndex;
	};
}

ReflectionProbeManager::ReflectionProbeManager(gal::GraphicsDevice *device, RendererResources *renderResources, ResourceViewRegistry *viewRegistry) noexcept
	:m_device(device),
	m_rendererResources(renderResources),
	m_viewRegistry(viewRegistry)
{
	for (uint32_t i = 0; i < k_cacheSize; ++i)
	{
		m_freeCacheSlots.push_back(static_cast<uint32_t>(k_cacheSize) - 1 - i);
	}
	// gbuffer
	{
		Format renderTargetFormats[]
		{
			Format::R8G8B8A8_SRGB,
			Format::R16G16B16A16_SFLOAT,
		};
		PipelineColorBlendAttachmentState blendStates[]
		{
			GraphicsPipelineBuilder::s_defaultBlendAttachment,
			GraphicsPipelineBuilder::s_defaultBlendAttachment,
		};
		const uint32_t renderTargetFormatCount = static_cast<uint32_t>(eastl::size(renderTargetFormats));

		for (size_t alphaTested = 0; alphaTested < 2; ++alphaTested)
		{
			const bool isAlphaTested = alphaTested != 0;

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

			const size_t bindingDescCount = eastl::size(bindingDescs);
			const size_t attributeDescCount = eastl::size(attributeDescs);

			GraphicsPipelineCreateInfo pipelineCreateInfo{};
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("assets/shaders/reflectionProbeGBuffer_vs");
			builder.setFragmentShader(isAlphaTested ? "assets/shaders/reflectionProbeGBuffer_alpha_tested_ps" : "assets/shaders/reflectionProbeGBuffer_ps");
			builder.setVertexBindingDescriptions(bindingDescCount, bindingDescs);
			builder.setVertexAttributeDescriptions(attributeDescCount, attributeDescs);
			builder.setColorBlendAttachments(renderTargetFormatCount, blendStates);
			builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
			builder.setColorAttachmentFormats(renderTargetFormatCount, renderTargetFormats);
			//builder.setPolygonModeCullMode(gal::PolygonMode::FILL, gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
			DescriptorSetLayoutBinding usedBindlessBindings[] =
			{
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1, ShaderStageFlags::VERTEX_BIT), // model matrices
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 2, ShaderStageFlags::PIXEL_BIT), // material buffer
			};

			DescriptorSetLayoutDeclaration layoutDecls[]
			{
				{ renderResources->m_offsetBufferDescriptorSetLayout, 1, &usedOffsetBufferBinding },
				{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
			};

			gal::StaticSamplerDescription staticSamplerDescs[]
			{
				gal::Initializers::staticAnisotropicRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT)
			};

			uint32_t pushConstSize = sizeof(uint32_t) * 2;
			builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 1, staticSamplerDescs, 2);

			device->createGraphicsPipelines(1, &pipelineCreateInfo, isAlphaTested ? &m_probeGbufferAlphaTestedPipeline : &m_probeGbufferPipeline);
		}
	}

	// relight
	{
		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::COMPUTE_BIT };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1), // directional lights
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 2), // textures 2d
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 3), // irradiance volumes
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 4), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ renderResources->m_offsetBufferDescriptorSetLayout, 1, &usedOffsetBufferBinding },
			{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};
		gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticLinearClampSampler(0, 0, ShaderStageFlags::COMPUTE_BIT);

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/reflectionProbeRelight_cs");
		builder.setPipelineLayoutDescription(2, layoutDecls, 0, ShaderStageFlags::COMPUTE_BIT, 1, &staticSamplerDesc, 2);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_probeRelightPipeline);
	}

	// ffx downsample
	{
		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::COMPUTE_BIT };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // 2d textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 1), // 2d rw_textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 2), // 2d rw_textures (globallycoherent)
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 3), // array textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 4), // array rw_textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 5), // array rw_textures (globallycoherent)
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_BYTE_BUFFER, 6), // counter buffer
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ renderResources->m_offsetBufferDescriptorSetLayout, 1, &usedOffsetBufferBinding },
			{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};
		gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticLinearClampSampler(0, 0, ShaderStageFlags::COMPUTE_BIT);

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/fidelityFxDownsample_cs");
		builder.setPipelineLayoutDescription(2, layoutDecls, 0, ShaderStageFlags::COMPUTE_BIT, 1, &staticSamplerDesc, 2);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_ffxDownsamplePipeline);
	}

	// filter
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // cube input tex
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 1), // array input tex
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 2), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};
		gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticLinearClampSampler(0, 0, ShaderStageFlags::COMPUTE_BIT);

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/reflectionProbeFilter_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(ProbeFilterPushConsts), ShaderStageFlags::COMPUTE_BIT, 1, &staticSamplerDesc, 1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_probeFilterPipeline);
	}

	// probe images
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = k_resolution;
		imageCreateInfo.m_height = k_resolution;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 6 * k_cacheSize;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_createFlags = {};

		// final array
		{
			imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::RW_TEXTURE_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = { 0.0f, 0.0f, 0.0f, 0.0f };
			imageCreateInfo.m_createFlags = ImageCreateFlags::CUBE_COMPATIBLE_BIT;
			imageCreateInfo.m_levels = k_numMips;

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_probeArrayImage);
			m_device->setDebugObjectName(ObjectType::IMAGE, m_probeArrayImage, "Reflection Probe Array Image");

			// reset
			imageCreateInfo.m_createFlags = {};
			imageCreateInfo.m_levels = 1;

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * k_cacheSize;

				m_device->createImageView(viewCreateInfo, &m_probeArrayView);
				m_probeArrayRWTextureViewHandle = renderResources->m_resourceViewRegistry->createRWTextureViewHandle(m_probeArrayView);
			}

			// cube array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::CUBE_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = k_numMips;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * k_cacheSize;

				m_device->createImageView(viewCreateInfo, &m_probeArrayCubeView);
				m_probeArrayCubeTextureViewHandle = renderResources->m_resourceViewRegistry->createTextureViewHandle(m_probeArrayCubeView);
			}

			// mip views
			for (uint32_t i = 0; i < k_cacheSize; ++i)
			{
				for (uint32_t j = 0; j < k_numMips; ++j)
				{
					gal::ImageViewCreateInfo viewCreateInfo{};
					viewCreateInfo.m_image = m_probeArrayImage;
					viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
					viewCreateInfo.m_format = imageCreateInfo.m_format;
					viewCreateInfo.m_baseMipLevel = j;
					viewCreateInfo.m_levelCount = 1;
					viewCreateInfo.m_baseArrayLayer = i * 6;
					viewCreateInfo.m_layerCount = 6;

					m_device->createImageView(viewCreateInfo, &m_probeArrayMipViews[i][j]);
					m_probeArrayMipRWTextureViewHandles[i][j] = renderResources->m_resourceViewRegistry->createRWTextureViewHandle(m_probeArrayMipViews[i][j]);
				}
			}
		}

		// albedo
		{
			imageCreateInfo.m_format = Format::R8G8B8A8_SRGB;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::COLOR_ATTACHMENT_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = { 0.0f, 0.0f, 0.0f, 0.0f };

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_probeAlbedoRoughnessArrayImage);
			m_device->setDebugObjectName(ObjectType::IMAGE, m_probeAlbedoRoughnessArrayImage, "Reflection Probe Albedo/Roughness Array Image");

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeAlbedoRoughnessArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * k_cacheSize;

				m_device->createImageView(viewCreateInfo, &m_probeAlbedoRoughnessArrayView);
				m_probeAlbedoRoughnessTextureViewHandle = renderResources->m_resourceViewRegistry->createTextureViewHandle(m_probeAlbedoRoughnessArrayView);
			}

			// slice views
			for (uint32_t i = 0; i < 6 * k_cacheSize; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeAlbedoRoughnessArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i;
				viewCreateInfo.m_layerCount = 1;

				m_device->createImageView(viewCreateInfo, &m_probeAlbedoRoughnessSliceViews[i]);
			}
		}

		// normal
		{
			imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::COLOR_ATTACHMENT_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = { 0.0f, 0.0f, 0.0f, 0.0f };

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_probeNormalDepthArrayImage);
			m_device->setDebugObjectName(ObjectType::IMAGE, m_probeAlbedoRoughnessArrayImage, "Reflection Probe Normal/Depth Array Image");

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeNormalDepthArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * k_cacheSize;

				m_device->createImageView(viewCreateInfo, &m_probeNormalDepthArrayView);
				m_probeNormalDepthTextureViewHandle = renderResources->m_resourceViewRegistry->createTextureViewHandle(m_probeNormalDepthArrayView);
			}

			// slice views
			for (uint32_t i = 0; i < 6 * k_cacheSize; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeNormalDepthArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i;
				viewCreateInfo.m_layerCount = 1;

				m_device->createImageView(viewCreateInfo, &m_probeNormalDepthSliceViews[i]);
			}
		}

		// depth
		{
			imageCreateInfo.m_layers = 6;
			imageCreateInfo.m_format = Format::D32_SFLOAT;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = { };

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_probeDepthBufferImage);
			m_device->setDebugObjectName(ObjectType::IMAGE, m_probeAlbedoRoughnessArrayImage, "Reflection Probe Depth Buffer Image");

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeDepthBufferImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6;

				m_device->createImageView(viewCreateInfo, &m_probeDepthArrayView);
			}

			// slice views
			for (uint32_t i = 0; i < 6; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeDepthBufferImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i;
				viewCreateInfo.m_layerCount = 1;

				m_device->createImageView(viewCreateInfo, &m_probeDepthSliceViews[i]);
			}
		}

		// tmp lit
		{
			imageCreateInfo.m_layers = 6;
			imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::RW_TEXTURE_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = { };
			imageCreateInfo.m_levels = k_numPrefilterMips;

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_probeTmpLitImage);
			m_device->setDebugObjectName(ObjectType::IMAGE, m_probeAlbedoRoughnessArrayImage, "Reflection Probe Tmp Lit Image");

			// cube view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeTmpLitImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::CUBE;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = k_numPrefilterMips;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6;

				m_device->createImageView(viewCreateInfo, &m_probeTmpLitCubeView);
				m_probeTmpLitCubeTextureViewHandle = renderResources->m_resourceViewRegistry->createTextureViewHandle(m_probeTmpLitCubeView);
			}

			// mip views
			for (uint32_t i = 0; i < k_numPrefilterMips; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeTmpLitImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = i;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6;

				m_device->createImageView(viewCreateInfo, &m_probeTmpLitArrayViews[i]);
				m_probeTmpLitArrayRWTextureViewHandles[i] = renderResources->m_resourceViewRegistry->createRWTextureViewHandle(m_probeTmpLitArrayViews[i]);
				if (i == 0)
				{
					m_probeTmpLitArrayMip0TextureViewHandle = renderResources->m_resourceViewRegistry->createTextureViewHandle(m_probeTmpLitArrayViews[0]);
				}
			}
		}
	}

	// SPD counter buffer
	{
		BufferCreateInfo createInfo{};
		createInfo.m_size = sizeof(uint32_t) * 6;
		createInfo.m_usageFlags = BufferUsageFlags::RW_BYTE_BUFFER_BIT | BufferUsageFlags::CLEAR_BIT;

		m_device->createBuffer(createInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_spdCounterBuffer);
		m_device->setDebugObjectName(ObjectType::IMAGE, m_probeAlbedoRoughnessArrayImage, "FidelityFX SPD Counter Buffer");

		DescriptorBufferInfo bufferInfo{};
		bufferInfo.m_buffer = m_spdCounterBuffer;
		bufferInfo.m_offset = 0;
		bufferInfo.m_range = sizeof(uint32_t) * 6;
		bufferInfo.m_structureByteStride = 4;
		m_spdCounterBufferViewHandle = renderResources->m_resourceViewRegistry->createRWByteBufferViewHandle(bufferInfo);
	}

	// transition images to expected layouts
	{
		auto *cmdList = renderResources->m_commandList;
		renderResources->m_commandListPool->reset();
		cmdList->begin();
		{
			// clear SPD counter buffer
			{
				gal::Barrier barrier = Initializers::bufferBarrier(m_spdCounterBuffer,
					PipelineStageFlags::TOP_OF_PIPE_BIT,
					PipelineStageFlags::CLEAR_BIT,
					ResourceState::UNDEFINED,
					ResourceState::CLEAR_RESOURCE);
				cmdList->barrier(1, &barrier);

				cmdList->fillBuffer(m_spdCounterBuffer, 0, sizeof(uint32_t) * 6, 0);
			}

			Barrier barriers[]
			{
				Initializers::imageBarrier(m_probeArrayImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::PIXEL_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE, {0, 1, 0, 6 * k_cacheSize}),
				Initializers::imageBarrier(m_probeDepthBufferImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlags::LATE_FRAGMENT_TESTS_BIT, ResourceState::UNDEFINED, ResourceState::WRITE_DEPTH_STENCIL, {0, 1, 0, 6}),
				Initializers::imageBarrier(m_probeAlbedoRoughnessArrayImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE, {0, 1, 0, 6 * k_cacheSize}),
				Initializers::imageBarrier(m_probeNormalDepthArrayImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE, {0, 1, 0, 6 * k_cacheSize}),
				Initializers::imageBarrier(m_probeTmpLitImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE, {0, k_numPrefilterMips, 0, 6}),
				Initializers::bufferBarrier(m_spdCounterBuffer, PipelineStageFlags::CLEAR_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::CLEAR_RESOURCE, ResourceState::RW_RESOURCE),
			};
			cmdList->barrier(sizeof(barriers) / sizeof(barriers[0]), barriers);


		}
		cmdList->end();
		Initializers::submitSingleTimeCommands(m_device->getGraphicsQueue(), cmdList);
	}
}

ReflectionProbeManager::~ReflectionProbeManager() noexcept
{
	m_device->destroyGraphicsPipeline(m_probeGbufferPipeline);
	m_device->destroyGraphicsPipeline(m_probeGbufferAlphaTestedPipeline);
	m_device->destroyComputePipeline(m_probeRelightPipeline);
	m_device->destroyComputePipeline(m_ffxDownsamplePipeline);
	m_device->destroyComputePipeline(m_probeFilterPipeline);

	// probe array
	m_device->destroyImage(m_probeArrayImage);
	m_device->destroyImageView(m_probeArrayCubeView);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeArrayCubeTextureViewHandle);
	m_device->destroyImageView(m_probeArrayView);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeArrayRWTextureViewHandle);
	for (size_t i = 0; i < k_cacheSize; ++i)
	{
		for (size_t j = 0; j < k_numMips; ++j)
		{
			m_device->destroyImageView(m_probeArrayMipViews[i][j]);
			m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeArrayMipRWTextureViewHandles[i][j]);
		}
	}

	// depth buffer
	m_device->destroyImage(m_probeDepthBufferImage);
	m_device->destroyImageView(m_probeDepthArrayView);
	for (auto v : m_probeDepthSliceViews)
	{
		m_device->destroyImageView(v);
	}

	// albedo/roughness
	m_device->destroyImage(m_probeAlbedoRoughnessArrayImage);
	m_device->destroyImageView(m_probeAlbedoRoughnessArrayView);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeAlbedoRoughnessTextureViewHandle);
	for (auto v : m_probeAlbedoRoughnessSliceViews)
	{
		m_device->destroyImageView(v);
	}

	// normal/depth
	m_device->destroyImage(m_probeNormalDepthArrayImage);
	m_device->destroyImageView(m_probeNormalDepthArrayView);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeNormalDepthTextureViewHandle);
	for (auto v : m_probeNormalDepthSliceViews)
	{
		m_device->destroyImageView(v);
	}

	// tmp lit
	m_device->destroyImage(m_probeTmpLitImage);
	m_device->destroyImageView(m_probeTmpLitCubeView);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeTmpLitArrayMip0TextureViewHandle);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_probeTmpLitCubeTextureViewHandle);
	for (auto v : m_probeTmpLitArrayViews)
	{
		m_device->destroyImageView(v);
	}
	for (auto v : m_probeTmpLitArrayRWTextureViewHandles)
	{
		m_rendererResources->m_resourceViewRegistry->destroyHandle(v);
	}

	// spd counter buffer
	m_device->destroyBuffer(m_spdCounterBuffer);
	m_rendererResources->m_resourceViewRegistry->destroyHandle(m_spdCounterBufferViewHandle);
}

void ReflectionProbeManager::update(rg::RenderGraph *graph, const Data &data) noexcept
{
	struct ProbeSortData
	{
		EntityID m_entity;
		TransformComponent *m_transformComp;
		ReflectionProbeComponent *m_probeComp;
		glm::vec3 m_capturePosition;
		float m_nearPlane;
		float m_farPlane;
		float m_cameraDist2;
	};

	// keep track of which slots were legitimately claimed by entities. all missing slots then belong to deleted entitites and we can free them.
	eastl::bitset<k_cacheSize> claimedSlots = 0;
	eastl::vector<ProbeSortData> sortData;

	data.m_ecs->iterate<TransformComponent, ReflectionProbeComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, ReflectionProbeComponent *probeC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &pc = probeC[i];

				if (pc.m_recapture)
				{
					pc.m_rendered = false;
					pc.m_recapture = false;
				}

				glm::vec3 cameraProbePosDiff = *data.m_cameraPosition - tc.m_curRenderTransform.m_translation;

				ProbeSortData probeSortData{};
				probeSortData.m_entity = entities[i];
				probeSortData.m_transformComp = &tc;
				probeSortData.m_probeComp = &pc;
				probeSortData.m_capturePosition = tc.m_curRenderTransform.m_translation + pc.m_captureOffset;
				probeSortData.m_nearPlane = pc.m_nearPlane;
				probeSortData.m_farPlane = pc.m_farPlane;
				probeSortData.m_cameraDist2 = glm::dot(cameraProbePosDiff, cameraProbePosDiff);

				sortData.push_back(probeSortData);

				// check if this entity actually owns the slot and correct the component data if it does not own the slot
				if (pc.m_cacheSlot != UINT32_MAX)
				{
					assert(pc.m_cacheSlot < k_cacheSize);
					if (m_cacheSlotOwners[pc.m_cacheSlot] == entities[i])
					{
						claimedSlots[pc.m_cacheSlot] = true;
					}
					else
					{
						// reset internal data: this entity does not own the slot it claims to own
						pc.m_cacheSlot = UINT32_MAX;
						pc.m_rendered = false;
						pc.m_lastLit = UINT32_MAX;
					}
				}
			}
		});

	// free all unclaimed slots
	for (size_t i = 0; i < k_cacheSize; ++i)
	{
		if (m_cacheSlotOwners[i] != k_nullEntity && !claimedSlots[i])
		{
			freeCacheSlot(static_cast<uint32_t>(i));
		}
	}

	// sort by distance to camera
	eastl::sort(sortData.begin(), sortData.end(), [&](const auto &lhs, const auto &rhs) { return lhs.m_cameraDist2 < rhs.m_cameraDist2; });

	// remove far away probes from cache
	for (size_t i = k_cacheSize; i < sortData.size(); ++i)
	{
		auto &sData = sortData[i];
		const uint32_t cacheSlot = sData.m_probeComp->m_cacheSlot;

		// probe occupies cache slot
		if (cacheSlot != UINT32_MAX)
		{
			// mark slot as free
			freeCacheSlot(cacheSlot);

			// reset internal data
			sData.m_probeComp->m_cacheSlot = UINT32_MAX;
			sData.m_probeComp->m_rendered = false;
			sData.m_probeComp->m_lastLit = UINT32_MAX;
		}
	}

	// add new close probes to the cache and find probe to relight
	const size_t activeProbeCount = eastl::min<size_t>(k_cacheSize, sortData.size());
	const float invFurthestDistance2 = 1.0f / (sortData.empty() ? 0.0f : sortData[activeProbeCount - 1].m_cameraDist2);
	size_t relightProbeIndex = UINT32_MAX;
	size_t renderProbeIndex = UINT32_MAX;
	float bestRelightScore = -1.0f;

	for (size_t i = 0; i < activeProbeCount; ++i)
	{
		auto &sData = sortData[i];
		const uint32_t cacheSlot = sData.m_probeComp->m_cacheSlot;

		// probe not yet in cache
		if (cacheSlot == UINT32_MAX)
		{
			sData.m_probeComp->m_cacheSlot = allocateCacheSlot(sData.m_entity);
		}

		// evaluate relighting score
		const uint32_t lastLit = sData.m_probeComp->m_lastLit;
		const float distanceScore = (1.0f - (sData.m_cameraDist2 * invFurthestDistance2)) * 0.5f + 0.5f;
		const float timeScore = lastLit == UINT32_MAX ? 9999999.0f : static_cast<float>(m_internalFrame - lastLit);
		const float relightScore = distanceScore * timeScore;

		if (relightScore > bestRelightScore)
		{
			bestRelightScore = relightScore;
			relightProbeIndex = i;
		}

		// render gbuffer of closest unrendered probe
		if (renderProbeIndex == UINT32_MAX && !sData.m_probeComp->m_rendered)
		{
			renderProbeIndex = i;
		}
	}

	// if we need to render a probe, we also light it directly
	relightProbeIndex = renderProbeIndex != UINT32_MAX ? renderProbeIndex : relightProbeIndex;


	// schedule rendering a probe gbuffer
	if (renderProbeIndex != UINT32_MAX)
	{
		auto &sData = sortData[renderProbeIndex];
		sData.m_probeComp->m_rendered = true;

		glm::mat4 viewMatrices[6];
		viewMatrices[0] = glm::lookAtLH(sData.m_capturePosition, sData.m_capturePosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		viewMatrices[1] = glm::lookAtLH(sData.m_capturePosition, sData.m_capturePosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		viewMatrices[2] = glm::lookAtLH(sData.m_capturePosition, sData.m_capturePosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		viewMatrices[3] = glm::lookAtLH(sData.m_capturePosition, sData.m_capturePosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		viewMatrices[4] = glm::lookAtLH(sData.m_capturePosition, sData.m_capturePosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		viewMatrices[5] = glm::lookAtLH(sData.m_capturePosition, sData.m_capturePosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		glm::mat4 projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, sData.m_farPlane, sData.m_nearPlane);
		for (size_t i = 0; i < 6; ++i)
		{
			m_currentRenderProbeViewProjMatrices[i] = projection * viewMatrices[i];
		}

		renderProbeGBuffer(graph, data, sData.m_probeComp->m_cacheSlot);
	}

	// relight probe
	if (relightProbeIndex != UINT32_MAX)
	{
		auto &sData = sortData[relightProbeIndex];
		// keep track of the frame where the probe was lit
		sData.m_probeComp->m_lastLit = m_internalFrame;

		m_currentRelightProbePosition = sData.m_capturePosition;
		m_currentRelightProbeNearPlane = sData.m_nearPlane;
		m_currentRelightProbeFarPlane = sData.m_farPlane;

		m_currentRelightProbeWorldToLocalTransposed =
			glm::transpose(glm::scale(1.0f / sData.m_transformComp->m_curRenderTransform.m_scale)
				* glm::mat4_cast(glm::inverse(sData.m_transformComp->m_curRenderTransform.m_rotation))
				* glm::translate(-sData.m_transformComp->m_curRenderTransform.m_translation));

		relightProbe(graph, data, sData.m_probeComp->m_cacheSlot);
	}

	// create buffer for probe data
	ReflectionProbeGPU *reflectionProbesBufferPtr = nullptr;
	{
		// prepare DescriptorBufferInfo
		DescriptorBufferInfo bufferInfo = Initializers::structuredBufferInfo(sizeof(ReflectionProbeGPU), k_cacheSize);
		bufferInfo.m_buffer = data.m_shaderResourceLinearAllocator->getBuffer();

		// allocate memory
		const uint64_t alignment = m_device->getBufferAlignment(DescriptorType::STRUCTURED_BUFFER, sizeof(ReflectionProbeGPU));
		uint8_t *bufferPtr = data.m_shaderResourceLinearAllocator->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);
		reflectionProbesBufferPtr = reinterpret_cast<ReflectionProbeGPU *>(bufferPtr);

		// create a transient bindless handle
		m_reflectionProbesBufferHandle = m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true);
	}

	// build probe list for frame
	for (size_t i = 0; i < activeProbeCount; ++i)
	{
		auto &sData = sortData[i];

		// only add rendered probes to the list. (every rendered probe is guaranteed to have a lit result in the cache)
		if (!sData.m_probeComp->m_rendered)
		{
			continue;
		}

		// TODO: frustum cull

		glm::mat4 worldToLocalTransposed =
			glm::transpose(glm::scale(1.0f / sData.m_transformComp->m_curRenderTransform.m_scale)
				* glm::mat4_cast(glm::inverse(sData.m_transformComp->m_curRenderTransform.m_rotation))
				* glm::translate(-sData.m_transformComp->m_curRenderTransform.m_translation));

		ReflectionProbeGPU probe{};
		probe.m_worldToLocal0 = worldToLocalTransposed[0];
		probe.m_worldToLocal1 = worldToLocalTransposed[1];
		probe.m_worldToLocal2 = worldToLocalTransposed[2];
		probe.m_capturePosition = sData.m_capturePosition;
		probe.m_arraySlot = static_cast<float>(sData.m_probeComp->m_cacheSlot);
		probe.m_boxInvFadeDist0 = 1.0f / (eastl::clamp(sData.m_probeComp->m_boxFadeDistances[0], 1e-5f, sData.m_transformComp->m_curRenderTransform.m_scale.x) / sData.m_transformComp->m_curRenderTransform.m_scale.x);
		probe.m_boxInvFadeDist1 = 1.0f / (eastl::clamp(sData.m_probeComp->m_boxFadeDistances[1], 1e-5f, sData.m_transformComp->m_curRenderTransform.m_scale.x) / sData.m_transformComp->m_curRenderTransform.m_scale.x);
		probe.m_boxInvFadeDist2 = 1.0f / (eastl::clamp(sData.m_probeComp->m_boxFadeDistances[2], 1e-5f, sData.m_transformComp->m_curRenderTransform.m_scale.y) / sData.m_transformComp->m_curRenderTransform.m_scale.y);
		probe.m_boxInvFadeDist3 = 1.0f / (eastl::clamp(sData.m_probeComp->m_boxFadeDistances[3], 1e-5f, sData.m_transformComp->m_curRenderTransform.m_scale.y) / sData.m_transformComp->m_curRenderTransform.m_scale.y);
		probe.m_boxInvFadeDist4 = 1.0f / (eastl::clamp(sData.m_probeComp->m_boxFadeDistances[4], 1e-5f, sData.m_transformComp->m_curRenderTransform.m_scale.z) / sData.m_transformComp->m_curRenderTransform.m_scale.z);
		probe.m_boxInvFadeDist5 = 1.0f / (eastl::clamp(sData.m_probeComp->m_boxFadeDistances[5], 1e-5f, sData.m_transformComp->m_curRenderTransform.m_scale.z) / sData.m_transformComp->m_curRenderTransform.m_scale.z);

		*reflectionProbesBufferPtr = probe;
		++reflectionProbesBufferPtr;
	}

	m_reflectionProbeCount = static_cast<uint32_t>(activeProbeCount);

	++m_internalFrame;
}

TextureViewHandle ReflectionProbeManager::getReflectionProbeArrayTextureViewHandle() const noexcept
{
	return m_probeArrayCubeTextureViewHandle;
}

StructuredBufferViewHandle ReflectionProbeManager::getReflectionProbeDataBufferhandle() const noexcept
{
	return m_reflectionProbesBufferHandle;
}

uint32_t ReflectionProbeManager::getReflectionProbeCount() const noexcept
{
	return m_reflectionProbeCount;
}

uint32_t ReflectionProbeManager::allocateCacheSlot(EntityID entity) noexcept
{
	assert(!m_freeCacheSlots.empty());
	uint32_t slot = m_freeCacheSlots.back();
	m_freeCacheSlots.pop_back();
	m_cacheSlotOwners[slot] = entity;
	return slot;
}

void ReflectionProbeManager::freeCacheSlot(uint32_t slot) noexcept
{
	if (slot < k_cacheSize && m_cacheSlotOwners[slot] != k_nullEntity)
	{
		assert(m_freeCacheSlots.size() < k_cacheSize);
		m_freeCacheSlots.push_back(slot);
		m_cacheSlotOwners[slot] = k_nullEntity;
	}
}

void ReflectionProbeManager::renderProbeGBuffer(rg::RenderGraph *graph, const Data &data, size_t probeIdx) const noexcept
{
	graph->addPass("Reflection Probe GBuffer", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Reflection Probe GBuffer");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_gpuProfilingCtx, cmdList, "Reflection Probe GBuffer");
			PROFILING_ZONE_SCOPED;

			constexpr uint32_t resolution = static_cast<uint32_t>(k_resolution);

			// transition images to correct state for writing in this pass
			{
				gal::Barrier barriers[2];
				{
					barriers[0] = Initializers::imageBarrier(m_probeAlbedoRoughnessArrayImage,
						PipelineStageFlags::COMPUTE_SHADER_BIT,
						PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
						ResourceState::READ_RESOURCE,
						ResourceState::WRITE_COLOR_ATTACHMENT,
						{ 0, 1, (uint32_t)probeIdx * 6, 6 });

					barriers[1] = Initializers::imageBarrier(m_probeNormalDepthArrayImage,
						PipelineStageFlags::COMPUTE_SHADER_BIT,
						PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
						ResourceState::READ_RESOURCE,
						ResourceState::WRITE_COLOR_ATTACHMENT,
						{ 0, 1, (uint32_t)probeIdx * 6, 6 });
				}
				cmdList->barrier(2, barriers);
			}

			for (size_t face = 0; face < 6; ++face)
			{
				ColorAttachmentDescription attachmentDescs[]
				{
					 { m_probeAlbedoRoughnessSliceViews[probeIdx * 6 + face], AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
					 { m_probeNormalDepthSliceViews[probeIdx * 6 + face], AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
				};
				DepthStencilAttachmentDescription depthBufferDesc{ m_probeDepthSliceViews[face], AttachmentLoadOp::CLEAR, AttachmentStoreOp::DONT_CARE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE };
				Rect renderRect{ {0, 0}, {resolution, resolution} };

				cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)resolution, (float)resolution, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {resolution, resolution} };
					cmdList->setScissor(0, 1, &scissor);

					struct PassConstants
					{
						float viewProjectionMatrix[16];
						uint32_t transformBufferIndex;
						uint32_t materialBufferIndex;
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &m_currentRenderProbeViewProjMatrices[face][0][0], sizeof(passConsts.viewProjectionMatrix));
					passConsts.transformBufferIndex = data.m_transformBufferHandle;
					passConsts.materialBufferIndex = data.m_materialsBufferHandle;

					uint32_t passConstsAddress = (uint32_t)data.m_constantBufferLinearAllocator->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

					const eastl::vector<SubMeshInstanceData> *instancesArr[]
					{
						&data.m_renderList->m_opaque,
						&data.m_renderList->m_opaqueAlphaTested,
					};
					GraphicsPipeline *pipelines[]
					{
						m_probeGbufferPipeline,
						m_probeGbufferAlphaTestedPipeline,
					};

					for (size_t listType = 0; listType < eastl::size(instancesArr); ++listType)
					{
						if (instancesArr[listType]->empty())
						{
							continue;
						}
						const bool alphaTested = (listType & 1) != 0;
						auto *pipeline = pipelines[listType];

						cmdList->bindPipeline(pipeline);

						gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, m_viewRegistry->getCurrentFrameDescriptorSet() };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

						for (const auto &instance : *instancesArr[listType])
						{
							struct MeshConstants
							{
								uint32_t transformIndex;
								uint32_t materialIndex;
							};

							MeshConstants consts{};
							consts.transformIndex = instance.m_transformIndex;
							consts.materialIndex = instance.m_materialHandle;

							cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, sizeof(consts), &consts);

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

							uint64_t vertexBufferOffsets[]
							{
								0, // positions
								alignedPositionsBufferSize, // normals
								alignedPositionsBufferSize + alignedNormalsBufferSize, // tangents
								alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize, // texcoords
							};

							cmdList->bindIndexBuffer(data.m_meshBufferHandles[instance.m_subMeshHandle].m_indexBuffer, 0, IndexType::UINT16);
							cmdList->bindVertexBuffers(0, 4, vertexBuffers, vertexBufferOffsets);
							cmdList->drawIndexed(data.m_meshDrawInfo[instance.m_subMeshHandle].m_indexCount, 1, 0, 0, 0);
						}
					}
				}
				cmdList->endRenderPass();
			}

			// transition images to correct state for reading in the relighting pass
			{
				gal::Barrier barriers[2];
				{
					barriers[0] = Initializers::imageBarrier(m_probeAlbedoRoughnessArrayImage,
						PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
						PipelineStageFlags::COMPUTE_SHADER_BIT,
						ResourceState::WRITE_COLOR_ATTACHMENT,
						ResourceState::READ_RESOURCE,
						{ 0, 1, (uint32_t)probeIdx * 6, 6 });

					barriers[1] = Initializers::imageBarrier(m_probeNormalDepthArrayImage,
						PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
						PipelineStageFlags::COMPUTE_SHADER_BIT,
						ResourceState::WRITE_COLOR_ATTACHMENT,
						ResourceState::READ_RESOURCE,
						{ 0, 1, (uint32_t)probeIdx * 6, 6 });
				}
				cmdList->barrier(2, barriers);
			}
		});
}

void ReflectionProbeManager::relightProbe(rg::RenderGraph *graph, const Data &data, size_t probeIdx) const noexcept
{
	// get list of directional lights and prepare GPU data. we do not care about shadow mapping, so making a list of all lights is enough.
	eastl::fixed_vector<DirectionalLightGPU, 8> directionalLights;
	eastl::fixed_vector<DirectionalLightGPU, 8> directionalLightsShadowed;
	data.m_ecs->iterate<TransformComponent, LightComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, LightComponent *lightC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &lc = lightC[i];

				if (lc.m_type == LightComponent::Type::Directional)
				{
					DirectionalLightGPU directionalLight{};
					directionalLight.m_color = glm::vec3(glm::unpackUnorm4x8(lc.m_color)) * lc.m_intensity;
					directionalLight.m_direction = glm::normalize(tc.m_curRenderTransform.m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
					directionalLight.m_cascadeCount = 0;

					if (lc.m_shadows)
					{
						directionalLightsShadowed.push_back(directionalLight);
					}
					else
					{
						directionalLights.push_back(directionalLight);
					}
				}
			}
		});

	// create buffers for directional light data
	uint32_t numDirectionalLights = static_cast<uint32_t>(directionalLights.size());
	uint32_t numDirectionalLightsShadowed = static_cast<uint32_t>(directionalLightsShadowed.size());
	StructuredBufferViewHandle directionalLightsBufferViewHandle = {};
	StructuredBufferViewHandle directionalLightsShadowedBufferViewHandle = {};
	{
		const uint64_t alignment = m_device->getBufferAlignment(DescriptorType::STRUCTURED_BUFFER, sizeof(DirectionalLightGPU));

		// unshadowed
		if (!directionalLights.empty())
		{
			// prepare DescriptorBufferInfo
			DescriptorBufferInfo bufferInfo = Initializers::structuredBufferInfo(sizeof(DirectionalLightGPU), directionalLights.size());
			bufferInfo.m_buffer = data.m_shaderResourceLinearAllocator->getBuffer();

			// allocate memory
			uint8_t *bufferPtr = data.m_shaderResourceLinearAllocator->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);
			memcpy(bufferPtr, directionalLights.data(), directionalLights.size() * sizeof(directionalLights[0]));

			// create a transient bindless handle
			directionalLightsBufferViewHandle = m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true);
		}

		// shadowed
		if (!directionalLightsShadowed.empty())
		{
			// prepare DescriptorBufferInfo
			DescriptorBufferInfo bufferInfo = Initializers::structuredBufferInfo(sizeof(DirectionalLightGPU), directionalLightsShadowed.size());
			bufferInfo.m_buffer = data.m_shaderResourceLinearAllocator->getBuffer();

			// allocate memory
			uint8_t *bufferPtr = data.m_shaderResourceLinearAllocator->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);
			memcpy(bufferPtr, directionalLightsShadowed.data(), directionalLightsShadowed.size() * sizeof(directionalLightsShadowed[0]));

			// create a transient bindless handle
			directionalLightsShadowedBufferViewHandle = m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true);
		}
	}

	// relight gbuffer
	graph->addPass("Reflection Probe Relight", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Reflection Probe Relight");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_gpuProfilingCtx, cmdList, "Reflection Probe Relight");
			PROFILING_ZONE_SCOPED;

			// transition tmp image to RW_RESOURCE (we need to write to the mips in the next pass, so just transitition them here already)
			{
				gal::Barrier barrier = Initializers::imageBarrier(m_probeTmpLitImage,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					ResourceState::READ_RESOURCE,
					ResourceState::RW_RESOURCE_WRITE_ONLY,
					{ 0, k_numPrefilterMips, 0, 6 });
				cmdList->barrier(1, &barrier);
			}

			struct RelightConstants
			{
				glm::mat4 probeFaceToWorldSpace[6];
				glm::vec4 worldToLocal0;
				glm::vec4 worldToLocal1;
				glm::vec4 worldToLocal2;
				glm::vec3 probePosition;
				float texelSize;
				uint32_t resolution;
				uint32_t probeArraySlot;
				uint32_t directionalLightCount;
				uint32_t directionalLightBufferIndex;
				uint32_t directionalLightShadowedCount;
				uint32_t directionalLightShadowedBufferIndex;
				uint32_t irradianceVolumeCount;
				uint32_t irradianceVolumeBufferIndex;
				uint32_t albedoRoughnessTextureIndex;
				uint32_t normalDepthTextureIndex;
				uint32_t resultTextureIndex;
			};

			glm::mat4 projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, m_currentRelightProbeFarPlane, m_currentRelightProbeNearPlane);

			RelightConstants consts{};
			consts.probeFaceToWorldSpace[0] = glm::inverse(projection * glm::lookAtLH(m_currentRelightProbePosition, m_currentRelightProbePosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
			consts.probeFaceToWorldSpace[1] = glm::inverse(projection * glm::lookAtLH(m_currentRelightProbePosition, m_currentRelightProbePosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
			consts.probeFaceToWorldSpace[2] = glm::inverse(projection * glm::lookAtLH(m_currentRelightProbePosition, m_currentRelightProbePosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
			consts.probeFaceToWorldSpace[3] = glm::inverse(projection * glm::lookAtLH(m_currentRelightProbePosition, m_currentRelightProbePosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
			consts.probeFaceToWorldSpace[4] = glm::inverse(projection * glm::lookAtLH(m_currentRelightProbePosition, m_currentRelightProbePosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
			consts.probeFaceToWorldSpace[5] = glm::inverse(projection * glm::lookAtLH(m_currentRelightProbePosition, m_currentRelightProbePosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
			consts.worldToLocal0 = m_currentRelightProbeWorldToLocalTransposed[0];
			consts.worldToLocal1 = m_currentRelightProbeWorldToLocalTransposed[1];
			consts.worldToLocal2 = m_currentRelightProbeWorldToLocalTransposed[2];
			consts.probePosition = m_currentRelightProbePosition;
			consts.texelSize = 1.0f / k_resolution;
			consts.resolution = static_cast<uint32_t>(k_resolution);
			consts.probeArraySlot = static_cast<uint32_t>(probeIdx);
			consts.directionalLightCount = numDirectionalLights;
			consts.directionalLightBufferIndex = directionalLightsBufferViewHandle;
			consts.directionalLightShadowedCount = numDirectionalLightsShadowed;
			consts.directionalLightShadowedBufferIndex = directionalLightsShadowedBufferViewHandle;
			consts.irradianceVolumeCount = data.m_irradianceVolumeCount;
			consts.irradianceVolumeBufferIndex = data.m_irradianceVolumeBufferViewHandle;
			consts.albedoRoughnessTextureIndex = m_probeAlbedoRoughnessTextureViewHandle;
			consts.normalDepthTextureIndex = m_probeNormalDepthTextureViewHandle;
			consts.resultTextureIndex = m_probeTmpLitArrayRWTextureViewHandles[0]; // write into mip 0

			uint32_t constsAddress = (uint32_t)data.m_constantBufferLinearAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, consts);

			cmdList->bindPipeline(m_probeRelightPipeline);

			gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, m_viewRegistry->getCurrentFrameDescriptorSet() };
			cmdList->bindDescriptorSets(m_probeRelightPipeline, 0, 2, sets, 1, &constsAddress);

			cmdList->dispatch((consts.resolution + 7) / 8, (consts.resolution + 7) / 8, 6);
		});

	// generate mips
	graph->addPass("Reflection Probe Generate Mips", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Reflection Probe Generate Mips");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_gpuProfilingCtx, cmdList, "Reflection Probe Generate Mips");
			PROFILING_ZONE_SCOPED;

			// transition first mip of tmp image to READ_RESOURCE, all other mips are already RW_RESOURCE
			{
				gal::Barrier barrier = Initializers::imageBarrier(m_probeTmpLitImage,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					ResourceState::RW_RESOURCE_WRITE_ONLY,
					ResourceState::READ_RESOURCE,
					{ 0, 1, 0, 6 });
				cmdList->barrier(1, &barrier);
			}

			varAU2(dispatchThreadGroupCountXY);
			varAU2(workGroupOffset);  // only needed if downsampling a smaller region of the image
			varAU2(numWorkGroupsAndMips);
			varAU4(rectInfo) = initAU4(0, 0, k_resolution, k_resolution); // left, top, width, height
			SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

			struct FFXDownsampleConstants
			{
				uint32_t mips;
				uint32_t numWorkGroups;
				uint32_t workGroupOffset[2];
				float inputTexelSize[2];
				uint32_t reductionType; // 0 = average, 1 = min, 2 = max
				uint32_t arrayTextureInput;
				uint32_t inputTextureIndex;
				uint32_t resultMip0TextureIndex;
				uint32_t resultMip1TextureIndex;
				uint32_t resultMip2TextureIndex;
				uint32_t resultMip3TextureIndex;
				uint32_t resultMip4TextureIndex;
				uint32_t resultMip5TextureIndex;
				uint32_t resultMip6TextureIndex;
				uint32_t resultMip7TextureIndex;
				uint32_t resultMip8TextureIndex;
				uint32_t resultMip9TextureIndex;
				uint32_t resultMip10TextureIndex;
				uint32_t resultMip11TextureIndex;
				uint32_t spdCounterBufferIndex;
			};

			FFXDownsampleConstants consts{};
			consts.mips = k_numPrefilterMips - 1; // first mip is already done
			consts.numWorkGroups = numWorkGroupsAndMips[0];
			consts.workGroupOffset[0] = workGroupOffset[0];
			consts.workGroupOffset[1] = workGroupOffset[1];
			consts.inputTexelSize[0] = 1.0f / k_resolution;
			consts.inputTexelSize[1] = 1.0f / k_resolution;
			consts.reductionType = 0;
			consts.arrayTextureInput = true;
			consts.inputTextureIndex = m_probeTmpLitArrayMip0TextureViewHandle;
			consts.resultMip0TextureIndex = m_probeTmpLitArrayRWTextureViewHandles[1]; // we read from mip 0 and write to mip *1*!
			consts.resultMip1TextureIndex = m_probeTmpLitArrayRWTextureViewHandles[2];
			consts.resultMip2TextureIndex = m_probeTmpLitArrayRWTextureViewHandles[3];
			consts.resultMip3TextureIndex = m_probeTmpLitArrayRWTextureViewHandles[4];
			consts.spdCounterBufferIndex = m_spdCounterBufferViewHandle;

			uint32_t constsAddress = (uint32_t)data.m_constantBufferLinearAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, consts);

			cmdList->bindPipeline(m_ffxDownsamplePipeline);

			gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, m_viewRegistry->getCurrentFrameDescriptorSet() };
			cmdList->bindDescriptorSets(m_ffxDownsamplePipeline, 0, 2, sets, 1, &constsAddress);

			cmdList->dispatch(dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], 6);

			// transition tmp image mips to READ_RESOURCE
			{
				gal::Barrier barrier = Initializers::imageBarrier(m_probeTmpLitImage,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					ResourceState::RW_RESOURCE_WRITE_ONLY,
					ResourceState::READ_RESOURCE,
					{ 1, k_numPrefilterMips - 1, 0, 6 });
				cmdList->barrier(1, &barrier);
			}
		});

	// filter
	graph->addPass("Reflection Probe Filter", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Reflection Probe Filter");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_gpuProfilingCtx, cmdList, "Reflection Probe Filter");
			PROFILING_ZONE_SCOPED;

			// transition result image to RW_RESOURCE
			{
				gal::Barrier barrier = Initializers::imageBarrier(m_probeArrayImage,
					PipelineStageFlags::PIXEL_SHADER_BIT,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					ResourceState::READ_RESOURCE,
					ResourceState::RW_RESOURCE_WRITE_ONLY,
					{ 0, k_numMips, (uint32_t)probeIdx * 6, 6 });
				cmdList->barrier(1, &barrier);
			}

			cmdList->bindPipeline(m_probeFilterPipeline);
			auto *bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
			cmdList->bindDescriptorSets(m_probeFilterPipeline, 0, 1, &bindlessSet, 0, nullptr);

			for (size_t i = 0; i < k_numMips; ++i)
			{
				constexpr float k_maxMip = static_cast<float>(k_numMips - 1);

				ProbeFilterPushConsts pushConsts{};
				pushConsts.resolution = static_cast<uint32_t>(k_resolution) >> i;
				pushConsts.texelSize = 1.0f / pushConsts.resolution;
				pushConsts.mip0Resolution = static_cast<float>(k_resolution);
				pushConsts.roughness = (i * i) / (k_maxMip * k_maxMip); // mipLevel = sqrt(roughness) * maxMip -> solve for roughness
				pushConsts.mipCount = static_cast<float>(k_numPrefilterMips);
				pushConsts.copyOnly = i == 0;
				pushConsts.inputTextureIndex = i == 0 ? m_probeTmpLitArrayMip0TextureViewHandle : m_probeTmpLitCubeTextureViewHandle;
				pushConsts.resultTextureIndex = m_probeArrayMipRWTextureViewHandles[probeIdx][i];

				cmdList->pushConstants(m_probeFilterPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((pushConsts.resolution + 7) / 8, (pushConsts.resolution + 7) / 8, 6);
			}

			// transition result image back to READ_RESOURCE
			{
				gal::Barrier barrier = Initializers::imageBarrier(m_probeArrayImage,
					PipelineStageFlags::COMPUTE_SHADER_BIT,
					PipelineStageFlags::PIXEL_SHADER_BIT,
					ResourceState::RW_RESOURCE_WRITE_ONLY,
					ResourceState::READ_RESOURCE,
					{ 0, k_numMips, (uint32_t)probeIdx * 6, 6 });
				cmdList->barrier(1, &barrier);
			}
		});
}
