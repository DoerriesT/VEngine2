#include "IrradianceVolumeManager.h"
#include <glm/gtx/transform.hpp>
#include <EASTL/fixed_vector.h>
#include <EASTL/numeric.h>
#include <EASTL/sort.h>
#include "gal/Initializers.h"
#include "RenderData.h"
#include "Mesh.h"
#include "RendererResources.h"
#include "ResourceViewRegistry.h"
#include "LinearGPUBufferAllocator.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "ecs/ECS.h"
#include "utility/Utility.h"
#include "LightManager.h"
#include "component/TransformComponent.h"
#include "component/IrradianceVolumeComponent.h"
#include "MeshRenderWorld.h"

using namespace gal;

namespace
{
	struct ForwardPushConstants
	{
		uint32_t transformIndex;
		uint32_t materialIndex;
	};

	struct SkyPushConsts
	{
		float invModelViewProjection[16];
		uint32_t exposureBufferIndex;
	};

	struct ProbeFilterPushConsts
	{
		uint32_t baseOutputOffset[2];
		float texelSize;
		float maxDistance;
		uint32_t inputTextureIndex;
		uint32_t resultTextureIndex;
	};

	struct ProbeAverageFilterPushConsts
	{
		uint32_t resolution[2];
		uint32_t inputTextureIndex;
		uint32_t resultTextureIndex;
	};
}

IrradianceVolumeManager::IrradianceVolumeManager(gal::GraphicsDevice *device, RendererResources *renderResources, ResourceViewRegistry *viewRegistry) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry),
	m_rendererResources(renderResources),
	m_lightManager(new LightManager(device, renderResources, viewRegistry))
{
	Format renderTargetFormats[]
	{
		Format::R16G16B16A16_SFLOAT,
		Format::R16_SFLOAT,
	};
	PipelineColorBlendAttachmentState blendStates[]
	{
		GraphicsPipelineBuilder::s_defaultBlendAttachment,
		GraphicsPipelineBuilder::s_defaultBlendAttachment,
	};
	const uint32_t renderTargetFormatCount = static_cast<uint32_t>(eastl::size(renderTargetFormats));

	// forward
	{
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
			builder.setVertexShader("assets/shaders/irradianceProbeForward_vs");
			builder.setFragmentShader(isAlphaTested ? "assets/shaders/irradianceProbeForward_alpha_tested_ps" : "assets/shaders/irradianceProbeForward_ps");
			builder.setVertexBindingDescriptions(bindingDescCount, bindingDescs);
			builder.setVertexAttributeDescriptions(attributeDescCount, attributeDescs);
			builder.setColorBlendAttachments(renderTargetFormatCount, blendStates);
			builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
			builder.setColorAttachmentFormats(renderTargetFormatCount, renderTargetFormats);
			builder.setPolygonModeCullMode(gal::PolygonMode::FILL, gal::CullModeFlags::NONE, gal::FrontFace::CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
			DescriptorSetLayoutBinding usedBindlessBindings[] =
			{
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1, ShaderStageFlags::VERTEX_BIT), // model matrices
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 2, ShaderStageFlags::PIXEL_BIT), // material buffer
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 3, ShaderStageFlags::PIXEL_BIT), // directional lights
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 4, ShaderStageFlags::PIXEL_BIT), // array textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 5, ShaderStageFlags::PIXEL_BIT), // exposure buffer, depth bins buffers
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_BYTE_BUFFER, 6, ShaderStageFlags::PIXEL_BIT), // picking buffer
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 7, ShaderStageFlags::PIXEL_BIT), // punctual lights
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 8, ShaderStageFlags::PIXEL_BIT), // punctual lights shadowed
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 9, ShaderStageFlags::PIXEL_BIT), // array textures UI
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 10, ShaderStageFlags::PIXEL_BIT), // irradiance volumes
			};

			DescriptorSetLayoutDeclaration layoutDecls[]
			{
				{ renderResources->m_offsetBufferDescriptorSetLayout, 1, &usedOffsetBufferBinding },
				{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
			};

			gal::StaticSamplerDescription staticSamplerDescs[]
			{
				gal::Initializers::staticAnisotropicRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT),
				gal::Initializers::staticLinearClampSampler(1, 0, gal::ShaderStageFlags::PIXEL_BIT),
				gal::Initializers::staticLinearClampSampler(2, 0, gal::ShaderStageFlags::PIXEL_BIT),
			};
			staticSamplerDescs[2].m_compareEnable = true;
			staticSamplerDescs[2].m_compareOp = CompareOp::GREATER_OR_EQUAL;

			builder.setPipelineLayoutDescription(2, layoutDecls, sizeof(ForwardPushConstants), ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, static_cast<uint32_t>(eastl::size(staticSamplerDescs)), staticSamplerDescs, 2);

			device->createGraphicsPipelines(1, &pipelineCreateInfo, isAlphaTested ? &m_forwardAlphaTestedPipeline : &m_forwardPipeline);
		}
	}

	// sky
	{
		gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
		gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/sky_vs");
		builder.setFragmentShader("assets/shaders/irradianceProbeSky_ps");
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
			{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(SkyPushConsts), ShaderStageFlags::ALL_STAGES, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_skyPipeline);
	}

	// probe filter
	for (size_t i = 0; i < 2; ++i)
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 1), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};
		gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticPointClampSampler(0, 0, ShaderStageFlags::COMPUTE_BIT);

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader(i == 0 ? "assets/shaders/irradianceProbeFilter_diffuse_cs" : "assets/shaders/irradianceProbeFilter_visibility_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(ProbeFilterPushConsts), ShaderStageFlags::COMPUTE_BIT, 1, &staticSamplerDesc, 1);

		device->createComputePipelines(1, &pipelineCreateInfo, i == 0 ? &m_diffuseFilterPipeline : &m_visibilityFilterPipeline);
	}

	// average probe filter
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 1), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ viewRegistry->getDescriptorSetLayout(), (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/irradianceProbeAverageFilter_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(ProbeAverageFilterPushConsts), ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_averageFilterPipeline);
	}

	// images
	{
		// cubemaps
		{
			ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_width = k_renderResolution;
			imageCreateInfo.m_height = k_renderResolution;
			imageCreateInfo.m_depth = 1;
			imageCreateInfo.m_levels = 1;
			imageCreateInfo.m_layers = 6;
			imageCreateInfo.m_samples = SampleCount::_1;
			imageCreateInfo.m_imageType = ImageType::_2D;
			imageCreateInfo.m_createFlags = ImageCreateFlags::CUBE_COMPATIBLE_BIT;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::COLOR_ATTACHMENT_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = { 0.0f, 0.0f, 0.0f, 0.0f };

			// color
			{
				imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;

				m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_colorImage);
				m_device->setDebugObjectName(ObjectType::IMAGE, m_colorImage, "Irradiance Volume Color Cube Image");
			}

			// distance
			{
				imageCreateInfo.m_format = Format::R16_SFLOAT;

				m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_distanceImage);
				m_device->setDebugObjectName(ObjectType::IMAGE, m_distanceImage, "Irradiance Volume Distance Cube Image");
			}
		}

		// depth buffer
		{
			ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_width = k_renderResolution;
			imageCreateInfo.m_height = k_renderResolution;
			imageCreateInfo.m_depth = 1;
			imageCreateInfo.m_levels = 1;
			imageCreateInfo.m_layers = 1;
			imageCreateInfo.m_samples = SampleCount::_1;
			imageCreateInfo.m_imageType = ImageType::_2D;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT;
			imageCreateInfo.m_optimizedClearValue.m_depthStencil = {};
			imageCreateInfo.m_format = Format::D32_SFLOAT;

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_depthBufferImage);
			m_device->setDebugObjectName(ObjectType::IMAGE, m_depthBufferImage, "Irradiance Volume Depth Buffer Image");
			m_device->createImageView(m_depthBufferImage, &m_depthBufferImageView);
		}

		// irradiance volume
		{
			ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_depth = 1;
			imageCreateInfo.m_levels = 1;
			imageCreateInfo.m_layers = 1;
			imageCreateInfo.m_samples = SampleCount::_1;
			imageCreateInfo.m_imageType = ImageType::_2D;
			imageCreateInfo.m_usageFlags = ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TRANSFER_SRC_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = {};

			// tmp diffuse
			{
				imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
				imageCreateInfo.m_width = (k_diffuseProbeResolution + 2) * k_tmpTextureProbeWidth;
				imageCreateInfo.m_height = (k_diffuseProbeResolution + 2) * k_tmpTextureProbeHeight;

				m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_tmpIrradianceVolumeDiffuseImage);
				m_device->setDebugObjectName(ObjectType::IMAGE, m_tmpIrradianceVolumeDiffuseImage, "Tmp Irradiance Volume Diffuse Image");
			}

			// tmp visibility
			{
				imageCreateInfo.m_format = Format::R16G16_SFLOAT;
				imageCreateInfo.m_width = (k_visibilityProbeResolution + 2) * k_tmpTextureProbeWidth;
				imageCreateInfo.m_height = (k_visibilityProbeResolution + 2) * k_tmpTextureProbeHeight;

				m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_tmpIrradianceVolumeVisibilityImage);
				m_device->setDebugObjectName(ObjectType::IMAGE, m_tmpIrradianceVolumeVisibilityImage, "Tmp Irradiance Volume Visibility Image");
			}
		}
	}

	// transition images to expected layouts
	{
		auto *cmdList = renderResources->m_commandList;
		renderResources->m_commandListPool->reset();
		cmdList->begin();
		{
			Barrier barriers[]
			{
				Initializers::imageBarrier(m_depthBufferImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlags::LATE_FRAGMENT_TESTS_BIT, ResourceState::UNDEFINED, ResourceState::WRITE_DEPTH_STENCIL),
			};
			cmdList->barrier(sizeof(barriers) / sizeof(barriers[0]), barriers);
		}
		cmdList->end();
		Initializers::submitSingleTimeCommands(m_device->getGraphicsQueue(), cmdList);
	}
}

