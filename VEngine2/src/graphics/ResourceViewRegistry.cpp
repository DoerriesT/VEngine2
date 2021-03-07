#include "ResourceViewRegistry.h"

static constexpr uint32_t s_textureBinding = 0;
static constexpr uint32_t s_rwTextureBinding = 65536;
static constexpr uint32_t s_typedBufferBinding = 131072;
static constexpr uint32_t s_rwTypedBufferBinding = 196608;
static constexpr uint32_t s_byteBufferBinding = 262144;
static constexpr uint32_t s_rwByteBufferBinding = 327680;

ResourceViewRegistry::ResourceViewRegistry(gal::GraphicsDevice *device) noexcept
	:m_device(device),
	m_textureHandleManager(65536),
	m_rwTextureHandleManager(65536),
	m_typedBufferHandleManager(65536),
	m_rwTypedBufferHandleManager(65536),
	m_byteBufferHandleManager(65536),
	m_rwByteBufferHandleManager(65536)
{
	auto stages = gal::ShaderStageFlags::ALL_STAGES;
	auto bindingFlags = gal::DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | gal::DescriptorBindingFlags::PARTIALLY_BOUND_BIT;

	gal::DescriptorSetLayoutBinding bindings[]
	{
		{gal::DescriptorType::TEXTURE, s_textureBinding, 0, 65536, stages, bindingFlags },
		{gal::DescriptorType::RW_TEXTURE, s_rwTextureBinding, 0, 65536, stages, bindingFlags },
		{gal::DescriptorType::TYPED_BUFFER, s_typedBufferBinding, 0, 65536, stages, bindingFlags },
		{gal::DescriptorType::RW_TYPED_BUFFER, s_rwTypedBufferBinding, 0, 65536, stages, bindingFlags },
		{gal::DescriptorType::BYTE_BUFFER, s_byteBufferBinding, 0, 65536, stages, bindingFlags },
		{gal::DescriptorType::RW_BYTE_BUFFER, s_rwByteBufferBinding, 0, 65536, stages, bindingFlags },
	};
	m_device->createDescriptorSetLayout((uint32_t)std::size(bindings), bindings, &m_descriptorSetLayout);
	m_device->createDescriptorSetPool(2, m_descriptorSetLayout, &m_descriptorSetPool);
	m_descriptorSetPool->allocateDescriptorSets(2, m_descriptorSets);
}

ResourceViewRegistry::~ResourceViewRegistry()
{
	m_device->destroyDescriptorSetPool(m_descriptorSetPool);
	m_device->destroyDescriptorSetLayout(m_descriptorSetLayout);
}

TextureViewHandle ResourceViewRegistry::createTextureViewHandle(gal::ImageView *imageView, bool transient) noexcept
{
	return (TextureViewHandle)createHandle(m_textureHandleManager, m_textureHandleManagerMutex, s_textureBinding, transient, imageView, nullptr, nullptr);
}

RWTextureViewHandle ResourceViewRegistry::createRWTextureViewHandle(gal::ImageView *imageView, bool transient) noexcept
{
	return (RWTextureViewHandle)createHandle(m_rwTextureHandleManager, m_rwTextureHandleManagerMutex, s_rwTextureBinding, transient, imageView, nullptr, nullptr);
}

TypedBufferViewHandle ResourceViewRegistry::createTypedBufferViewHandle(gal::BufferView *bufferView, bool transient) noexcept
{
	return (TypedBufferViewHandle)createHandle(m_typedBufferHandleManager, m_typedBufferHandleManagerMutex, s_typedBufferBinding, transient, nullptr, bufferView, nullptr);
}

RWTypedBufferViewHandle ResourceViewRegistry::createRWTypedBufferViewHandle(gal::BufferView *bufferView, bool transient) noexcept
{
	return (RWTypedBufferViewHandle)createHandle(m_rwTypedBufferHandleManager, m_rwTypedBufferHandleManagerMutex, s_rwTypedBufferBinding, transient, nullptr, bufferView, nullptr);
}

