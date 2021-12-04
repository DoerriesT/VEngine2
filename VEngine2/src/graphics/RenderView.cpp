#include "RenderView.h"
#include "pass/ShadowModule.h"
#include "pass/ForwardModule.h"
#include "pass/PostProcessModule.h"
#include "pass/GridPass.h"
#include "RenderViewResources.h"
#include "ResourceViewRegistry.h"
#include "gal/Initializers.h"
#include <glm/gtc/type_ptr.hpp>
#include "component/TransformComponent.h"
#include "component/MeshComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "component/LightComponent.h"
#include "component/CameraComponent.h"
#include "component/OutlineComponent.h"
#include <glm/gtx/transform.hpp>
#include "MeshManager.h"
#include "MaterialManager.h"
#include "animation/Skeleton.h"
#include "RenderGraph.h"
#include "RendererResources.h"
#include "BufferStackAllocator.h"
#include "RenderWorld.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/numeric.h>
#include <EASTL/sort.h>
#include <glm/gtx/quaternion.hpp>

using namespace gal;

static void calculateCascadeViewProjectionMatrices(const glm::vec3 &lightDir,
	float nearPlane,
	float farPlane,
	float fovy,
	const glm::mat4 &invViewMatrix,
	uint32_t width,
	uint32_t height,
	float maxShadowDistance,
	float splitLambda,
	float shadowTextureSize,
	size_t cascadeCount,
	glm::mat4 *viewProjectionMatrices,
	glm::vec4 *cascadeParams,
	glm::vec4 *viewMatrixDepthRows)
{
	float splits[LightComponent::k_maxCascades];

	assert(cascadeCount <= LightComponent::k_maxCascades);

	// compute split distances
	farPlane = fminf(farPlane, maxShadowDistance);
	const float range = farPlane - nearPlane;
	const float ratio = farPlane / nearPlane;
	const float invCascadeCount = 1.0f / cascadeCount;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		float p = (i + 1) * invCascadeCount;
		float log = nearPlane * std::pow(ratio, p);
		float uniform = nearPlane + range * p;
		splits[i] = splitLambda * (log - uniform) + uniform;
	}

	const float aspectRatio = width * (1.0f / height);
	float previousSplit = nearPlane;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
		const float n = previousSplit;
		const float f = splits[i];
		const float k = sqrtf(1.0f + aspectRatio * aspectRatio) * tanf(fovy * 0.5f);

		glm::vec3 center;
		float radius;
		const float k2 = k * k;
		const float fnSum = f + n;
		const float fnDiff = f - n;
		const float fnSum2 = fnSum * fnSum;
		const float fnDiff2 = fnDiff * fnDiff;
		if (k2 >= fnDiff / fnSum)
		{
			center = glm::vec3(0.0f, 0.0f, -f);
			radius = f * k;
		}
		else
		{
			center = glm::vec3(0.0f, 0.0f, -0.5f * fnSum * (1.0f + k2));
			radius = 0.5f * sqrtf(fnDiff2 + 2 * (f * f + n * n) * k2 + fnSum2 * k2 * k2);
		}
		glm::vec4 tmp = invViewMatrix * glm::vec4(center, 1.0f);
		center = glm::vec3(tmp / tmp.w);

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (fabsf(lightDir.x) < 0.001f && fabsf(lightDir.z) < 0.001f)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		glm::mat4 lightView = glm::lookAt(center + lightDir * 150.0f, center, upDir);

		constexpr float depthRange = 300.0f;
		constexpr float precisionRange = 65536.0f;

		// snap to shadow map texel to avoid shimmering
		lightView[3].x -= fmodf(lightView[3].x, (radius / 256.0f) * 2.0f);
		lightView[3].y -= fmodf(lightView[3].y, (radius / 256.0f) * 2.0f);
		lightView[3].z -= fmodf(lightView[3].z, depthRange / precisionRange);

		viewMatrixDepthRows[i] = glm::vec4(lightView[0][2], lightView[1][2], lightView[2][2], lightView[3][2]);

		viewProjectionMatrices[i] = glm::ortho(-radius, radius, -radius, radius, 0.0f, depthRange) * lightView;

		// depthNormalBiases[i] holds the depth/normal offset biases in texel units
		const float unitsPerTexel = radius * 2.0f / shadowTextureSize;
		cascadeParams[i].x = unitsPerTexel * -cascadeParams[i].x / depthRange;
		cascadeParams[i].y = unitsPerTexel * cascadeParams[i].y;
		cascadeParams[i].z = 1.0f / unitsPerTexel;

		previousSplit = splits[i];
	}
}

