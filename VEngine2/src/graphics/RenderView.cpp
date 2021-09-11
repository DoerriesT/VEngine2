#include "RenderView.h"
#include "pass/MeshPass.h"
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
#include "animation/Skeleton.h"

RenderView::RenderView(ECS *ecs, gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept
	:m_ecs(ecs),
	m_device(device),
	m_viewRegistry(viewRegistry),
	m_meshManager(meshManager),
	m_width(width),
	m_height(height)
{
	m_renderViewResources = new RenderViewResources(m_device, viewRegistry, m_width, m_height);
	m_meshPass = new MeshPass(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_gridPass = new GridPass(m_device, offsetBufferSetLayout);
}

RenderView::~RenderView()
{
	delete m_meshPass;
	delete m_gridPass;
	delete m_renderViewResources;
}

void RenderView::render(gal::CommandList *cmdList, BufferStackAllocator *bufferAllocator, gal::DescriptorSet *offsetBufferSet, const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition, bool transitionResultToTexture) noexcept
{
	if (m_width == 0 || m_height == 0)
	{
		return;
	}

	{
		gal::Barrier barriers[] =
		{
			gal::Initializers::imageBarrier(
				m_renderViewResources->m_resultImage,
				m_renderViewResources->m_resultImagePipelineStages,
				gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
				m_renderViewResources->m_resultImageResourceState,
				gal::ResourceState::WRITE_COLOR_ATTACHMENT),
			gal::Initializers::imageBarrier(
				m_renderViewResources->m_depthBufferImage,
				m_renderViewResources->m_depthBufferImagePipelineStages,
				gal::PipelineStageFlags::EARLY_FRAGMENT_TESTS_BIT | gal::PipelineStageFlags::LATE_FRAGMENT_TESTS_BIT,
				m_renderViewResources->m_depthBufferImageResourceState,
				gal::ResourceState::READ_WRITE_DEPTH_STENCIL)
		};

		cmdList->barrier(2, barriers);

		m_renderViewResources->m_resultImagePipelineStages = gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT;
		m_renderViewResources->m_resultImageResourceState = gal::ResourceState::WRITE_COLOR_ATTACHMENT;
		m_renderViewResources->m_depthBufferImagePipelineStages = gal::PipelineStageFlags::EARLY_FRAGMENT_TESTS_BIT | gal::PipelineStageFlags::LATE_FRAGMENT_TESTS_BIT;
		m_renderViewResources->m_depthBufferImageResourceState = gal::ResourceState::READ_WRITE_DEPTH_STENCIL;
	}

	eastl::vector<glm::mat4> modelMatrices;
	eastl::vector<SubMeshDrawInfo> meshDrawInfo;
	eastl::vector<SubMeshBufferHandles> meshBufferHandles;
	eastl::vector<uint32_t> skinningMatrixOffsets;


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
				for (auto h : submeshhandles)
				{
					modelMatrices.push_back(modelMatrix);
					meshDrawInfo.push_back(m_meshManager->getSubMeshDrawInfo(h));
					meshBufferHandles.push_back(m_meshManager->getSubMeshBufferHandles(h));
				}
				
			}
		});

	uint32_t meshCount = static_cast<uint32_t>(modelMatrices.size());

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
				for (auto h : submeshhandles)
				{
					modelMatrices.push_back(modelMatrix);
					meshDrawInfo.push_back(m_meshManager->getSubMeshDrawInfo(h));
					meshBufferHandles.push_back(m_meshManager->getSubMeshBufferHandles(h));
					skinningMatrixOffsets.push_back(curSkinningMatrixOffset);
				}

				assert(curSkinningMatrixOffset + smc.m_matrixPalette.size() <= 1024);

				memcpy(skinningMatricesBufferPtr + curSkinningMatrixOffset, smc.m_matrixPalette.data(), smc.m_matrixPalette.size() * sizeof(glm::mat4));
				curSkinningMatrixOffset += curSkinningMatrixOffset;
			}
		});

	m_renderViewResources->m_skinningMatricesBuffers[m_frame & 1]->unmap();

	uint32_t skinnedMeshCount = static_cast<uint32_t>(modelMatrices.size()) - meshCount;

	MeshPass::Data meshPassData{};
	meshPassData.m_bufferAllocator = bufferAllocator;
	meshPassData.m_offsetBufferSet = offsetBufferSet;
	meshPassData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	meshPassData.m_width = m_width;
	meshPassData.m_height = m_height;
	meshPassData.m_meshCount = meshCount;
	meshPassData.m_skinnedMeshCount = skinnedMeshCount;
	meshPassData.m_colorAttachment = m_renderViewResources->m_resultImageView;
	meshPassData.m_depthBufferAttachment = m_renderViewResources->m_depthBufferImageView;
	meshPassData.m_skinningMatrixBufferIndex = m_renderViewResources->m_skinningMatricesBufferViewHandles[m_frame & 1];
	meshPassData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	meshPassData.m_modelMatrices = modelMatrices.data();
	meshPassData.m_meshDrawInfo = meshDrawInfo.data();
	meshPassData.m_meshBufferHandles = meshBufferHandles.data();
	meshPassData.m_skinningMatrixOffsets = skinningMatrixOffsets.data();

	m_meshPass->record(cmdList, meshPassData);
	

	GridPass::Data gridPassData{};
	gridPassData.m_bufferAllocator = bufferAllocator;
	gridPassData.m_offsetBufferSet = offsetBufferSet;
	gridPassData.m_width = m_width;
	gridPassData.m_height = m_height;
	gridPassData.m_colorAttachment = m_renderViewResources->m_resultImageView;
	gridPassData.m_depthBufferAttachment = m_renderViewResources->m_depthBufferImageView;
	gridPassData.m_modelMatrix = glm::mat4(1.0f);
	gridPassData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	gridPassData.m_thinLineColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
	gridPassData.m_thickLineColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
	gridPassData.m_cameraPos = glm::make_vec3(cameraPosition);
	gridPassData.m_cellSize = 1.0f;
	gridPassData.m_gridSize = 1000.0f;

	m_gridPass->record(cmdList, gridPassData);

	{
		gal::Barrier b1 = gal::Initializers::imageBarrier(
			m_renderViewResources->m_resultImage,
			m_renderViewResources->m_resultImagePipelineStages,
			transitionResultToTexture ? gal::PipelineStageFlags::PIXEL_SHADER_BIT : gal::PipelineStageFlags::TRANSFER_BIT,
			m_renderViewResources->m_resultImageResourceState,
			transitionResultToTexture ? gal::ResourceState::READ_RESOURCE : gal::ResourceState::READ_TRANSFER);

		cmdList->barrier(1, &b1);

		m_renderViewResources->m_resultImagePipelineStages = transitionResultToTexture ? gal::PipelineStageFlags::PIXEL_SHADER_BIT : gal::PipelineStageFlags::TRANSFER_BIT;
		m_renderViewResources->m_resultImageResourceState = transitionResultToTexture ? gal::ResourceState::READ_RESOURCE : gal::ResourceState::READ_TRANSFER;
	}

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