ByteBufferViewHandle ResourceViewRegistry::createByteBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient) noexcept
{
	return (ByteBufferViewHandle)createHandle(m_byteBufferHandleManager, m_byteBufferHandleManagerMutex, s_byteBufferBinding, transient, nullptr, nullptr, &bufferInfo);
}

RWByteBufferViewHandle ResourceViewRegistry::createRWByteBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient) noexcept
{
	return (RWByteBufferViewHandle)createHandle(m_rwByteBufferHandleManager, m_rwByteBufferHandleManagerMutex, s_rwByteBufferBinding, transient, nullptr, nullptr, &bufferInfo);
}

StructuredBufferViewHandle ResourceViewRegistry::createStructuredBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient) noexcept
{
	return (StructuredBufferViewHandle)createByteBufferViewHandle(bufferInfo, transient);
}

RWStructuredBufferViewHandle ResourceViewRegistry::createRWStructuredBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient) noexcept
{
	return (RWStructuredBufferViewHandle)createRWByteBufferViewHandle(bufferInfo, transient);
}

void ResourceViewRegistry::updateHandle(TextureViewHandle handle, gal::ImageView *imageView) noexcept
{
	updateHandle(handle, s_textureBinding, imageView, nullptr, nullptr);
}

void ResourceViewRegistry::updateHandle(RWTextureViewHandle handle, gal::ImageView *imageView) noexcept
{
	updateHandle(handle, s_rwTextureBinding, imageView, nullptr, nullptr);
}

void ResourceViewRegistry::updateHandle(TypedBufferViewHandle handle, gal::BufferView *bufferView) noexcept
{
	updateHandle(handle, s_typedBufferBinding, nullptr, bufferView, nullptr);
}

void ResourceViewRegistry::updateHandle(RWTypedBufferViewHandle handle, gal::BufferView *bufferView) noexcept
{
	updateHandle(handle, s_rwTypedBufferBinding, nullptr, bufferView, nullptr);
}

