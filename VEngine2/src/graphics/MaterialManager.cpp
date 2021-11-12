#include "MaterialManager.h"
#include "TextureManager.h"
#include "Log.h"
#include "gal/GraphicsAbstractionLayer.h"

static MaterialGPU createGPUMaterial(const MaterialCreateInfo &createInfo, TextureManager *textureManager) noexcept
{
	MaterialGPU matGpu{};
	matGpu.m_emissive[0] = createInfo.m_emissiveFactor[0];
	matGpu.m_emissive[1] = createInfo.m_emissiveFactor[1];
	matGpu.m_emissive[2] = createInfo.m_emissiveFactor[2];
	matGpu.m_alphaMode = createInfo.m_alpha;
	matGpu.m_albedo = createInfo.m_albedoFactor;
	matGpu.m_metalness = createInfo.m_metallicFactor;
	matGpu.m_roughness = createInfo.m_roughnessFactor;
	matGpu.m_albedoTextureHandle = textureManager->getViewHandle(createInfo.m_albedoTexture);
	matGpu.m_normalTextureHandle = textureManager->getViewHandle(createInfo.m_normalTexture);
	matGpu.m_metalnessTextureHandle = textureManager->getViewHandle(createInfo.m_metallicTexture);
	matGpu.m_roughnessTextureHandle = textureManager->getViewHandle(createInfo.m_roughnessTexture);
	matGpu.m_occlusionTextureHandle = textureManager->getViewHandle(createInfo.m_occlusionTexture);
	matGpu.m_emissiveTextureHandle = textureManager->getViewHandle(createInfo.m_emissiveTexture);
	matGpu.m_displacementTextureHandle = textureManager->getViewHandle(createInfo.m_displacementTexture);

	return matGpu;
}

MaterialManager::MaterialManager(gal::GraphicsDevice *device, TextureManager *textureManager, gal::Buffer *materialBuffer) noexcept
	:m_device(device),
	m_textureManager(textureManager),
	m_materialBuffer(materialBuffer)
{
	m_materials.resize(16);
}

void MaterialManager::createMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept
{
	LOCK_HOLDER(m_materialsMutex);
	MaterialGPU *gpuMaterials = nullptr;
	m_materialBuffer->map((void **)&gpuMaterials);
	for (size_t i = 0; i < count; ++i)
	{
		// allocate handle
		{
			LOCK_HOLDER(m_handleManagerMutex);
			handles[i] = (MaterialHandle)m_handleManager.allocate();
		}

		if (!handles[i])
		{
			Log::err("MaterialManager: Failed to allocate MaterialHandle!");
			continue;
		}

		// store material
		{
			if (handles[i] >= m_materials.size())
			{
				m_materials.resize((size_t)(m_materials.size() * 1.5));
			}

			m_materials[handles[i]] = createGPUMaterial(materials[i], m_textureManager);
			memcpy(gpuMaterials + handles[i], &m_materials[handles[i]], sizeof(MaterialGPU));
		}
	}
	m_materialBuffer->unmap();
}

void MaterialManager::updateMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept
{
	LOCK_HOLDER(m_materialsMutex);
	for (size_t i = 0; i < count; ++i)
	{
		const bool validHandle = handles[i] != 0 && handles[i] < m_materials.size();
		if (!validHandle)
		{
			Log::warn("MaterialManager: Tried to update an invalid MaterialHandle!");
			return;
		}

		m_materials[handles[i]] = createGPUMaterial(materials[i], m_textureManager);
	}
}

void MaterialManager::destroyMaterials(uint32_t count, MaterialHandle *handles) noexcept
{
	LOCK_HOLDER(m_materialsMutex);
	for (size_t i = 0; i < count; ++i)
	{
		const bool validHandle = handles[i] != 0 && handles[i] < m_materials.size();

		if (!validHandle)
		{
			continue;
		}

		m_materials[handles[i]] = {};

		{
			LOCK_HOLDER(m_handleManagerMutex);
			m_handleManager.free(handles[i]);
		}
	}
}

MaterialGPU MaterialManager::getMaterial(MaterialHandle handle) const noexcept
{
	LOCK_HOLDER(m_materialsMutex);
	const bool validHandle = handle != 0 && handle < m_materials.size();
	if (!validHandle)
	{
		return MaterialGPU();
	}
	return m_materials[handle];
}

void MaterialManager::flushUploadCopies(gal::CommandList *cmdList, uint64_t frameIndex) noexcept
{
}
