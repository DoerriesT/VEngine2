#include "RenderView.h"
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
#include <glm/gtx/transform.hpp>
#include "MeshManager.h"
#include "MaterialManager.h"
#include "animation/Skeleton.h"
#include "RenderGraph.h"
#include "RendererResources.h"

using namespace gal;

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
	m_forwardModule = new ForwardModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_postProcessModule = new PostProcessModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_gridPass = new GridPass(m_device, offsetBufferSetLayout);
}

RenderView::~RenderView()
{
	delete m_forwardModule;
	delete m_postProcessModule;
	delete m_gridPass;
	delete m_renderViewResources;
}

void RenderView::render(rg::RenderGraph *graph, BufferStackAllocator *bufferAllocator, gal::DescriptorSet *offsetBufferSet, const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition, bool transitionResultToTexture) noexcept
{
	if (m_width == 0 || m_height == 0)
	{
		return;
	}

	// result image
	rg::ResourceHandle resultImageHandle = graph->importImage(m_renderViewResources->m_resultImage, "Render View Result", m_renderViewResources->m_resultImageState);
	rg::ResourceViewHandle resultImageViewHandle = m_resultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Render View Result", resultImageHandle, graph));

	m_modelMatrices.clear();
	m_renderList.clear();

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
	m_renderViewResources->m_skinningMatricesBuffers[m_frame & 1]->map((void **)&skinningMatricesBufferPtr);

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

	m_renderViewResources->m_skinningMatricesBuffers[m_frame & 1]->unmap();

	ForwardModule::Data forwardModuleData{};
	forwardModuleData.m_profilingCtx = m_device->getProfilingContext();
	forwardModuleData.m_bufferAllocator = bufferAllocator;
	forwardModuleData.m_offsetBufferSet = offsetBufferSet;
	forwardModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	forwardModuleData.m_width = m_width;
	forwardModuleData.m_height = m_height;
	forwardModuleData.m_skinningMatrixBufferHandle = m_renderViewResources->m_skinningMatricesBufferViewHandles[m_frame & 1];
	forwardModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	forwardModuleData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	forwardModuleData.m_cameraPosition = glm::make_vec3(cameraPosition);
	forwardModuleData.m_renderList = &m_renderList;
	forwardModuleData.m_modelMatrices = m_modelMatrices.data();
	forwardModuleData.m_meshDrawInfo = subMeshDrawInfoTable;
	forwardModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();

	ForwardModule::ResultData forwardModuleResultData;

	m_forwardModule->record(graph, forwardModuleData, &forwardModuleResultData);

	PostProcessModule::Data postProcessModuleData{};
	postProcessModuleData.m_profilingCtx = m_device->getProfilingContext();
	postProcessModuleData.m_bufferAllocator = bufferAllocator;
	postProcessModuleData.m_offsetBufferSet = offsetBufferSet;
	postProcessModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	postProcessModuleData.m_width = m_width;
	postProcessModuleData.m_height = m_height;
	postProcessModuleData.m_lightingImageView = forwardModuleResultData.m_lightingImageViewHandle;
	postProcessModuleData.m_depthBufferImageViewHandle = forwardModuleResultData.m_depthBufferImageViewHandle;
	postProcessModuleData.m_resultImageViewHandle = resultImageViewHandle;
	postProcessModuleData.m_skinningMatrixBufferHandle = m_renderViewResources->m_skinningMatricesBufferViewHandles[m_frame & 1];
	postProcessModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	postProcessModuleData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	postProcessModuleData.m_cameraPosition = glm::make_vec3(cameraPosition);
	postProcessModuleData.m_renderList = &m_renderList;
	postProcessModuleData.m_modelMatrices = m_modelMatrices.data();
	postProcessModuleData.m_meshDrawInfo = subMeshDrawInfoTable;
	postProcessModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	postProcessModuleData.m_debugNormals = true;

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