IrradianceVolumeManager::~IrradianceVolumeManager() noexcept
{
	if (m_lightManager)
	{
		delete m_lightManager;
		m_lightManager = nullptr;
	}

	m_device->destroyGraphicsPipeline(m_forwardPipeline);
	m_device->destroyGraphicsPipeline(m_forwardAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_skyPipeline);
	m_device->destroyComputePipeline(m_diffuseFilterPipeline);
	m_device->destroyComputePipeline(m_visibilityFilterPipeline);
	m_device->destroyComputePipeline(m_averageFilterPipeline);

	m_device->destroyImage(m_tmpIrradianceVolumeDiffuseImage);
	m_device->destroyImage(m_tmpIrradianceVolumeVisibilityImage);
	m_device->destroyImage(m_colorImage);
	m_device->destroyImage(m_distanceImage);
	m_device->destroyImage(m_depthBufferImage);
	m_device->destroyImageView(m_depthBufferImageView);

	while (!m_deletionQueue.empty())
	{
		auto &oldResources = m_deletionQueue.front();

		m_device->destroyImage(oldResources.m_diffuseImage);
		m_device->destroyImage(oldResources.m_visibilityImage);
		m_device->destroyImage(oldResources.m_averageDiffuseImage);
		m_device->destroyImageView(oldResources.m_diffuseImageView);
		m_device->destroyImageView(oldResources.m_visibilityImageView);
		m_device->destroyImageView(oldResources.m_averageDiffuseImageView);
		m_viewRegistry->destroyHandle(oldResources.m_diffuseImageViewHandle);
		m_viewRegistry->destroyHandle(oldResources.m_visibilityImageViewHandle);
		m_viewRegistry->destroyHandle(oldResources.m_averageDiffuseImageViewHandle);
		m_viewRegistry->destroyHandle(oldResources.m_averageDiffuseImageRWViewHandle);
		m_deletionQueue.pop();
	}
}