void ResourceViewRegistry::updateHandle(ByteBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept
{
	updateHandle(handle, s_byteBufferBinding, nullptr, nullptr, &bufferInfo);
}

void ResourceViewRegistry::updateHandle(RWByteBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept
{
	updateHandle(handle, s_rwByteBufferBinding, nullptr, nullptr, &bufferInfo);
}

void ResourceViewRegistry::updateHandle(StructuredBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept
{
	updateHandle((ByteBufferViewHandle)handle, bufferInfo);
}

void ResourceViewRegistry::updateHandle(RWStructuredBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept
{
	updateHandle((RWByteBufferViewHandle)handle, bufferInfo);
}

void ResourceViewRegistry::destroyHandle(TextureViewHandle handle) noexcept
{
	destroyHandle(m_textureHandleManager, handle, s_textureBinding);
}

void ResourceViewRegistry::destroyHandle(RWTextureViewHandle handle) noexcept
{
	destroyHandle(m_rwTextureHandleManager, handle, s_rwTextureBinding);
}

void ResourceViewRegistry::destroyHandle(TypedBufferViewHandle handle) noexcept
{
	destroyHandle(m_typedBufferHandleManager, handle, s_typedBufferBinding);
}

void ResourceViewRegistry::destroyHandle(RWTypedBufferViewHandle handle) noexcept
{
	destroyHandle(m_rwTypedBufferHandleManager, handle, s_rwTypedBufferBinding);
}

void ResourceViewRegistry::destroyHandle(ByteBufferViewHandle handle) noexcept
{
	destroyHandle(m_byteBufferHandleManager, handle, s_byteBufferBinding);
}

void ResourceViewRegistry::destroyHandle(RWByteBufferViewHandle handle) noexcept
{
	destroyHandle(m_rwByteBufferHandleManager, handle, s_rwByteBufferBinding);
}

void ResourceViewRegistry::destroyHandle(StructuredBufferViewHandle handle) noexcept
{
	destroyHandle((ByteBufferViewHandle)handle);
}

void ResourceViewRegistry::destroyHandle(RWStructuredBufferViewHandle handle) noexcept
{
	destroyHandle((RWByteBufferViewHandle)handle);
}

void ResourceViewRegistry::flushChanges() noexcept
{
	const size_t resIdx = m_frame % 2;

	SpinLockHolder holder(m_pendingUpdatesMutex[resIdx]);

	m_descriptorSets[resIdx]->update((uint32_t)m_pendingUpdates[resIdx].size(), m_pendingUpdates[resIdx].data());
	m_pendingUpdates[resIdx].clear();
}

void ResourceViewRegistry::swapSets() noexcept
{
	m_textureHandleManager.freeTransientHandles();
	m_rwTextureHandleManager.freeTransientHandles();
	m_typedBufferHandleManager.freeTransientHandles();
	m_rwTypedBufferHandleManager.freeTransientHandles();
	m_rwByteBufferHandleManager.freeTransientHandles();
	m_byteBufferHandleManager.freeTransientHandles();
	++m_frame;
}

gal::DescriptorSetLayout *ResourceViewRegistry::getDescriptorSetLayout() const noexcept
{
	return m_descriptorSetLayout;
}

gal::DescriptorSet *ResourceViewRegistry::getCurrentFrameDescriptorSet() const noexcept
{
	return m_descriptorSets[m_frame % 2];
}

void ResourceViewRegistry::addUpdate(const gal::DescriptorSetUpdate &update, bool transient)
{
	for (size_t frame = 0; frame < 2; ++frame)
	{
		if (transient && (frame != (m_frame % 2)))
		{
			continue;
		}

		SpinLockHolder holder(m_pendingUpdatesMutex[frame]);

		bool replacedExisting = false;

		for (auto &u : m_pendingUpdates[frame])
		{
			if (u.m_dstBinding == update.m_dstBinding && u.m_dstArrayElement == update.m_dstArrayElement)
			{
				u = update;
				replacedExisting = true;
			}
		}

		if (!replacedExisting)
		{
			m_pendingUpdates[frame].push_back(update);
		}
	}
}

uint32_t ResourceViewRegistry::createHandle(HandleManager &manager, SpinLock &managerMutex, uint32_t binding, bool transient, gal::ImageView *imageView, gal::BufferView *bufferView, const gal::DescriptorBufferInfo *bufferInfo)
{
	uint32_t handle = 0;

	{
		SpinLockHolder holder(managerMutex);
		m_textureHandleManager.allocate(transient);
	}

	// allocation failed
	if (!handle)
	{
		return handle;
	}

	gal::DescriptorSetUpdate update{};
	update.m_dstBinding = binding;
	update.m_dstArrayElement = (uint32_t)handle;
	update.m_descriptorCount = 1;
	update.m_imageView = imageView;
	update.m_bufferView = bufferView;
	if (bufferInfo)
	{
		update.m_bufferInfo1 = *bufferInfo;
	}

	addUpdate(update, transient);

	return handle;
}

void ResourceViewRegistry::updateHandle(uint32_t handle, uint32_t binding, gal::ImageView *imageView, gal::BufferView *bufferView, const gal::DescriptorBufferInfo *bufferInfo)
{
	gal::DescriptorSetUpdate update{};
	update.m_dstBinding = binding;
	update.m_dstArrayElement = (uint32_t)handle;
	update.m_descriptorCount = 1;
	update.m_imageView = imageView;
	update.m_bufferView = bufferView;
	if (bufferInfo)
	{
		update.m_bufferInfo1 = *bufferInfo;
	}

	addUpdate(update, false);
}

void ResourceViewRegistry::destroyHandle(HandleManager &manager, uint32_t handle, uint32_t binding)
{
	for (size_t frame = 0; frame < 2; ++frame)
	{
		SpinLockHolder holder(m_pendingUpdatesMutex[frame]);

		auto &updates = m_pendingUpdates[frame];
		updates.erase(eastl::remove_if(updates.begin(), updates.end(), [&](const auto &u)
			{
				return u.m_dstBinding == binding && u.m_dstArrayElement == handle;
			}), updates.end());
	}

	manager.free(handle);
}