RenderView::RenderView(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager, RendererResources *rendererResources, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry),
	m_meshManager(meshManager),
	m_materialManager(materialManager),
	m_rendererResources(rendererResources),
	m_width(width),
	m_height(height)
{
	m_renderViewResources = new RenderViewResources(m_device, viewRegistry, m_width, m_height);
	m_shadowModule = new ShadowModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_forwardModule = new ForwardModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_postProcessModule = new PostProcessModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_gridPass = new GridPass(m_device, offsetBufferSetLayout);
}

RenderView::~RenderView()
{
	delete m_shadowModule;
	delete m_forwardModule;
	delete m_postProcessModule;
	delete m_gridPass;
	delete m_renderViewResources;
}

void RenderView::render(
	float deltaTime,
	float time,
	const RenderWorld &renderWorld,
	rg::RenderGraph *graph,
	BufferStackAllocator *bufferAllocator,
	gal::DescriptorSet *offsetBufferSet,
	const float *viewMatrix,
	const float *projectionMatrix,
	const float *cameraPosition,
	const CameraComponent *cameraComponent,
	uint32_t pickingPosX,
	uint32_t pickingPosY
) noexcept
{
	if (m_width == 0 || m_height == 0)
	{
		return;
	}

	const size_t resIdx = m_frame & 1;
	const size_t prevResIdx = (m_frame + 1) & 1;

	// read back picking data
	{
		auto *buffer = m_renderViewResources->m_pickingDataReadbackBuffers[resIdx];
		uint32_t *pickingReadbackData = nullptr;
		buffer->map((void **)&pickingReadbackData);
		{
			MemoryRange range{ 0, sizeof(uint32_t) * 4 };
			buffer->invalidate(1, &range);
			m_pickedEntity = pickingReadbackData[1]; // [0] is depth, [1] is the picked entity
		}
		buffer->unmap();
	}

	const glm::mat4 viewMat = glm::make_mat4(viewMatrix);
	const glm::mat4 projMat = glm::make_mat4(projectionMatrix);
	const glm::mat4 viewProjMat = projMat * viewMat;
	const glm::vec4 viewMatDepthRow = glm::vec4(viewMat[0][2], viewMat[1][2], viewMat[2][2], viewMat[3][2]);
	const glm::vec3 cameraPos = glm::make_vec3(cameraPosition);
	const glm::vec3 gridPos = glm::vec3(cameraPos.x, 0.0f, cameraPos.z);

	// result image
	rg::ResourceHandle resultImageHandle = graph->importImage(m_renderViewResources->m_resultImage, "Render View Result", m_renderViewResources->m_resultImageState);
	rg::ResourceViewHandle resultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Render View Result", resultImageHandle, graph));
	m_resultImageViewHandle = resultImageViewHandle;

	// exposure buffer
	rg::ResourceHandle exposureBufferHandle = graph->importBuffer(m_renderViewResources->m_exposureDataBuffer, "Exposure Buffer", m_renderViewResources->m_exposureDataBufferState);
	rg::ResourceViewHandle exposureBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::create("Exposure Buffer", exposureBufferHandle, sizeof(float) * 4, 0, sizeof(float)));

	// picking readback buffer
	rg::ResourceHandle pickingReadbackBufferHandle = graph->importBuffer(m_renderViewResources->m_pickingDataReadbackBuffers[resIdx], "Picking Readback Buffer", &m_renderViewResources->m_pickingDataReadbackBufferStates[resIdx]);
	rg::ResourceViewHandle pickingReadbackBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::createDefault("Picking Readback Buffer", pickingReadbackBufferHandle, graph));

	// picking buffer
	rg::ResourceHandle pickingBufferHandle = graph->createBuffer(rg::BufferDesc::create("Picking Buffer", sizeof(uint32_t) * 4, gal::BufferUsageFlags::RW_BYTE_BUFFER_BIT | gal::BufferUsageFlags::TRANSFER_SRC_BIT | gal::BufferUsageFlags::CLEAR_BIT));
	rg::ResourceViewHandle pickingBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::createDefault("Picking Buffer", pickingBufferHandle, graph));

	m_modelMatrices.clear();
	m_shadowMatrices.clear();
	m_shadowTextureRenderHandles.clear();
	m_shadowTextureSampleHandles.clear();
	m_globalMedia.clear();
	m_directionalLights.clear();
	m_shadowedDirectionalLights.clear();
	m_punctualLights.clear();
	m_punctualLightsOrder.clear();
	m_lightTransforms.clear();
	m_renderList.clear();
	m_outlineRenderList.clear();

	const glm::mat4 invViewMatrix = glm::inverse(glm::make_mat4(viewMatrix));

	// process directional lights
	for (const auto &dirLight : renderWorld.m_directionalLights)
	{
		DirectionalLightGPU directionalLight{};
		directionalLight.m_color = glm::vec3(glm::unpackUnorm4x8(dirLight.m_color)) * dirLight.m_intensity;
		directionalLight.m_direction = dirLight.m_direction;
		directionalLight.m_cascadeCount = dirLight.m_shadowsEnabled ? dirLight.m_cascadeCount : 0;

		if (dirLight.m_shadowsEnabled)
		{
			glm::vec4 cascadeParams[LightComponent::k_maxCascades];
			glm::vec4 depthRows[LightComponent::k_maxCascades];

			assert(dirLight.m_cascadeCount <= LightComponent::k_maxCascades);

			for (size_t j = 0; j < dirLight.m_cascadeCount; ++j)
			{
				cascadeParams[j].x = dirLight.m_depthBias[j];
				cascadeParams[j].y = dirLight.m_normalOffsetBias[j];
			}

			calculateCascadeViewProjectionMatrices(directionalLight.m_direction,
				cameraComponent->m_near,
				cameraComponent->m_far,
				cameraComponent->m_fovy,
				invViewMatrix,
				m_width,
				m_height,
				dirLight.m_maxShadowDistance,
				dirLight.m_splitLambda,
				2048.0f,
				dirLight.m_cascadeCount,
				directionalLight.m_shadowMatrices,
				cascadeParams,
				depthRows);

			rg::ResourceHandle shadowMapHandle = graph->createImage(rg::ImageDesc::create("Cascaded Shadow Maps", Format::D16_UNORM, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, 2048, 2048, 1, dirLight.m_cascadeCount));
			rg::ResourceViewHandle shadowMapViewHandle = graph->createImageView(rg::ImageViewDesc::create("Cascaded Shadow Maps", shadowMapHandle, { 0, 1, 0, dirLight.m_cascadeCount }, ImageViewType::_2D_ARRAY));
			static_assert(sizeof(TextureViewHandle) == sizeof(shadowMapViewHandle));
			directionalLight.m_shadowTextureHandle = static_cast<TextureViewHandle>(shadowMapViewHandle); // we later correct these to be actual TextureViewHandles
			m_shadowTextureSampleHandles.push_back(shadowMapViewHandle);

			for (size_t j = 0; j < dirLight.m_cascadeCount; ++j)
			{
				m_shadowMatrices.push_back(directionalLight.m_shadowMatrices[j]);
				m_shadowTextureRenderHandles.push_back(graph->createImageView(rg::ImageViewDesc::create("Shadow Cascade", shadowMapHandle, { 0, 1, static_cast<uint32_t>(j), 1 }, ImageViewType::_2D)));
			}

			m_shadowedDirectionalLights.push_back(directionalLight);
		}
		else
		{
			m_directionalLights.push_back(directionalLight);
		}
	}

	// process punctual lights
	for (const auto &light : renderWorld.m_punctualLights)
	{
		float intensity = light.m_spotLight ? (1.0f / (glm::pi<float>())) : (1.0f / (4.0f * glm::pi<float>()));
		intensity *= light.m_intensity;
		PunctualLightGPU punctualLight{};
		punctualLight.m_color = glm::vec3(glm::unpackUnorm4x8(light.m_color)) * intensity;
		punctualLight.m_invSqrAttRadius = 1.0f / (light.m_radius * light.m_radius);
		punctualLight.m_position = light.m_position;
		if (light.m_spotLight)
		{
			punctualLight.m_angleScale = 1.0f / fmaxf(0.001f, cosf(light.m_innerAngle * 0.5f) - cosf(light.m_outerAngle * 0.5f));
			punctualLight.m_angleOffset = -cosf(light.m_outerAngle * 0.5f) * punctualLight.m_angleScale;
			punctualLight.m_direction = light.m_direction;
		}
		else
		{
			punctualLight.m_angleScale = -1.0f; // special value to mark this as a point light
		}

		m_punctualLights.push_back(punctualLight);
		if (light.m_spotLight)
		{
			m_lightTransforms.push_back(viewProjMat * glm::translate(light.m_position) * glm::mat4_cast(glm::rotation(glm::vec3(0.0f, -1.0f, 0.0f), light.m_direction)) * glm::scale(glm::vec3(light.m_radius)));
		}
		else
		{
			m_lightTransforms.push_back(viewProjMat * glm::translate(light.m_position) * glm::scale(glm::vec3(light.m_radius)));
		}
	}

	// sort lights and assign to depth bins
	{
		union FloatUint32Union
		{
			float f;
			uint32_t ui;
		};

		m_punctualLightsOrder.resize(m_punctualLights.size());
		for (size_t i = 0; i < m_punctualLightsOrder.size(); ++i)
		{
			FloatUint32Union fu;
			fu.f = -glm::dot(glm::vec4(m_punctualLights[i].m_position, 1.0f), viewMatDepthRow);

			uint64_t packedDepthAndIndex = 0;
			packedDepthAndIndex |= static_cast<uint64_t>(i) & 0xFFFFFFFF; // index
			// floats interpreted as integers keep the less-than ordering, so we can just put the depth
			// in the most significant bits and use that to sort by depth
			packedDepthAndIndex |= static_cast<uint64_t>(fu.ui) << 32ull;
			m_punctualLightsOrder[i] = packedDepthAndIndex;
		}

		// sort by distance to camera
		eastl::sort(m_punctualLightsOrder.begin(), m_punctualLightsOrder.end());

		// clear bins
		constexpr size_t k_numDepthBins = 8192;
		constexpr float k_depthBinRange = 1.0f; // each bin covers a depth range of 1 unit
		constexpr uint32_t k_emptyBin = ((~0u & 0xFFFFu) << 16u);
		m_punctualLightsDepthBins.resize(k_numDepthBins);
		eastl::fill(m_punctualLightsDepthBins.begin(), m_punctualLightsDepthBins.end(), k_emptyBin);

		// assign lights to bins
		for (size_t i = 0; i < m_punctualLightsOrder.size(); ++i)
		{
			FloatUint32Union fu;
			fu.ui = static_cast<uint32_t>(m_punctualLightsOrder[i] >> 32ull);
			float lightDepthVS = -fu.f;

			const size_t lightIndex = static_cast<size_t>(m_punctualLightsOrder[i] & 0xFFFFFFFF);

			const auto &light = m_punctualLights[lightIndex];
			const float radius = glm::sqrt(1.0f / light.m_invSqrAttRadius);
			float nearestPoint = -lightDepthVS - radius;
			float farthestPoint = -lightDepthVS + radius;

			size_t minBin = eastl::min(static_cast<size_t>(fmaxf(nearestPoint / k_depthBinRange, 0.0f)), size_t(k_numDepthBins - 1));
			size_t maxBin = eastl::min(static_cast<size_t>(fmaxf(farthestPoint / k_depthBinRange, 0.0f)), size_t(k_numDepthBins - 1));

			for (size_t j = minBin; j <= maxBin; ++j)
			{
				uint32_t &val = m_punctualLightsDepthBins[j];
				uint32_t minIndex = (val & 0xFFFF0000) >> 16;
				uint32_t maxIndex = val & 0xFFFF;
				minIndex = eastl::min(minIndex, static_cast<uint32_t>(i));
				maxIndex = eastl::max(maxIndex, static_cast<uint32_t>(i));
				val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
			}
		}
	}

	// process global participating media
	for (const auto &pm : renderWorld.m_globalParticipatingMedia)
	{
		GlobalParticipatingMediumGPU medium{};
		medium.emissive = glm::unpackUnorm4x8(pm.m_emissiveColor) * pm.m_emissiveIntensity;
		medium.extinction = pm.m_extinction;
		medium.scattering = glm::unpackUnorm4x8(pm.m_albedo) * pm.m_extinction;
		medium.phase = pm.m_phaseAnisotropy;
		medium.heightFogEnabled = pm.m_heightFogEnabled;
		medium.heightFogStart = pm.m_heightFogStart;
		medium.heightFogFalloff = pm.m_heightFogFalloff;
		medium.maxHeight = pm.m_maxHeight;
		medium.textureScale = pm.m_textureScale;
		medium.textureBias = glm::make_vec3(pm.m_textureBias);
		medium.densityTexture = pm.m_densityTextureHandle;

		m_globalMedia.push_back(medium);
	}

	// create transform matrices
	for (size_t i = 0; i < renderWorld.m_meshTransformsCount; ++i)
	{
		const auto &transform = renderWorld.m_interpolatedTransforms[i + renderWorld.m_meshTransformsOffset];
		glm::mat4 modelMatrix = glm::translate(transform.m_translation) * glm::mat4_cast(transform.m_rotation) * glm::scale(transform.m_scale);
		m_modelMatrices.push_back(modelMatrix);
	}

	// fill mesh render lists
	bool anyOutlines = false;
	for (const auto &mesh : renderWorld.m_meshes)
	{
		SubMeshInstanceData instanceData{};
		instanceData.m_subMeshHandle = mesh.m_subMeshHandle;
		instanceData.m_transformIndex = static_cast<uint32_t>(mesh.m_transformIndex - renderWorld.m_meshTransformsOffset);
		instanceData.m_skinningMatricesOffset = static_cast<uint32_t>(mesh.m_skinningMatricesOffset);
		instanceData.m_materialHandle = mesh.m_materialHandle;
		instanceData.m_entityID = mesh.m_entity;

		const bool alphaTested = m_materialManager->getMaterial(instanceData.m_materialHandle).m_alphaMode == MaterialCreateInfo::Alpha::Mask;
		const bool skinned = mesh.m_skinningMatricesCount != 0;

		if (alphaTested)
			if (skinned)
				m_renderList.m_opaqueSkinnedAlphaTested.push_back(instanceData);
			else
				m_renderList.m_opaqueAlphaTested.push_back(instanceData);
		else
			if (skinned)
				m_renderList.m_opaqueSkinned.push_back(instanceData);
			else
				m_renderList.m_opaque.push_back(instanceData);

		if (mesh.m_outlined)
		{
			anyOutlines = true;
			if (alphaTested)
				if (skinned)
					m_outlineRenderList.m_opaqueSkinnedAlphaTested.push_back(instanceData);
				else
					m_outlineRenderList.m_opaqueAlphaTested.push_back(instanceData);
			else
				if (skinned)
					m_outlineRenderList.m_opaqueSkinned.push_back(instanceData);
				else
					m_outlineRenderList.m_opaque.push_back(instanceData);
		}
	}

	auto createShaderResourceBuffer = [&](DescriptorType descriptorType, auto dataAsVector, bool copyNow = true, void **resultBufferPtr = nullptr, uint64_t *permutation = nullptr)
	{
		const size_t elementSize = sizeof(dataAsVector[0]);

		// prepare DescriptorBufferInfo
		DescriptorBufferInfo bufferInfo = Initializers::structuedBufferInfo(elementSize, dataAsVector.size());
		bufferInfo.m_buffer = m_rendererResources->m_shaderResourceBufferStackAllocators[resIdx]->getBuffer();

		// allocate memory
		uint64_t alignment = m_device->getBufferAlignment(descriptorType, elementSize);
		uint8_t *bufferPtr = m_rendererResources->m_shaderResourceBufferStackAllocators[resIdx]->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

		// copy to destination
		if (copyNow && !dataAsVector.empty())
		{
			if (permutation)
			{
				for (size_t i = 0; i < dataAsVector.size(); ++i)
				{
					size_t index = static_cast<size_t>(permutation[i] & 0xFFFFFFFF);
					memcpy(bufferPtr + i * elementSize, &dataAsVector[index], elementSize);
				}
			}
			else
			{
				memcpy(bufferPtr, dataAsVector.data(), dataAsVector.size() *elementSize);
			}
		}

		if (resultBufferPtr)
		{
			*resultBufferPtr = bufferPtr;
		}

		// create a transient bindless handle
		return descriptorType == DescriptorType::STRUCTURED_BUFFER ?
			m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true) : 
			m_viewRegistry->createByteBufferViewHandle(bufferInfo, true);
	};

	StructuredBufferViewHandle punctualLightsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_punctualLights, true, nullptr, m_punctualLightsOrder.data());
	ByteBufferViewHandle punctualLightsDepthBinsBufferViewHandle = (ByteBufferViewHandle)createShaderResourceBuffer(DescriptorType::BYTE_BUFFER, m_punctualLightsDepthBins);
	StructuredBufferViewHandle lightTransformsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_lightTransforms, true, nullptr, m_punctualLightsOrder.data());
	StructuredBufferViewHandle directionalLightsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_directionalLights);
	void *directionalLightsShadowedBufferPtr = nullptr;
	StructuredBufferViewHandle directionalLightsShadowedBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_shadowedDirectionalLights, false, &directionalLightsShadowedBufferPtr);
	StructuredBufferViewHandle skinningMatricesBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, renderWorld.m_skinningMatrices);

	StructuredBufferViewHandle globalMediaBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_globalMedia);

	// prepare data
	eastl::fixed_vector<rg::ResourceUsageDesc, 8> prepareFrameDataPassUsages;
	prepareFrameDataPassUsages.push_back({ pickingBufferViewHandle, {gal::ResourceState::CLEAR_RESOURCE} });
	if (m_frame == 0)
	{
		prepareFrameDataPassUsages.push_back({ exposureBufferViewHandle, {gal::ResourceState::WRITE_TRANSFER} });
	}
	graph->addPass("Prepare Frame Data", rg::QueueType::GRAPHICS, prepareFrameDataPassUsages.size(), prepareFrameDataPassUsages.data(), [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			// upload light data
			{
				// correct render graph handles to be actual bindless handles
				for (auto &dirLight : m_shadowedDirectionalLights)
				{
					dirLight.m_shadowTextureHandle = static_cast<TextureViewHandle>(registry.getBindlessHandle(static_cast<rg::ResourceViewHandle>(dirLight.m_shadowTextureHandle), DescriptorType::TEXTURE));
				}

				if (!m_shadowedDirectionalLights.empty())
				{
					memcpy(directionalLightsShadowedBufferPtr, m_shadowedDirectionalLights.data(), m_shadowedDirectionalLights.size() * sizeof(DirectionalLightGPU));
				}
			}

			// clear picking buffer
			cmdList->fillBuffer(registry.getBuffer(pickingBufferViewHandle), 0, sizeof(uint32_t) * 4, 0);

			// initialize exposure buffer
			if (m_frame == 0)
			{
				float data[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				cmdList->updateBuffer(registry.getBuffer(exposureBufferViewHandle), 0, sizeof(data), data);
			}
		});


	ShadowModule::Data shadowModuleData{};
	shadowModuleData.m_profilingCtx = m_device->getProfilingContext();
	shadowModuleData.m_bufferAllocator = bufferAllocator;
	shadowModuleData.m_offsetBufferSet = offsetBufferSet;
	shadowModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	shadowModuleData.m_skinningMatrixBufferHandle = skinningMatricesBufferViewHandle;
	shadowModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	shadowModuleData.m_shadowJobCount = m_shadowMatrices.size();
	shadowModuleData.m_shadowMatrices = m_shadowMatrices.data();
	shadowModuleData.m_shadowTextureViewHandles = m_shadowTextureRenderHandles.data();
	shadowModuleData.m_renderList = &m_renderList;
	shadowModuleData.m_modelMatrices = m_modelMatrices.data();
	shadowModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	shadowModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();

	m_shadowModule->record(graph, shadowModuleData);


	ForwardModule::ResultData forwardModuleResultData;
	ForwardModule::Data forwardModuleData{};
	forwardModuleData.m_profilingCtx = m_device->getProfilingContext();
	forwardModuleData.m_bufferAllocator = bufferAllocator;
	forwardModuleData.m_offsetBufferSet = offsetBufferSet;
	forwardModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	forwardModuleData.m_width = m_width;
	forwardModuleData.m_height = m_height;
	forwardModuleData.m_frame = m_frame;
	forwardModuleData.m_fovy = cameraComponent->m_fovy;
	forwardModuleData.m_nearPlane = cameraComponent->m_near;
	forwardModuleData.m_pickingPosX = pickingPosX;
	forwardModuleData.m_pickingPosY = pickingPosY;
	forwardModuleData.m_skinningMatrixBufferHandle = skinningMatricesBufferViewHandle;
	forwardModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	forwardModuleData.m_globalMediaBufferHandle = globalMediaBufferViewHandle;
	forwardModuleData.m_directionalLightsBufferHandle = directionalLightsBufferViewHandle;
	forwardModuleData.m_directionalLightsShadowedBufferHandle = directionalLightsShadowedBufferViewHandle;
	forwardModuleData.m_lightTransformBufferHandle = lightTransformsBufferViewHandle;
	forwardModuleData.m_punctualLightsBufferHandle = punctualLightsBufferViewHandle;
	forwardModuleData.m_punctualLightsDepthBinsBufferHandle = punctualLightsDepthBinsBufferViewHandle;
	forwardModuleData.m_pickingBufferHandle = pickingBufferViewHandle;
	forwardModuleData.m_exposureBufferHandle = exposureBufferViewHandle;
	forwardModuleData.m_shadowMapViewHandles = m_shadowTextureSampleHandles.data();
	forwardModuleData.m_globalMediaCount = static_cast<uint32_t>(m_globalMedia.size());
	forwardModuleData.m_directionalLightCount = static_cast<uint32_t>(m_directionalLights.size());
	forwardModuleData.m_directionalLightShadowedCount = static_cast<uint32_t>(m_shadowedDirectionalLights.size());
	forwardModuleData.m_punctualLightCount = static_cast<uint32_t>(m_punctualLights.size());
	forwardModuleData.m_shadowMapViewHandleCount = m_shadowTextureSampleHandles.size();
	forwardModuleData.m_viewProjectionMatrix = viewProjMat;
	forwardModuleData.m_invViewProjectionMatrix = glm::inverse(viewProjMat);
	forwardModuleData.m_viewMatrix = viewMat;
	forwardModuleData.m_invProjectionMatrix = glm::inverse(projMat);
	forwardModuleData.m_cameraPosition = cameraPos;
	forwardModuleData.m_renderList = &m_renderList;
	forwardModuleData.m_modelMatrices = m_modelMatrices.data();
	forwardModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	forwardModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	forwardModuleData.m_punctualLights = m_punctualLights.data();
	forwardModuleData.m_punctualLightsOrder = m_punctualLightsOrder.data();
	forwardModuleData.m_rendererResources = m_rendererResources;

	m_forwardModule->record(graph, forwardModuleData, &forwardModuleResultData);


	PostProcessModule::Data postProcessModuleData{};
	postProcessModuleData.m_profilingCtx = m_device->getProfilingContext();
	postProcessModuleData.m_bufferAllocator = bufferAllocator;
	postProcessModuleData.m_vertexBufferAllocator = m_rendererResources->m_vertexBufferStackAllocators[resIdx];
	postProcessModuleData.m_offsetBufferSet = offsetBufferSet;
	postProcessModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	postProcessModuleData.m_width = m_width;
	postProcessModuleData.m_height = m_height;
	postProcessModuleData.m_deltaTime = deltaTime;
	postProcessModuleData.m_time = time;
	postProcessModuleData.m_lightingImageView = forwardModuleResultData.m_lightingImageViewHandle;
	postProcessModuleData.m_depthBufferImageViewHandle = forwardModuleResultData.m_depthBufferImageViewHandle;
	postProcessModuleData.m_resultImageViewHandle = resultImageViewHandle;
	postProcessModuleData.m_exposureBufferViewHandle = exposureBufferViewHandle;
	postProcessModuleData.m_skinningMatrixBufferHandle = skinningMatricesBufferViewHandle;
	postProcessModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	postProcessModuleData.m_viewProjectionMatrix = viewProjMat;
	postProcessModuleData.m_cameraPosition = cameraPos;
	postProcessModuleData.m_renderList = &m_renderList;
	postProcessModuleData.m_outlineRenderList = &m_outlineRenderList;
	postProcessModuleData.m_modelMatrices = m_modelMatrices.data();
	postProcessModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	postProcessModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	postProcessModuleData.m_debugNormals = false;
	postProcessModuleData.m_renderOutlines = anyOutlines;

	m_postProcessModule->record(graph, postProcessModuleData, nullptr);


	GridPass::Data gridPassData{};
	gridPassData.m_profilingCtx = m_device->getProfilingContext();
	gridPassData.m_bufferAllocator = bufferAllocator;
	gridPassData.m_offsetBufferSet = offsetBufferSet;
	gridPassData.m_width = m_width;
	gridPassData.m_height = m_height;
	gridPassData.m_colorAttachment = resultImageViewHandle;
	gridPassData.m_depthBufferAttachment = forwardModuleResultData.m_depthBufferImageViewHandle;
	gridPassData.m_modelMatrix = glm::translate(glm::floor(gridPos * 0.1f) * 10.0f);
	gridPassData.m_viewProjectionMatrix = viewProjMat;
	gridPassData.m_thinLineColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
	gridPassData.m_thickLineColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
	gridPassData.m_cameraPos = cameraPos;
	gridPassData.m_cellSize = 1.0f;
	gridPassData.m_gridSize = 200.0f;

	m_gridPass->record(graph, gridPassData);

	// copy picking data to readback buffer
	rg::ResourceUsageDesc pickingBufferReadbackUsages[]
	{
		{ pickingBufferViewHandle, {gal::ResourceState::READ_TRANSFER} },
		{ pickingReadbackBufferViewHandle, {gal::ResourceState::WRITE_TRANSFER} },
	};
	graph->addPass("Readback Picking Buffer", rg::QueueType::GRAPHICS, eastl::size(pickingBufferReadbackUsages), pickingBufferReadbackUsages, [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			BufferCopy bufferCopy{};
			bufferCopy.m_srcOffset = 0;
			bufferCopy.m_dstOffset = 0;
			bufferCopy.m_size = sizeof(uint32_t) * 4;
			cmdList->copyBuffer(registry.getBuffer(pickingBufferViewHandle), registry.getBuffer(pickingReadbackBufferViewHandle), 1, &bufferCopy);
		});

	++m_frame;
}

void RenderView::resize(uint32_t width, uint32_t height) noexcept
{
	if (width != 0 && height != 0)
	{
		m_width = width;
		m_height = height;
		m_renderViewResources->resize(width, height);
	}
}

gal::Image *RenderView::getResultImage() const noexcept
{
	return m_renderViewResources->m_resultImage;
}

gal::ImageView *RenderView::getResultImageView() const noexcept
{
	return m_renderViewResources->m_resultImageView;
}

TextureViewHandle RenderView::getResultTextureViewHandle() const noexcept
{
	return m_renderViewResources->m_resultImageTextureViewHandle;
}

rg::ResourceViewHandle RenderView::getResultRGViewHandle() const noexcept
{
	return m_resultImageViewHandle;
}

EntityID RenderView::getPickedEntity() const noexcept
{
	return m_pickedEntity;
}

void RenderView::clearDebugGeometry() noexcept
{
	m_postProcessModule->clearDebugGeometry();
}

void RenderView::drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept
{
	m_postProcessModule->drawDebugLine(visibility, position0, position1, color0, color1);
}

void RenderView::drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept
{
	m_postProcessModule->drawDebugTriangle(visibility, position0, position1, position2, color0, color1, color2);
}
