#include "VolumetricFogModule.h"
#include <EASTL/fixed_vector.h>
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "utility/Utility.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace gal;

static constexpr size_t k_haltonSampleCount = 16;
static constexpr uint32_t k_imageWidth = 240;
static constexpr uint32_t k_imageHeight = 135;
static constexpr uint32_t k_imageDepth = 128;

VolumetricFogModule::VolumetricFogModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device),
	m_haltonJitter(new float[k_haltonSampleCount * 3])
{
	for (size_t i = 0; i < k_haltonSampleCount; ++i)
	{
		m_haltonJitter[i * 3 + 0] = util::halton(i + 1, 2);
		m_haltonJitter[i * 3 + 1] = util::halton(i + 1, 3);
		m_haltonJitter[i * 3 + 2] = util::halton(i + 1, 5);
	}

	// scatter
	{
		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::COMPUTE_BIT };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // 3d textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 1), // 2d array textures UI
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 2), // 2d array textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 3), // directional lights
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 4), // directional lights shadowed
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 5), // punctual lights
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 6), // global media
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 7), // local media
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 8), // exposure
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 9), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		gal::StaticSamplerDescription staticSamplerDescs[]
		{
			gal::Initializers::staticLinearClampSampler(0, 0, gal::ShaderStageFlags::COMPUTE_BIT),
			gal::Initializers::staticLinearClampSampler(1, 0, gal::ShaderStageFlags::COMPUTE_BIT),
		};
		staticSamplerDescs[1].m_compareEnable = true;
		staticSamplerDescs[1].m_compareOp = CompareOp::LESS_OR_EQUAL;

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/volumetricFogScatter_cs");
		builder.setPipelineLayoutDescription((uint32_t)eastl::size(layoutDecls), layoutDecls, 0, ShaderStageFlags::COMPUTE_BIT, 2, staticSamplerDescs, 2);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_scatterPipeline);
	}

	// filter
	{
		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::COMPUTE_BIT };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // 3d textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 1), // exposure
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 2), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		gal::StaticSamplerDescription staticSamplerDescs[]
		{
			gal::Initializers::staticLinearClampSampler(0, 0, gal::ShaderStageFlags::COMPUTE_BIT),
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/volumetricFogFilter_cs");
		builder.setPipelineLayoutDescription((uint32_t)eastl::size(layoutDecls), layoutDecls, 0, ShaderStageFlags::COMPUTE_BIT, 1, staticSamplerDescs, 2);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_filterPipeline);
	}

	// integrate
	{
		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::COMPUTE_BIT };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0), // 3d textures
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 1), // result
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/volumetricFogIntegrate_cs");
		builder.setPipelineLayoutDescription((uint32_t)eastl::size(layoutDecls), layoutDecls, 0, ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_integratePipeline);
	}

	// history images
	for (size_t i = 0; i < 2; ++i)
	{
		gal::ImageCreateInfo createInfo{};
		createInfo.m_width = k_imageWidth;
		createInfo.m_height = k_imageHeight;
		createInfo.m_depth = k_imageDepth;
		createInfo.m_format = gal::Format::R16G16B16A16_SFLOAT;
		createInfo.m_imageType = ImageType::_3D;
		createInfo.m_usageFlags = gal::ImageUsageFlags::TEXTURE_BIT | gal::ImageUsageFlags::RW_TEXTURE_BIT;

		m_device->createImage(createInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_historyTextures[i]);
		m_historyTextureState[i] = {};
	}
}

VolumetricFogModule::~VolumetricFogModule() noexcept
{
	delete[] m_haltonJitter;
	m_device->destroyComputePipeline(m_scatterPipeline);
	m_device->destroyComputePipeline(m_filterPipeline);
	m_device->destroyComputePipeline(m_integratePipeline);

	for (size_t i = 0; i < 2; ++i)
	{
		m_device->destroyImage(m_historyTextures[i]);
	}
}

