#pragma once
#include "Handles.h"
#include "Material.h"
#include "utility/SpinLock.h"
#include "utility/HandleManager.h"
#include <EASTL/vector.h>
#include "gal/FwdDecl.h"

class TextureManager;

class MaterialManager
{
public:
	explicit MaterialManager(gal::GraphicsDevice *device, TextureManager *textureManager, gal::Buffer *materialBuffer) noexcept;
	void createMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept;
	void updateMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept;
	void destroyMaterials(uint32_t count, MaterialHandle *handles) noexcept;
	MaterialGPU getMaterial(MaterialHandle handle) const noexcept;
	void flushUploadCopies(gal::CommandList *cmdList, uint64_t frameIndex) noexcept;

private:
	gal::GraphicsDevice *m_device;
	mutable SpinLock m_handleManagerMutex;
	mutable SpinLock m_materialsMutex;
	HandleManager m_handleManager;
	eastl::vector<MaterialGPU> m_materials;
	TextureManager *m_textureManager;
	gal::Buffer *m_materialBuffer;
};