void IrradianceVolumeManager::update(rg::RenderGraph *graph, const Data &data) noexcept
{
	if (m_abortedBake)
	{
		for (auto &v : m_volumes)
		{
			if (data.m_ecs->isValid(v.m_entity) && data.m_ecs->hasComponent<IrradianceVolumeComponent>(v.m_entity))
			{
				data.m_ecs->getComponent<IrradianceVolumeComponent>(v.m_entity)->m_internalHandle = 0;
			}

			// put old resources in deletion queue
			DeletionQueueItem qItem{};
			qItem.m_diffuseImage = v.m_diffuseImage;
			qItem.m_diffuseImageView = v.m_diffuseImageView;
			qItem.m_diffuseImageViewHandle = v.m_diffuseImageViewHandle;
			qItem.m_visibilityImage = v.m_visibilityImage;
			qItem.m_visibilityImageView = v.m_visibilityImageView;
			qItem.m_visibilityImageViewHandle = v.m_visibilityImageViewHandle;
			qItem.m_averageDiffuseImage = v.m_averageDiffuseImage;
			qItem.m_averageDiffuseImageView = v.m_averageDiffuseImageView;
			qItem.m_averageDiffuseImageViewHandle = v.m_averageDiffuseImageViewHandle;
			qItem.m_averageDiffuseImageRWViewHandle = v.m_averageDiffuseImageRWViewHandle;
			qItem.m_frameToFreeIn = data.m_frame + 2;

			m_deletionQueue.push(qItem);
		}

		m_curBounce = 0;
		m_curVolumeIdx = 0;
		m_curProbeIdx = 0;
		m_processedProbes = 0;
		m_totalProbes = 0;
		m_volumes.clear();
		m_sortedGPUVolumes.clear();
		m_abortedBake = false;
		m_baking = false;
	}

	if (m_startedBake)
	{
		m_curBounce = 0;
		m_curVolumeIdx = 0;
		m_curProbeIdx = 0;
		m_processedProbes = 0;
		m_totalProbes = 0;

		eastl::vector<InternalIrradianceVolume> tmpVolumes;
		data.m_ecs->iterate<TransformComponent, IrradianceVolumeComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, IrradianceVolumeComponent *volumeC)
			{
				for (size_t i = 0; i < count; ++i)
				{
					auto &tc = transC[i];
					auto &vc = volumeC[i];

					InternalIrradianceVolume volume{};
					volume.m_transform = tc.m_globalTransform;
					volume.m_entity = entities[i];
					volume.m_resolutionX = eastl::max<uint32_t>(2, vc.m_resolutionX);
					volume.m_resolutionY = eastl::max<uint32_t>(2, vc.m_resolutionY);
					volume.m_resolutionZ = eastl::max<uint32_t>(2, vc.m_resolutionZ);
					volume.m_fadeoutStart = fmaxf(vc.m_fadeoutStart, 0.0f);
					volume.m_fadeoutEnd = fmaxf(vc.m_fadeoutEnd, volume.m_fadeoutStart + 1e-5f);
					volume.m_selfShadowBias = vc.m_selfShadowBias;
					volume.m_nearPlane = vc.m_nearPlane;
					volume.m_farPlane = vc.m_farPlane;

					const uint32_t diffuseTexWidth = (k_diffuseProbeResolution + 2) * volume.m_resolutionX * volume.m_resolutionY;
					const uint32_t diffuseTexHeight = (k_diffuseProbeResolution + 2) * volume.m_resolutionZ;
					const uint32_t visibilityTexWidth = (k_visibilityProbeResolution + 2) * volume.m_resolutionX * volume.m_resolutionY;
					const uint32_t visibilityTexHeight = (k_visibilityProbeResolution + 2) * volume.m_resolutionZ;

					bool createTextures = true;

					// try to recycle old textures if possible
					if (vc.m_internalHandle != 0 && vc.m_internalHandle <= m_volumes.size() && m_volumes[vc.m_internalHandle - 1].m_entity == entities[i])
					{
						auto &oldVolume = m_volumes[vc.m_internalHandle - 1];

						assert(oldVolume.m_diffuseImage);
						assert(oldVolume.m_visibilityImage);
						const auto &diffuseTexDesc = oldVolume.m_diffuseImage->getDescription();
						const auto &visibilityTexDesc = oldVolume.m_visibilityImage->getDescription();

						// reject all old textures that do not match exactly
						if (diffuseTexDesc.m_width != diffuseTexWidth || diffuseTexDesc.m_height != diffuseTexHeight ||
							visibilityTexDesc.m_width != visibilityTexWidth || visibilityTexDesc.m_height != visibilityTexHeight)
						{
							createTextures = true;

							// put old resources in deletion queue
							DeletionQueueItem qItem{};
							qItem.m_diffuseImage = oldVolume.m_diffuseImage;
							qItem.m_diffuseImageView = oldVolume.m_diffuseImageView;
							qItem.m_diffuseImageViewHandle = oldVolume.m_diffuseImageViewHandle;
							qItem.m_visibilityImage = oldVolume.m_visibilityImage;
							qItem.m_visibilityImageView = oldVolume.m_visibilityImageView;
							qItem.m_visibilityImageViewHandle = oldVolume.m_visibilityImageViewHandle;
							qItem.m_averageDiffuseImage = oldVolume.m_averageDiffuseImage;
							qItem.m_averageDiffuseImageView = oldVolume.m_averageDiffuseImageView;
							qItem.m_averageDiffuseImageViewHandle = oldVolume.m_averageDiffuseImageViewHandle;
							qItem.m_averageDiffuseImageRWViewHandle = oldVolume.m_averageDiffuseImageRWViewHandle;
							qItem.m_frameToFreeIn = data.m_frame + 2;

							m_deletionQueue.push(qItem);
						}
						//else
						//{
						//	createTextures = false;
						//
						//	volume.m_diffuseImage = oldVolume.m_diffuseImage;
						//	volume.m_diffuseImageView = oldVolume.m_diffuseImageView;
						//	volume.m_diffuseImageViewHandle = oldVolume.m_diffuseImageViewHandle;
						//	volume.m_visibilityImage = oldVolume.m_visibilityImage;
						//	volume.m_visibilityImageView = oldVolume.m_visibilityImageView;
						//	volume.m_visibilityImageViewHandle = oldVolume.m_visibilityImageViewHandle;
						//	volume.m_averageDiffuseImage = oldVolume.m_averageDiffuseImage;
						//	volume.m_averageDiffuseImageView = oldVolume.m_averageDiffuseImageView;
						//	volume.m_averageDiffuseImageViewHandle = oldVolume.m_averageDiffuseImageViewHandle;
						//	volume.m_averageDiffuseImageRWViewHandle = oldVolume.m_averageDiffuseImageRWViewHandle;
						//}
					}

					if (createTextures)
					{
						ImageCreateInfo imageCreateInfo{};
						imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::TRANSFER_DST_BIT;

						// diffuse
						imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
						imageCreateInfo.m_width = (k_diffuseProbeResolution + 2) * volume.m_resolutionX * volume.m_resolutionY;
						imageCreateInfo.m_height = (k_diffuseProbeResolution + 2) * volume.m_resolutionZ;
						m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &volume.m_diffuseImage);
						m_device->setDebugObjectName(ObjectType::IMAGE, volume.m_diffuseImage, "Irradiance Volume Diffuse Image");
						m_device->createImageView(volume.m_diffuseImage, &volume.m_diffuseImageView);
						m_device->setDebugObjectName(ObjectType::IMAGE_VIEW, volume.m_diffuseImageView, "Irradiance Volume Diffuse Image View");
						volume.m_diffuseImageViewHandle = m_viewRegistry->createTextureViewHandle(volume.m_diffuseImageView);

						// visibility
						imageCreateInfo.m_format = Format::R16G16_SFLOAT;
						imageCreateInfo.m_width = (k_visibilityProbeResolution + 2) * volume.m_resolutionX * volume.m_resolutionY;
						imageCreateInfo.m_height = (k_visibilityProbeResolution + 2) * volume.m_resolutionZ;
						m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &volume.m_visibilityImage);
						m_device->setDebugObjectName(ObjectType::IMAGE, volume.m_visibilityImage, "Irradiance Volume Visibility Image");
						m_device->createImageView(volume.m_visibilityImage, &volume.m_visibilityImageView);
						m_device->setDebugObjectName(ObjectType::IMAGE_VIEW, volume.m_visibilityImageView, "Irradiance Volume Visibility Image View");
						volume.m_visibilityImageViewHandle = m_viewRegistry->createTextureViewHandle(volume.m_visibilityImageView);

						// average diffuse
						imageCreateInfo.m_usageFlags = ImageUsageFlags::TEXTURE_BIT | ImageUsageFlags::RW_TEXTURE_BIT;
						imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
						imageCreateInfo.m_width = volume.m_resolutionX * volume.m_resolutionY;
						imageCreateInfo.m_height = volume.m_resolutionZ;
						m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &volume.m_averageDiffuseImage);
						m_device->setDebugObjectName(ObjectType::IMAGE, volume.m_averageDiffuseImage, "Irradiance Volume Avg Diffuse Image");
						m_device->createImageView(volume.m_averageDiffuseImage, &volume.m_averageDiffuseImageView);
						m_device->setDebugObjectName(ObjectType::IMAGE_VIEW, volume.m_averageDiffuseImageView, "Irradiance Volume Avg Diffuse Image View");
						volume.m_averageDiffuseImageViewHandle = m_viewRegistry->createTextureViewHandle(volume.m_averageDiffuseImageView);
						volume.m_averageDiffuseImageRWViewHandle = m_viewRegistry->createRWTextureViewHandle(volume.m_averageDiffuseImageView);
					}


					tmpVolumes.push_back(volume);

					m_totalProbes += volume.m_resolutionX * volume.m_resolutionY * volume.m_resolutionZ;
				}
			});

		m_volumes = eastl::move(tmpVolumes);
		m_totalProbes *= m_targetBounces;

		// sort by size
		eastl::sort(m_volumes.begin(), m_volumes.end(), [&](const auto &lhs, const auto &rhs)
			{
				float lhsVolume = lhs.m_transform.m_scale.x * lhs.m_transform.m_scale.y * lhs.m_transform.m_scale.z;
				float rhsVolume = rhs.m_transform.m_scale.x * rhs.m_transform.m_scale.y * rhs.m_transform.m_scale.z;

				return lhsVolume < rhsVolume;
			});

		// create gpu volumes
		uint32_t nextInternalHandle = 1;
		m_sortedGPUVolumes.clear();
		for (const auto &volume : m_volumes)
		{
			// while we are at it, set the handle on the component
			data.m_ecs->getComponent<IrradianceVolumeComponent>(volume.m_entity)->m_internalHandle = nextInternalHandle++;

			glm::vec3 probeSpacing = volume.m_transform.m_scale / (glm::vec3(volume.m_resolutionX, volume.m_resolutionY, volume.m_resolutionZ) * 0.5f);
			glm::vec3 volumeOrigin = volume.m_transform.m_translation - (glm::mat3_cast(volume.m_transform.m_rotation) * volume.m_transform.m_scale);
			glm::mat4 worldToLocalTransposed = glm::transpose(glm::scale(1.0f / probeSpacing) * glm::mat4_cast(glm::inverse(volume.m_transform.m_rotation)) * glm::translate(-volumeOrigin));
			glm::mat4 localToWorldTransposed = glm::transpose(glm::translate(volumeOrigin) * glm::mat4_cast(volume.m_transform.m_rotation) * glm::scale(probeSpacing));

			IrradianceVolumeGPU volumeGPU{};
			volumeGPU.worldToLocal0 = worldToLocalTransposed[0];
			volumeGPU.worldToLocal1 = worldToLocalTransposed[1];
			volumeGPU.worldToLocal2 = worldToLocalTransposed[2];
			volumeGPU.localToWorld0 = localToWorldTransposed[0];
			volumeGPU.localToWorld1 = localToWorldTransposed[1];
			volumeGPU.localToWorld2 = localToWorldTransposed[2];
			volumeGPU.volumeSize = glm::vec3(volume.m_resolutionX, volume.m_resolutionY, volume.m_resolutionZ);
			volumeGPU.normalBias = volume.m_selfShadowBias;
			volumeGPU.volumeTexelSize.x = 1.0f / (volume.m_resolutionX * volume.m_resolutionY);
			volumeGPU.volumeTexelSize.y = 1.0f / volume.m_resolutionZ;
			volumeGPU.diffuseTextureIndex = volume.m_diffuseImageViewHandle;
			volumeGPU.visibilityTextureIndex = volume.m_visibilityImageViewHandle;
			volumeGPU.averageDiffuseTextureIndex = volume.m_averageDiffuseImageViewHandle;
			volumeGPU.fadeoutStart = volume.m_fadeoutStart;
			volumeGPU.fadeoutEnd = volume.m_fadeoutEnd;

			m_sortedGPUVolumes.push_back(volumeGPU);
		}

		graph->addPass("Irradiance Volume Initial Transition", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				eastl::fixed_vector<Barrier, 16> barriers;
				for (const auto &v : m_volumes)
				{
					barriers.push_back(Initializers::imageBarrier(v.m_diffuseImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE));
					barriers.push_back(Initializers::imageBarrier(v.m_visibilityImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE));
					barriers.push_back(Initializers::imageBarrier(v.m_averageDiffuseImage, PipelineStageFlags::TOP_OF_PIPE_BIT, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_RESOURCE));
				}
				cmdList->barrier(static_cast<uint32_t>(barriers.size()), barriers.data());
			});

		m_startedBake = false;
		m_baking = true;
	}

	// remove deleted/invalid volumes
	if (!m_baking)
	{
		bool needToRemove = false;
		for (const auto &volume : m_volumes)
		{
			if (!data.m_ecs->isValid(volume.m_entity) || !data.m_ecs->hasComponents<TransformComponent, IrradianceVolumeComponent>(volume.m_entity))
			{
				needToRemove = false;
				break;
			}
		}

		if (needToRemove)
		{
			size_t writeIdx = 0;
			for (size_t i = 0; i < m_volumes.size(); ++i)
			{
				const auto &volume = m_volumes[i];
				if (data.m_ecs->isValid(volume.m_entity) && data.m_ecs->hasComponents<TransformComponent, IrradianceVolumeComponent>(volume.m_entity))
				{
					if (writeIdx != i)
					{
						assert(writeIdx < i);
						m_volumes[writeIdx] = volume;
						m_sortedGPUVolumes[writeIdx] = m_sortedGPUVolumes[i];
						++writeIdx;
					}
				}
				else
				{
					// put old resources in deletion queue
					DeletionQueueItem qItem{};
					qItem.m_diffuseImage = volume.m_diffuseImage;
					qItem.m_diffuseImageView = volume.m_diffuseImageView;
					qItem.m_diffuseImageViewHandle = volume.m_diffuseImageViewHandle;
					qItem.m_visibilityImage = volume.m_visibilityImage;
					qItem.m_visibilityImageView = volume.m_visibilityImageView;
					qItem.m_visibilityImageViewHandle = volume.m_visibilityImageViewHandle;
					qItem.m_averageDiffuseImage = volume.m_averageDiffuseImage;
					qItem.m_averageDiffuseImageView = volume.m_averageDiffuseImageView;
					qItem.m_averageDiffuseImageViewHandle = volume.m_averageDiffuseImageViewHandle;
					qItem.m_averageDiffuseImageRWViewHandle = volume.m_averageDiffuseImageRWViewHandle;
					qItem.m_frameToFreeIn = data.m_frame + 2;

					m_deletionQueue.push(qItem);
				}
			}

			m_volumes.resize(writeIdx);
			m_sortedGPUVolumes.resize(writeIdx);
		}
	}

	// delete old resources
	while (!m_deletionQueue.empty())
	{
		auto &oldResources = m_deletionQueue.front();

		if (oldResources.m_frameToFreeIn <= data.m_frame)
		{
			m_device->destroyImage(oldResources.m_diffuseImage);
			m_device->destroyImage(oldResources.m_visibilityImage);
			m_device->destroyImage(oldResources.m_averageDiffuseImage);
			m_device->destroyImageView(oldResources.m_diffuseImageView);
			m_device->destroyImageView(oldResources.m_visibilityImageView);
			m_device->destroyImageView(oldResources.m_averageDiffuseImageView);
			m_viewRegistry->destroyHandle(oldResources.m_diffuseImageViewHandle);
			m_viewRegistry->destroyHandle(oldResources.m_visibilityImageViewHandle);
			m_viewRegistry->destroyHandle(oldResources.m_averageDiffuseImageViewHandle);
			m_viewRegistry->destroyHandle(oldResources.m_averageDiffuseImageRWViewHandle);
			m_deletionQueue.pop();
		}
		else
		{
			// since frameIndex is monotonically increasing, we can just break out of the loop once we encounter
			// the first item where we have not reached the frame to delete it in yet.
			break;
		}
	}

	// create buffer for volume data
	{
		// prepare DescriptorBufferInfo
		DescriptorBufferInfo bufferInfo = Initializers::structuredBufferInfo(sizeof(m_sortedGPUVolumes[0]), eastl::max<size_t>(1, m_sortedGPUVolumes.size()));
		bufferInfo.m_buffer = data.m_shaderResourceLinearAllocator->getBuffer();

		// allocate memory
		const uint64_t alignment = m_device->getBufferAlignment(DescriptorType::STRUCTURED_BUFFER, sizeof(m_sortedGPUVolumes[0]));
		auto *bufferPtr = data.m_shaderResourceLinearAllocator->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

		// copy in sorted order to gpu memory
		if (!m_sortedGPUVolumes.empty())
		{
			memcpy(bufferPtr, m_sortedGPUVolumes.data(), m_sortedGPUVolumes.size() * sizeof(m_sortedGPUVolumes[0]));
		}

		// create a transient bindless handle
		m_irradianceVolumeBufferHandle = m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true);
	}

	if (!m_baking)
	{
		return;
	}

	// color image
	rg::ResourceHandle colorImageHandle = graph->importImage(m_colorImage, "Irradiance Volume Color Image", m_colorImageState);
	rg::ResourceViewHandle colorImageViewHandles[6];
	for (size_t i = 0; i < 6; ++i)
	{
		colorImageViewHandles[i] = graph->createImageView(rg::ImageViewDesc::create("Irradiance Volume Color Image", colorImageHandle, { 0, 1, (uint32_t)i, 1 }, ImageViewType::_2D));
	}
	rg::ResourceViewHandle colorImageCubeViewHandle = graph->createImageView(rg::ImageViewDesc::create("Irradiance Volume Color Cube Image", colorImageHandle, { 0, 1, 0, 6 }, ImageViewType::CUBE));

	// distance image
	rg::ResourceHandle distanceImageHandle = graph->importImage(m_distanceImage, "Irradiance Volume Distance Image", m_distanceImageState);
	rg::ResourceViewHandle distanceImageViewHandles[6];
	for (size_t i = 0; i < 6; ++i)
	{
		distanceImageViewHandles[i] = graph->createImageView(rg::ImageViewDesc::create("Irradiance Volume Distance Image", distanceImageHandle, { 0, 1, (uint32_t)i, 1 }, ImageViewType::_2D));
	}
	rg::ResourceViewHandle distanceImageCubeViewHandle = graph->createImageView(rg::ImageViewDesc::create("Irradiance Volume Distance Cube Image", distanceImageHandle, { 0, 1, 0, 6 }, ImageViewType::CUBE));

	// tmp diffuse image
	rg::ResourceHandle tmpDiffuseImageHandle = graph->importImage(m_tmpIrradianceVolumeDiffuseImage, "Tmp Irradiance Volume Diffuse Image", &m_tmpIrradianceVolumeDiffuseImageState);
	rg::ResourceViewHandle tmpDiffuseImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Tmp Irradiance Volume Diffuse Image", tmpDiffuseImageHandle, graph));

	// tmp visibility image
	rg::ResourceHandle tmpVisibilityImageHandle = graph->importImage(m_tmpIrradianceVolumeVisibilityImage, "Tmp Irradiance Volume Visibility Image", &m_tmpIrradianceVolumeVisibilityImageState);
	rg::ResourceViewHandle tmpVisibilityImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Tmp Irradiance Volume Visibility Image", tmpVisibilityImageHandle, graph));


	const bool firstBounce = m_curBounce == 0;
	const uint32_t curVolumeIdx = m_curVolumeIdx;
	const auto &curVolume = m_volumes[curVolumeIdx];

	const uint32_t probeIdxX = m_curProbeIdx % curVolume.m_resolutionX;
	const uint32_t probeIdxZ = (m_curProbeIdx / curVolume.m_resolutionX) % curVolume.m_resolutionZ;
	const uint32_t probeIdxY = m_curProbeIdx / (curVolume.m_resolutionX * curVolume.m_resolutionZ);
	const glm::vec3 probeGridCoord = glm::vec3(probeIdxX, probeIdxY, probeIdxZ) + 0.5f;

	const glm::vec3 probeSpacing = curVolume.m_transform.m_scale / (glm::vec3(curVolume.m_resolutionX, curVolume.m_resolutionY, curVolume.m_resolutionZ) * 0.5f);
	const float maxProbeDistance = glm::length(probeSpacing) * 1.2f;

	glm::vec3 probeWorldSpacePos;
	probeWorldSpacePos.x = glm::dot(m_sortedGPUVolumes[m_curVolumeIdx].localToWorld0, glm::vec4(probeGridCoord, 1.0f));
	probeWorldSpacePos.y = glm::dot(m_sortedGPUVolumes[m_curVolumeIdx].localToWorld1, glm::vec4(probeGridCoord, 1.0f));
	probeWorldSpacePos.z = glm::dot(m_sortedGPUVolumes[m_curVolumeIdx].localToWorld2, glm::vec4(probeGridCoord, 1.0f));

	const uint32_t activeIrradianceVolumeCount = getIrradianceVolumeCount();

	glm::mat4 projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, curVolume.m_farPlane, curVolume.m_nearPlane);
	glm::mat4 viewMatrices[6];
	viewMatrices[0] = glm::lookAtLH(probeWorldSpacePos, probeWorldSpacePos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	viewMatrices[1] = glm::lookAtLH(probeWorldSpacePos, probeWorldSpacePos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	viewMatrices[2] = glm::lookAtLH(probeWorldSpacePos, probeWorldSpacePos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	viewMatrices[3] = glm::lookAtLH(probeWorldSpacePos, probeWorldSpacePos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	viewMatrices[4] = glm::lookAtLH(probeWorldSpacePos, probeWorldSpacePos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	viewMatrices[5] = glm::lookAtLH(probeWorldSpacePos, probeWorldSpacePos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));


	for (size_t face = 0; face < 6; ++face)
	{
		glm::mat4 viewProjectionMatrix = projection * viewMatrices[face];

		m_lightRecordData[face] = {};
		LightManager::Data lightMgrData{};
		lightMgrData.m_ecs = data.m_ecs;
		lightMgrData.m_width = k_renderResolution;
		lightMgrData.m_height = k_renderResolution;
		lightMgrData.m_near = curVolume.m_nearPlane;
		lightMgrData.m_far = curVolume.m_farPlane;
		lightMgrData.m_fovy = glm::radians(90.0f);
		lightMgrData.m_staticMeshShadowsOnly = true;
		lightMgrData.m_viewProjectionMatrix = viewProjectionMatrix;
		lightMgrData.m_invViewMatrix = glm::inverse(viewMatrices[face]);
		lightMgrData.m_viewMatrixDepthRow = glm::transpose(viewMatrices[face])[2];
		lightMgrData.m_gpuProfilingCtx = data.m_gpuProfilingCtx;
		lightMgrData.m_shaderResourceLinearAllocator = data.m_shaderResourceLinearAllocator;
		lightMgrData.m_constantBufferLinearAllocator = data.m_constantBufferLinearAllocator;
		lightMgrData.m_offsetBufferSet = data.m_offsetBufferSet;
		lightMgrData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
		lightMgrData.m_meshRenderWorld = data.m_meshRenderWorld;
		lightMgrData.m_meshDrawInfo = data.m_meshDrawInfo;
		lightMgrData.m_meshBufferHandles = data.m_meshBufferHandles;

		m_lightManager->update(lightMgrData, graph, &m_lightRecordData[face]);

		eastl::fixed_vector<rg::ResourceUsageDesc, 32> usageDescs;
		usageDescs.push_back({ colorImageViewHandles[face], {ResourceState::WRITE_COLOR_ATTACHMENT} });
		usageDescs.push_back({ distanceImageViewHandles[face], {ResourceState::WRITE_COLOR_ATTACHMENT} });
		for (const auto &handle : m_lightRecordData[face].m_shadowMapViewHandles)
		{
			usageDescs.push_back({ handle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
		}
		graph->addPass("Irradiance Volume Render Probe Face", rg::QueueType::GRAPHICS, usageDescs.size(), usageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Irradiance Volume Render Probe Face");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_gpuProfilingCtx, cmdList, "Irradiance Volume Render Probe Face");
				PROFILING_ZONE_SCOPED;

				MeshRenderList2 renderList;
				data.m_meshRenderWorld->createMeshRenderList(viewMatrices[face], viewProjectionMatrix, m_volumes[curVolumeIdx].m_farPlane, &renderList);

				ColorAttachmentDescription attachmentDescs[]
				{
					 { registry.getImageView(colorImageViewHandles[face]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
					 { registry.getImageView(distanceImageViewHandles[face]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE },
				};
				DepthStencilAttachmentDescription depthBufferDesc{ m_depthBufferImageView, AttachmentLoadOp::CLEAR, AttachmentStoreOp::DONT_CARE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE };
				Rect renderRect{ {0, 0}, {k_renderResolution, k_renderResolution} };

				cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)k_renderResolution, (float)k_renderResolution, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {k_renderResolution, k_renderResolution} };
					cmdList->setScissor(0, 1, &scissor);

					// forward
					{
						struct PassConstants
						{
							float viewProjectionMatrix[16];
							float viewMatrixDepthRow[4];
							float cameraPosition[3];
							uint32_t transformBufferIndex;
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
							uint32_t irradianceVolumeCount;
							uint32_t irradianceVolumeBufferIndex;
							float maxDistance;
						};

						PassConstants passConsts;
						memcpy(passConsts.viewProjectionMatrix, &viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
						passConsts.viewMatrixDepthRow[0] = viewMatrices[face][0][2];
						passConsts.viewMatrixDepthRow[1] = viewMatrices[face][1][2];
						passConsts.viewMatrixDepthRow[2] = viewMatrices[face][2][2];
						passConsts.viewMatrixDepthRow[3] = viewMatrices[face][3][2];
						memcpy(passConsts.cameraPosition, &probeWorldSpacePos.x, sizeof(passConsts.cameraPosition));
						passConsts.transformBufferIndex = renderList.m_transformsBufferViewHandle;
						passConsts.materialBufferIndex = data.m_materialsBufferHandle;
						passConsts.directionalLightCount = m_lightRecordData[face].m_directionalLightCount;
						passConsts.directionalLightBufferIndex = m_lightRecordData[face].m_directionalLightsBufferViewHandle;
						passConsts.directionalLightShadowedCount = m_lightRecordData[face].m_directionalLightShadowedCount;
						passConsts.directionalLightShadowedBufferIndex = m_lightRecordData[face].m_directionalLightsShadowedBufferViewHandle;
						passConsts.punctualLightCount = m_lightRecordData[face].m_punctualLightCount;
						passConsts.punctualLightBufferIndex = m_lightRecordData[face].m_punctualLightsBufferViewHandle;
						passConsts.punctualLightTileTextureIndex = m_lightRecordData[face].m_punctualLightsTileTextureViewHandle;
						passConsts.punctualLightDepthBinsBufferIndex = m_lightRecordData[face].m_punctualLightsDepthBinsBufferViewHandle;
						passConsts.punctualLightShadowedCount = m_lightRecordData[face].m_punctualLightShadowedCount;
						passConsts.punctualLightShadowedBufferIndex = m_lightRecordData[face].m_punctualLightsShadowedBufferViewHandle;
						passConsts.punctualLightShadowedTileTextureIndex = m_lightRecordData[face].m_punctualLightsShadowedTileTextureViewHandle;
						passConsts.punctualLightShadowedDepthBinsBufferIndex = m_lightRecordData[face].m_punctualLightsShadowedDepthBinsBufferViewHandle;
						passConsts.irradianceVolumeCount = activeIrradianceVolumeCount;
						passConsts.irradianceVolumeBufferIndex = m_irradianceVolumeBufferHandle;
						passConsts.maxDistance = maxProbeDistance;

						uint32_t passConstsAddress = (uint32_t)data.m_constantBufferLinearAllocator->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

						GraphicsPipeline *pipelines[]
						{
							m_forwardPipeline,
							m_forwardAlphaTestedPipeline,
						};

						for (size_t alphaTested = 0; alphaTested < 2; ++alphaTested)
						{
							for (size_t outlined = 0; outlined < 2; ++outlined)
							{
								const size_t listIdx = MeshRenderList2::getListIndex(false, alphaTested == 0 ? MaterialAlphaMode::Opaque : MaterialAlphaMode::Mask, false, outlined != 0);

								if (renderList.m_counts[listIdx] == 0)
								{
									continue;
								}
								auto *pipeline = alphaTested == 0 ? m_forwardPipeline : m_forwardAlphaTestedPipeline;

								cmdList->bindPipeline(pipeline);

								gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, m_viewRegistry->getCurrentFrameDescriptorSet() };
								cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

								for (size_t i = 0; i < renderList.m_counts[listIdx]; ++i)
								{
									const auto &instance = renderList.m_submeshInstances[renderList.m_indices[renderList.m_offsets[listIdx] + i]];

									ForwardPushConstants consts{};
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
					}

					// sky
					{
						cmdList->bindPipeline(m_skyPipeline);
						auto *bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
						cmdList->bindDescriptorSets(m_skyPipeline, 0, 1, &bindlessSet, 0, nullptr);

						glm::mat4 invViewProjectionMatrix = glm::inverse(viewProjectionMatrix);

						SkyPushConsts skyPushConsts{};
						memcpy(skyPushConsts.invModelViewProjection, &invViewProjectionMatrix[0][0], sizeof(skyPushConsts.invModelViewProjection));
						skyPushConsts.exposureBufferIndex = 0; // not actually used

						cmdList->pushConstants(m_skyPipeline, ShaderStageFlags::ALL_STAGES, 0, sizeof(skyPushConsts), &skyPushConsts);

						cmdList->draw(3, 1, 0, 0);
					}
				}
				cmdList->endRenderPass();
			});
	}



	eastl::fixed_vector<rg::ResourceUsageDesc, 4> filterUsageDescs;
	filterUsageDescs.push_back({ colorImageCubeViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	filterUsageDescs.push_back({ tmpDiffuseImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	if (firstBounce)
	{
		filterUsageDescs.push_back({ distanceImageCubeViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
		filterUsageDescs.push_back({ tmpVisibilityImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	}
	graph->addPass("Irradiance Volume Filter", rg::QueueType::GRAPHICS, filterUsageDescs.size(), filterUsageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Irradiance Volume Filter");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_gpuProfilingCtx, cmdList, "Irradiance Volume Filter");
			PROFILING_ZONE_SCOPED;

			// diffuse
			{
				cmdList->bindPipeline(m_diffuseFilterPipeline);
				auto *bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
				cmdList->bindDescriptorSets(m_diffuseFilterPipeline, 0, 1, &bindlessSet, 0, nullptr);

				ProbeFilterPushConsts pushConsts{};
				pushConsts.baseOutputOffset[0] = (probeIdxY * m_volumes[curVolumeIdx].m_resolutionX + probeIdxX) * (k_diffuseProbeResolution + 2);
				pushConsts.baseOutputOffset[1] = (probeIdxZ) * (k_diffuseProbeResolution + 2);
				pushConsts.texelSize = 1.0f / k_diffuseProbeResolution;
				pushConsts.inputTextureIndex = registry.getBindlessHandle(colorImageCubeViewHandle, DescriptorType::TEXTURE);
				pushConsts.resultTextureIndex = registry.getBindlessHandle(tmpDiffuseImageViewHandle, DescriptorType::RW_TEXTURE);
				pushConsts.maxDistance = maxProbeDistance;

				cmdList->pushConstants(m_diffuseFilterPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch(1, 1, 1);
			}

			// visibility
			if (firstBounce)
			{
				cmdList->bindPipeline(m_visibilityFilterPipeline);
				auto *bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
				cmdList->bindDescriptorSets(m_visibilityFilterPipeline, 0, 1, &bindlessSet, 0, nullptr);

				ProbeFilterPushConsts pushConsts{};
				pushConsts.baseOutputOffset[0] = (probeIdxY * m_volumes[curVolumeIdx].m_resolutionX + probeIdxX) * (k_visibilityProbeResolution + 2);
				pushConsts.baseOutputOffset[1] = (probeIdxZ) * (k_visibilityProbeResolution + 2);
				pushConsts.texelSize = 1.0f / k_visibilityProbeResolution;
				pushConsts.inputTextureIndex = registry.getBindlessHandle(distanceImageCubeViewHandle, DescriptorType::TEXTURE);
				pushConsts.resultTextureIndex = registry.getBindlessHandle(tmpVisibilityImageViewHandle, DescriptorType::RW_TEXTURE);
				pushConsts.maxDistance = maxProbeDistance;

				cmdList->pushConstants(m_visibilityFilterPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch(1, 1, 1);
			}
		});

	++m_curProbeIdx;

	// finished volume
	if ((probeIdxX + 1) == curVolume.m_resolutionX &&
		(probeIdxY + 1) == curVolume.m_resolutionY &&
		(probeIdxZ + 1) == curVolume.m_resolutionZ)
	{
		eastl::fixed_vector<rg::ResourceUsageDesc, 4> copyUsageDescs;
		copyUsageDescs.push_back({ tmpDiffuseImageViewHandle, {ResourceState::READ_TRANSFER} });
		copyUsageDescs.push_back({ tmpVisibilityImageViewHandle, {ResourceState::READ_TRANSFER} });
		graph->addPass("Irradiance Volume Copy", rg::QueueType::GRAPHICS, copyUsageDescs.size(), copyUsageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
			{
				{
					eastl::fixed_vector<Barrier, 2> barriers;
					barriers.push_back(Initializers::imageBarrier(m_volumes[curVolumeIdx].m_diffuseImage, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, PipelineStageFlags::TRANSFER_BIT, ResourceState::READ_RESOURCE, ResourceState::WRITE_TRANSFER));
					if (firstBounce)
					{
						barriers.push_back(Initializers::imageBarrier(m_volumes[curVolumeIdx].m_visibilityImage, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, PipelineStageFlags::TRANSFER_BIT, ResourceState::READ_RESOURCE, ResourceState::WRITE_TRANSFER));
					}
					cmdList->barrier(static_cast<uint32_t>(barriers.size()), barriers.data());
				}

				// diffuse
				{
					ImageCopy imageCopy{};
					imageCopy.m_srcLayerCount = 1;
					imageCopy.m_dstLayerCount = 1;
					imageCopy.m_extent = { m_volumes[curVolumeIdx].m_resolutionX * m_volumes[curVolumeIdx].m_resolutionY * (k_diffuseProbeResolution + 2), m_volumes[curVolumeIdx].m_resolutionZ * (k_diffuseProbeResolution + 2), 1 };
					cmdList->copyImage(m_tmpIrradianceVolumeDiffuseImage, m_volumes[curVolumeIdx].m_diffuseImage, 1, &imageCopy);
				}

				// visibility
				if (firstBounce)
				{
					ImageCopy imageCopy{};
					imageCopy.m_srcLayerCount = 1;
					imageCopy.m_dstLayerCount = 1;
					imageCopy.m_extent = { m_volumes[curVolumeIdx].m_resolutionX * m_volumes[curVolumeIdx].m_resolutionY * (k_visibilityProbeResolution + 2), m_volumes[curVolumeIdx].m_resolutionZ * (k_visibilityProbeResolution + 2), 1 };
					cmdList->copyImage(m_tmpIrradianceVolumeVisibilityImage, m_volumes[curVolumeIdx].m_visibilityImage, 1, &imageCopy);
				}

				{
					eastl::fixed_vector<Barrier, 2> barriers;
					barriers.push_back(Initializers::imageBarrier(m_volumes[curVolumeIdx].m_diffuseImage, PipelineStageFlags::TRANSFER_BIT, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::WRITE_TRANSFER, ResourceState::READ_RESOURCE));
					if (firstBounce)
					{
						barriers.push_back(Initializers::imageBarrier(m_volumes[curVolumeIdx].m_visibilityImage, PipelineStageFlags::TRANSFER_BIT, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::WRITE_TRANSFER, ResourceState::READ_RESOURCE));
					}
					cmdList->barrier(static_cast<uint32_t>(barriers.size()), barriers.data());
				}
			});

		// only do this on the last bounce
		if ((m_curBounce + 1) == m_targetBounces)
		{
			graph->addPass("Irradiance Volume Avg Filter", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
				{
					{
						Barrier b = Initializers::imageBarrier(m_volumes[curVolumeIdx].m_averageDiffuseImage, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::READ_RESOURCE, ResourceState::RW_RESOURCE_WRITE_ONLY);
						cmdList->barrier(1, &b);
					}

					cmdList->bindPipeline(m_averageFilterPipeline);
					auto *bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
					cmdList->bindDescriptorSets(m_averageFilterPipeline, 0, 1, &bindlessSet, 0, nullptr);

					ProbeAverageFilterPushConsts pushConsts{};
					pushConsts.resolution[0] = m_volumes[curVolumeIdx].m_resolutionX * m_volumes[curVolumeIdx].m_resolutionY;
					pushConsts.resolution[1] = m_volumes[curVolumeIdx].m_resolutionZ;
					pushConsts.inputTextureIndex = m_volumes[curVolumeIdx].m_diffuseImageViewHandle;
					pushConsts.resultTextureIndex = m_volumes[curVolumeIdx].m_averageDiffuseImageRWViewHandle;

					cmdList->pushConstants(m_averageFilterPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					cmdList->dispatch((pushConsts.resolution[0] + 7) / 8, (pushConsts.resolution[1] + 7) / 8, 1);

					{
						Barrier b = Initializers::imageBarrier(m_volumes[curVolumeIdx].m_averageDiffuseImage, PipelineStageFlags::COMPUTE_SHADER_BIT, PipelineStageFlags::PIXEL_SHADER_BIT | PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::RW_RESOURCE_WRITE_ONLY, ResourceState::READ_RESOURCE);
						cmdList->barrier(1, &b);
					}
				});
		}

		// go to next volume
		++m_curVolumeIdx;
		m_curProbeIdx = 0;

		// we finished a bounce
		if (m_curVolumeIdx == m_volumes.size())
		{
			++m_curBounce;
			m_curVolumeIdx = 0;
		}
	}

	++m_processedProbes;

	// finished baking
	if (m_curBounce == m_targetBounces)
	{
		m_baking = false;
	}
}

StructuredBufferViewHandle IrradianceVolumeManager::getIrradianceVolumeBufferViewHandle() const noexcept
{
	return m_irradianceVolumeBufferHandle;
}

uint32_t IrradianceVolumeManager::getIrradianceVolumeCount() const noexcept
{
	return (m_baking && (m_curBounce == 0)) ? 0 : static_cast<uint32_t>(m_sortedGPUVolumes.size());
}

uint32_t IrradianceVolumeManager::getProcessedProbesCount() const noexcept
{
	return m_processedProbes;
}

bool IrradianceVolumeManager::startBake() noexcept
{
	if (!m_startedBake && !m_baking)
	{
		m_startedBake = true;
		return true;
	}
	return false;
}

bool IrradianceVolumeManager::abortBake() noexcept
{
	if (m_startedBake || m_baking)
	{
		m_abortedBake = true;
		return true;
	}
	return false;
}

bool IrradianceVolumeManager::isBaking() const noexcept
{
	return m_startedBake || m_baking;
}

uint32_t IrradianceVolumeManager::getTotalProbeCount() const noexcept
{
	return m_totalProbes;
}