void VolumetricFogModule::record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept
{
	const size_t resIdx = data.m_frame & 1;
	const size_t prevResIdx = (data.m_frame + 1) & 1;

	// scatter result image
	rg::ResourceHandle scatterResultImageHandle = graph->createImage(rg::ImageDesc::create("Volumetric Fog Scatter Result", Format::R16G16B16A16_SFLOAT, ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TEXTURE_BIT, k_imageWidth, k_imageHeight, k_imageDepth / 2, 1, 1, ImageType::_3D));
	rg::ResourceViewHandle scatterResultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Volumetric Fog Scatter Result", scatterResultImageHandle, graph));

	// filter history image
	rg::ResourceHandle filterHistoryImageHandle = graph->importImage(m_historyTextures[prevResIdx], "Volumetric Fog Filter Result", &m_historyTextureState[prevResIdx]);
	rg::ResourceViewHandle filterHistoryImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Volumetric Fog Filter Result", filterHistoryImageHandle, graph));

	// filter result image
	rg::ResourceHandle filterResultImageHandle = graph->importImage(m_historyTextures[resIdx], "Volumetric Fog Filter Result", &m_historyTextureState[resIdx]);
	rg::ResourceViewHandle filterResultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Volumetric Fog Filter Result", filterResultImageHandle, graph));

	// volumetric fog integrate result image
	rg::ResourceHandle integrateResultImageHandle = graph->createImage(rg::ImageDesc::create("Volumetric Fog Integrated", Format::R16G16B16A16_SFLOAT, ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TEXTURE_BIT, k_imageWidth, k_imageHeight, k_imageDepth, 1, 1, ImageType::_3D));
	rg::ResourceViewHandle integrateImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Volumetric Fog Integrated", integrateResultImageHandle, graph));
	resultData->m_volumetricFogImageViewHandle = integrateImageViewHandle;


	const size_t haltonIdx = data.m_frame % k_haltonSampleCount;
	const float jitterX = m_haltonJitter[haltonIdx * 3 + 0];
	const float jitterY = m_haltonJitter[haltonIdx * 3 + 1];
	const float jitterZ = m_haltonJitter[haltonIdx * 3 + 2];


	auto proj = glm::perspective(data.m_fovy, data.m_width / float(data.m_height), data.m_nearPlane, 64.0f);
	auto invViewProj = glm::inverse(proj * data.m_viewMatrix);
	glm::vec4 frustumCorners[4];
	frustumCorners[0] = invViewProj * glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f); // top left
	frustumCorners[1] = invViewProj * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // top right
	frustumCorners[2] = invViewProj * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f); // bottom left
	frustumCorners[3] = invViewProj * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f); // bottom right

	for (auto &c : frustumCorners)
	{
		c /= c.w;
		c -= glm::vec4(data.m_cameraPosition, 0.0f);
	}

	glm::mat4 prevProjMat = m_prevProjMatrix;
	glm::mat4 prevViewMat = m_prevViewMatrix;
	m_prevProjMatrix = proj;
	m_prevViewMatrix = data.m_viewMatrix;

	eastl::fixed_vector<rg::ResourceUsageDesc, 32 + 5> usageDescs;
	usageDescs.push_back({ scatterResultImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	usageDescs.push_back({ filterHistoryImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	usageDescs.push_back({ filterResultImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	usageDescs.push_back({ integrateImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	usageDescs.push_back({ data.m_exposureBufferHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	if (data.m_localMediaCount > 0)
	{
		usageDescs.push_back({ data.m_localMediaTileTextureViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	}
	if (data.m_punctualLightCount > 0)
	{
		usageDescs.push_back({ data.m_punctualLightsTileTextureViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT} });
	}
	for (size_t i = 0; i < data.m_shadowMapViewHandleCount; ++i)
	{
		usageDescs.push_back({ data.m_shadowMapViewHandles[i], {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
	}
	graph->addPass("Volumetric Fog", rg::QueueType::GRAPHICS, usageDescs.size(), usageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
		{
			struct Constants
			{
				glm::mat4 prevViewMatrix;
				glm::mat4 prevProjMatrix;
				glm::vec4 volumeResResultRes;
				glm::vec4 viewMatrixDepthRow;
				glm::vec3 frustumCornerTL;
				float volumeDepth;
				glm::vec3 frustumCornerTR;
				float volumeNear;
				glm::vec3 frustumCornerBL;
				float volumeFar;
				glm::vec3 frustumCornerBR;
				float jitterX;
				glm::vec3 cameraPos;
				float jitterY;
				float volumeTexelSize[2];
				float jitterZ;
				uint32_t directionalLightCount;
				uint32_t directionalLightBufferIndex;
				uint32_t directionalLightShadowedCount;
				uint32_t directionalLightShadowedBufferIndex;
				uint32_t punctualLightCount;
				uint32_t punctualLightBufferIndex;
				uint32_t punctualLightTileTextureIndex;
				uint32_t punctualLightDepthBinsBufferIndex;
				uint32_t globalMediaCount;
				uint32_t globalMediaBufferIndex;
				uint32_t localMediaCount;
				uint32_t localMediaBufferIndex;
				uint32_t localMediaTileTextureIndex;
				uint32_t localMediaDepthBinsBufferIndex;
				uint32_t exposureBufferIndex;
				uint32_t scatterResultTextureIndex;
				uint32_t filterInputTextureIndex;
				uint32_t filterHistoryTextureIndex;
				uint32_t filterResultTextureIndex;
				uint32_t integrateInputTextureIndex;
				uint32_t integrateResultTextureIndex;
				float filterAlpha;
				uint32_t checkerBoardCondition;
				uint32_t ignoreHistory;
			};

			Constants consts{};
			consts.prevViewMatrix = prevViewMat;
			consts.prevProjMatrix = prevProjMat;
			consts.volumeResResultRes = glm::vec4(k_imageWidth, k_imageHeight, data.m_width, data.m_height);
			consts.viewMatrixDepthRow = glm::vec4(data.m_viewMatrix[0][2], data.m_viewMatrix[1][2], data.m_viewMatrix[2][2], data.m_viewMatrix[3][2]);
			consts.frustumCornerTL = frustumCorners[0];
			consts.frustumCornerTR = frustumCorners[1];
			consts.frustumCornerBL = frustumCorners[2];
			consts.frustumCornerBR = frustumCorners[3];
			consts.volumeDepth = k_imageDepth;
			consts.volumeNear = 0.5f;
			consts.volumeFar = 64.0f;
			consts.jitterX = jitterX;
			consts.jitterY = jitterY;
			consts.jitterZ = jitterZ;
			consts.cameraPos = data.m_cameraPosition;
			consts.volumeTexelSize[0] = 1.0f / k_imageWidth;
			consts.volumeTexelSize[1] = 1.0f / k_imageHeight;
			consts.directionalLightCount = data.m_directionalLightCount;
			consts.directionalLightBufferIndex = data.m_directionalLightsBufferHandle;
			consts.directionalLightShadowedCount = data.m_directionalLightShadowedCount;
			consts.directionalLightShadowedBufferIndex = data.m_directionalLightsShadowedBufferHandle;
			consts.punctualLightCount = data.m_punctualLightCount;
			consts.punctualLightBufferIndex = data.m_punctualLightsBufferHandle;
			consts.punctualLightTileTextureIndex = data.m_punctualLightCount > 0 ? registry.getBindlessHandle(data.m_punctualLightsTileTextureViewHandle, DescriptorType::TEXTURE) : 0;
			consts.punctualLightDepthBinsBufferIndex = data.m_punctualLightsDepthBinsBufferHandle;
			consts.globalMediaCount = data.m_globalMediaCount;
			consts.globalMediaBufferIndex = data.m_globalMediaBufferHandle;
			consts.localMediaCount = data.m_localMediaBufferHandle;
			consts.localMediaBufferIndex = data.m_localMediaBufferHandle;
			consts.localMediaTileTextureIndex = consts.localMediaCount > 0 ? registry.getBindlessHandle(data.m_localMediaTileTextureViewHandle, DescriptorType::TEXTURE) : 0;
			consts.localMediaDepthBinsBufferIndex = data.m_localMediaDepthBinsBufferHandle;
			consts.exposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferHandle, DescriptorType::BYTE_BUFFER);
			consts.scatterResultTextureIndex = registry.getBindlessHandle(scatterResultImageViewHandle, DescriptorType::RW_TEXTURE);
			consts.filterInputTextureIndex = registry.getBindlessHandle(scatterResultImageViewHandle, DescriptorType::TEXTURE);
			consts.filterHistoryTextureIndex = registry.getBindlessHandle(filterHistoryImageViewHandle, DescriptorType::TEXTURE);
			consts.filterResultTextureIndex = registry.getBindlessHandle(filterResultImageViewHandle, DescriptorType::RW_TEXTURE);
			consts.integrateInputTextureIndex = registry.getBindlessHandle(filterResultImageViewHandle, DescriptorType::TEXTURE);
			consts.integrateResultTextureIndex = registry.getBindlessHandle(integrateImageViewHandle, DescriptorType::RW_TEXTURE);
			consts.filterAlpha = 0.05f;
			consts.checkerBoardCondition = data.m_frame & 1;
			consts.ignoreHistory = data.m_ignoreHistory;

			uint32_t constsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, consts);

			// scatter
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Volumetric Fog Scatter");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Volumetric Fog Scatter");
				PROFILING_ZONE_SCOPED;

				cmdList->bindPipeline(m_scatterPipeline);
				gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
				cmdList->bindDescriptorSets(m_scatterPipeline, 0, 2, sets, 1, &constsAddress);

				cmdList->dispatch((k_imageWidth + 3) / 4, (k_imageHeight + 3) / 4, ((k_imageDepth / 2) + 3) / 4);
			}

			{
				Barrier b = Initializers::imageBarrier(
					registry.getImage(scatterResultImageHandle),
					PipelineStageFlags::COMPUTE_SHADER_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT,
					ResourceState::RW_RESOURCE_WRITE_ONLY, ResourceState::READ_RESOURCE);
				cmdList->barrier(1, &b);
			}

			// filter
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Volumetric Fog Filter");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Volumetric Fog Filter");
				PROFILING_ZONE_SCOPED;

				cmdList->bindPipeline(m_filterPipeline);
				gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
				cmdList->bindDescriptorSets(m_filterPipeline, 0, 2, sets, 1, &constsAddress);

				cmdList->dispatch((k_imageWidth + 3) / 4, (k_imageHeight + 3) / 4, (k_imageDepth + 3) / 4);
			}

			{
				Barrier b = Initializers::imageBarrier(
					registry.getImage(filterResultImageHandle),
					PipelineStageFlags::COMPUTE_SHADER_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT,
					ResourceState::RW_RESOURCE_WRITE_ONLY, ResourceState::READ_RESOURCE);
				cmdList->barrier(1, &b);
			}

			// integrate
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Volumetric Fog Integrate");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Volumetric Fog Integrate");
				PROFILING_ZONE_SCOPED;

				cmdList->bindPipeline(m_integratePipeline);
				gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
				cmdList->bindDescriptorSets(m_integratePipeline, 0, 2, sets, 1, &constsAddress);

				cmdList->dispatch((k_imageWidth + 7) / 8, (k_imageHeight + 7) / 8, 1);
			}
		});
}
