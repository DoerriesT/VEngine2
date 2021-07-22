#pragma once
#include "Handles.h"
#include "Material.h"
#include "utility/SpinLock.h"
#include "utility/HandleManager.h"
#include <EASTL/vector.h>

class TextureManager;

class MaterialManager
{
public:
	explicit MaterialManager(TextureManager *textureManager) noexcept;
	void createMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept;
	void updateMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept;
	void destroyMaterials(uint32_t count, MaterialHandle *handles) noexcept;
	MaterialGPU getMaterial(MaterialHandle handle) const noexcept;

private:
	mutable SpinLock m_handleManagerMutex;
	mutable SpinLock m_materialsMutex;
	HandleManager m_handleManager;
	eastl::vector<MaterialGPU> m_materials;
	TextureManager *m_textureManager;
};