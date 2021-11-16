#include "RenderView.h"
#include "pass/ShadowModule.h"
#include "pass/ForwardModule.h"
#include "pass/PostProcessModule.h"
#include "pass/GridPass.h"
#include "RenderViewResources.h"
#include "ResourceViewRegistry.h"
#include "gal/Initializers.h"
#include <glm/gtc/type_ptr.hpp>
#include "ecs/ECS.h"
#include "component/TransformComponent.h"
#include "component/MeshComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "component/LightComponent.h"
#include "component/CameraComponent.h"
#include <glm/gtx/transform.hpp>
#include "MeshManager.h"
#include "MaterialManager.h"
#include "animation/Skeleton.h"
#include "RenderGraph.h"
#include "RendererResources.h"
#include "BufferStackAllocator.h"

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

RenderView::RenderView(ECS *ecs, gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager, RendererResources *rendererResources, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept
	:m_ecs(ecs),
	m_device(device),
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
	rg::RenderGraph *graph,
	BufferStackAllocator *bufferAllocator,
	gal::DescriptorSet *offsetBufferSet,
	const float *viewMatrix,
	const float *projectionMatrix,
	const float *cameraPosition,
	const CameraComponent *cameraComponent,
	bool transitionResultToTexture) noexcept
{
	if (m_width == 0 || m_height == 0)
	{
		return;
	}

	const size_t resIdx = m_frame & 1;
	const size_t prevResIdx = (m_frame + 1) & 1;

	// result image
	rg::ResourceHandle resultImageHandle = graph->importImage(m_renderViewResources->m_resultImage, "Render View Result", m_renderViewResources->m_resultImageState);
	rg::ResourceViewHandle resultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Render View Result", resultImageHandle, graph));
	m_resultImageViewHandle = resultImageViewHandle;

	// exposure buffer
	rg::ResourceHandle exposureBufferHandle = graph->importBuffer(m_renderViewResources->m_exposureDataBuffer, "Exposure Buffer", m_renderViewResources->m_exposureDataBufferState);
	rg::ResourceViewHandle exposureBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::create("Exposure Buffer", exposureBufferHandle, sizeof(float) * 4, 0, sizeof(float)));

	m_modelMatrices.clear();
	m_shadowMatrices.clear();
	m_shadowTextureRenderHandles.clear();
	m_shadowTextureSampleHandles.clear();
	m_directionalLights.clear();
	m_shadowedDirectionalLights.clear();
	m_renderList.clear();

	const glm::mat4 invViewMatrix = glm::inverse(glm::make_mat4(viewMatrix));

	m_ecs->iterate<TransformComponent, LightComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, LightComponent *lightC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &lc = lightC[i];

				switch (lc.m_type)
				{
				case LightComponent::Type::Point:
				case LightComponent::Type::Spot:
					break;
				case LightComponent::Type::Directional:
				{
					DirectionalLightGPU directionalLight{};
					directionalLight.m_color = glm::vec3(glm::unpackUnorm4x8(lc.m_color)) * lc.m_intensity;
					directionalLight.m_direction = glm::normalize(tc.m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
					directionalLight.m_cascadeCount = lc.m_shadows ? lc.m_cascadeCount : 0;

					if (lc.m_shadows)
					{
						glm::vec4 cascadeParams[LightComponent::k_maxCascades];
						glm::vec4 depthRows[LightComponent::k_maxCascades];

						assert(lc.m_cascadeCount <= LightComponent::k_maxCascades);

						for (size_t j = 0; j < lc.m_cascadeCount; ++j)
						{
							cascadeParams[j].x = lc.m_depthBias[j];
							cascadeParams[j].y = lc.m_normalOffsetBias[j];
						}

						calculateCascadeViewProjectionMatrices(directionalLight.m_direction,
							cameraComponent->m_near,
							cameraComponent->m_far,
							cameraComponent->m_fovy,
							invViewMatrix,
							m_width,
							m_height,
							lc.m_maxShadowDistance,
							lc.m_splitLambda,
							2048.0f,
							lc.m_cascadeCount,
							directionalLight.m_shadowMatrices,
							cascadeParams,
							depthRows);

						rg::ResourceHandle shadowMapHandle = graph->createImage(rg::ImageDesc::create("Cascaded Shadow Maps", Format::D16_UNORM, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, 2048, 2048, 1, lc.m_cascadeCount));
						rg::ResourceViewHandle shadowMapViewHandle = graph->createImageView(rg::ImageViewDesc::create("Cascaded Shadow Maps", shadowMapHandle, {0, 1, 0, lc.m_cascadeCount}, ImageViewType::_2D_ARRAY));
						static_assert(sizeof(TextureViewHandle) == sizeof(shadowMapViewHandle));
						directionalLight.m_shadowTextureHandle = static_cast<TextureViewHandle>(shadowMapViewHandle); // we later correct these to be actual TextureViewHandles
						m_shadowTextureSampleHandles.push_back(shadowMapViewHandle);

						for (size_t j = 0; j < lc.m_cascadeCount; ++j)
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
					break;
				}
				default:
					assert(false);
					break;
				}
			}
		});

	auto createStructuredBuffer = [&](auto dataAsVector, bool copyNow = true, void **resultBufferPtr = nullptr) -> StructuredBufferViewHandle
	{
		const size_t elementSize = sizeof(dataAsVector[0]);

		// prepare DescriptorBufferInfo
		DescriptorBufferInfo bufferInfo = Initializers::structuedBufferInfo(elementSize, dataAsVector.size());
		bufferInfo.m_buffer = m_rendererResources->m_shaderResourceBufferStackAllocators[resIdx]->getBuffer();

		// allocate memory
		uint64_t alignment = m_device->getBufferAlignment(DescriptorType::STRUCTURED_BUFFER, elementSize);
		uint8_t *bufferPtr = m_rendererResources->m_shaderResourceBufferStackAllocators[resIdx]->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

		// copy to destination
		if (copyNow && !dataAsVector.empty())
		{
			memcpy(bufferPtr, dataAsVector.data(), dataAsVector.size() * elementSize);
		}

		if (resultBufferPtr)
		{
			*resultBufferPtr = bufferPtr;
		}

		// create a transient bindless handle
		return m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true);
	};

	StructuredBufferViewHandle directionalLightsBufferViewHandle = createStructuredBuffer(m_directionalLights);
	void *directionalLightsShadowedBufferPtr = nullptr;
	StructuredBufferViewHandle directionalLightsShadowedBufferViewHandle = createStructuredBuffer(m_shadowedDirectionalLights, false, &directionalLightsShadowedBufferPtr);

	const auto *subMeshDrawInfoTable = m_meshManager->getSubMeshDrawInfoTable();

	m_ecs->iterate<TransformComponent, MeshComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, MeshComponent *meshC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &mc = meshC[i];
				if (!mc.m_mesh.get())
				{
					continue;
				}
				glm::mat4 modelMatrix = glm::translate(tc.m_translation) * glm::mat4_cast(tc.m_rotation) * glm::scale(tc.m_scale);
				const auto &submeshhandles = mc.m_mesh->getSubMeshhandles();
				const auto &materialAssets = mc.m_mesh->getMaterials();
				for (size_t j = 0; j < submeshhandles.size(); ++j)
				{
					SubMeshInstanceData instanceData{};
					instanceData.m_subMeshHandle = submeshhandles[j];
					instanceData.m_transformIndex = static_cast<uint32_t>(m_modelMatrices.size());
					instanceData.m_skinningMatricesOffset = 0;
					instanceData.m_materialHandle = materialAssets[j]->getMaterialHandle();

					m_modelMatrices.push_back(modelMatrix);

					const bool skinned = subMeshDrawInfoTable[instanceData.m_subMeshHandle].m_skinned;
					const bool alphaTested = m_materialManager->getMaterial(instanceData.m_materialHandle).m_alphaMode == MaterialCreateInfo::Alpha::Mask;

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
				}

			}
		});

	uint32_t curSkinningMatrixOffset = 0;

	glm::mat4 *skinningMatricesBufferPtr = nullptr;
	m_renderViewResources->m_skinningMatricesBuffers[resIdx]->map((void **)&skinningMatricesBufferPtr);

	m_ecs->iterate<TransformComponent, SkinnedMeshComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, SkinnedMeshComponent *sMeshC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &smc = sMeshC[i];

				if (!smc.m_mesh->isSkinned())
				{
					continue;
				}

				assert(smc.m_matrixPalette.size() == smc.m_skeleton->getSkeleton()->getJointCount());

				glm::mat4 modelMatrix = glm::translate(tc.m_translation) * glm::mat4_cast(tc.m_rotation) * glm::scale(tc.m_scale);
				const auto &submeshhandles = smc.m_mesh->getSubMeshhandles();
				const auto &materialAssets = smc.m_mesh->getMaterials();
				for (size_t j = 0; j < submeshhandles.size(); ++j)
				{
					SubMeshInstanceData instanceData{};
					instanceData.m_subMeshHandle = submeshhandles[j];
					instanceData.m_transformIndex = static_cast<uint32_t>(m_modelMatrices.size());
					instanceData.m_skinningMatricesOffset = curSkinningMatrixOffset;
					instanceData.m_materialHandle = materialAssets[j]->getMaterialHandle();

					m_modelMatrices.push_back(modelMatrix);

					const bool skinned = subMeshDrawInfoTable[instanceData.m_subMeshHandle].m_skinned;
					const bool alphaTested = m_materialManager->getMaterial(instanceData.m_materialHandle).m_alphaMode == MaterialCreateInfo::Alpha::Mask;

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
				}

				assert(curSkinningMatrixOffset + smc.m_matrixPalette.size() <= 1024);

				memcpy(skinningMatricesBufferPtr + curSkinningMatrixOffset, smc.m_matrixPalette.data(), smc.m_matrixPalette.size() * sizeof(glm::mat4));
				curSkinningMatrixOffset += curSkinningMatrixOffset;
			}
		});

	m_renderViewResources->m_skinningMatricesBuffers[resIdx]->unmap();


	graph->addPass("Upload Light Data", rg::QueueType::GRAPHICS, 0, nullptr, [this, directionalLightsShadowedBufferPtr](gal::CommandList *cmdList, const rg::Registry &registry)
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
		});

	if (m_frame == 0)
	{
		rg::ResourceUsageDesc exposureBufferInitUsage = { exposureBufferViewHandle, {gal::ResourceState::WRITE_TRANSFER} };
		graph->addPass("Exposure Buffer Init", rg::QueueType::GRAPHICS, 1, &exposureBufferInitUsage, [=](gal::CommandList *cmdList, const rg::Registry &registry)
			{
				float data[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				cmdList->updateBuffer(registry.getBuffer(exposureBufferViewHandle), 0, sizeof(data), data);
			});
	}
	


	ShadowModule::Data shadowModuleData{};
	shadowModuleData.m_profilingCtx = m_device->getProfilingContext();
	shadowModuleData.m_bufferAllocator = bufferAllocator;
	shadowModuleData.m_offsetBufferSet = offsetBufferSet;
	shadowModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	shadowModuleData.m_skinningMatrixBufferHandle = m_renderViewResources->m_skinningMatricesBufferViewHandles[resIdx];
	shadowModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	shadowModuleData.m_shadowJobCount = m_shadowMatrices.size();
	shadowModuleData.m_shadowMatrices = m_shadowMatrices.data();
	shadowModuleData.m_shadowTextureViewHandles = m_shadowTextureRenderHandles.data();
	shadowModuleData.m_renderList = &m_renderList;
	shadowModuleData.m_modelMatrices = m_modelMatrices.data();
	shadowModuleData.m_meshDrawInfo = subMeshDrawInfoTable;
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
	forwardModuleData.m_skinningMatrixBufferHandle = m_renderViewResources->m_skinningMatricesBufferViewHandles[resIdx];
	forwardModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	forwardModuleData.m_directionalLightsBufferHandle = directionalLightsBufferViewHandle;
	forwardModuleData.m_directionalLightsShadowedBufferHandle = directionalLightsShadowedBufferViewHandle;
	forwardModuleData.m_exposureBufferHandle = exposureBufferViewHandle;
	forwardModuleData.m_shadowMapViewHandles = m_shadowTextureSampleHandles.data();
	forwardModuleData.m_directionalLightCount = static_cast<uint32_t>(m_directionalLights.size());
	forwardModuleData.m_directionalLightShadowedCount = static_cast<uint32_t>(m_shadowedDirectionalLights.size());
	forwardModuleData.m_shadowMapViewHandleCount = m_shadowTextureSampleHandles.size();
	forwardModuleData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	forwardModuleData.m_cameraPosition = glm::make_vec3(cameraPosition);
	forwardModuleData.m_renderList = &m_renderList;
	forwardModuleData.m_modelMatrices = m_modelMatrices.data();
	forwardModuleData.m_meshDrawInfo = subMeshDrawInfoTable;
	forwardModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();

	m_forwardModule->record(graph, forwardModuleData, &forwardModuleResultData);


	PostProcessModule::Data postProcessModuleData{};
	postProcessModuleData.m_profilingCtx = m_device->getProfilingContext();
	postProcessModuleData.m_bufferAllocator = bufferAllocator;
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
	postProcessModuleData.m_skinningMatrixBufferHandle = m_renderViewResources->m_skinningMatricesBufferViewHandles[resIdx];
	postProcessModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	postProcessModuleData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	postProcessModuleData.m_cameraPosition = glm::make_vec3(cameraPosition);
	postProcessModuleData.m_renderList = &m_renderList;
	postProcessModuleData.m_modelMatrices = m_modelMatrices.data();
	postProcessModuleData.m_meshDrawInfo = subMeshDrawInfoTable;
	postProcessModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	postProcessModuleData.m_debugNormals = false;

	m_postProcessModule->record(graph, postProcessModuleData, nullptr);


	GridPass::Data gridPassData{};
	gridPassData.m_profilingCtx = m_device->getProfilingContext();
	gridPassData.m_bufferAllocator = bufferAllocator;
	gridPassData.m_offsetBufferSet = offsetBufferSet;
	gridPassData.m_width = m_width;
	gridPassData.m_height = m_height;
	gridPassData.m_colorAttachment = resultImageViewHandle;
	gridPassData.m_depthBufferAttachment = forwardModuleResultData.m_depthBufferImageViewHandle;
	gridPassData.m_modelMatrix = glm::mat4(1.0f);
	gridPassData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	gridPassData.m_thinLineColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
	gridPassData.m_thickLineColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
	gridPassData.m_cameraPos = glm::make_vec3(cameraPosition);
	gridPassData.m_cellSize = 1.0f;
	gridPassData.m_gridSize = 1000.0f;

	m_gridPass->record(graph, gridPassData);

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
