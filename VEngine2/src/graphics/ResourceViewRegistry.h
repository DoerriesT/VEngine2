#pragma once
#include <stdint.h>
#include <EASTL/vector.h>
#include "ViewHandles.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "utility/HandleManager.h"
#include "utility/SpinLock.h"

class ResourceViewRegistry
{
public:
	static constexpr uint32_t k_textureBinding = 0;
	static constexpr uint32_t k_rwTextureBinding = 65536;
	static constexpr uint32_t k_typedBufferBinding = 131072;
	static constexpr uint32_t k_rwTypedBufferBinding = 196608;
	static constexpr uint32_t k_byteBufferBinding = 262144;
	static constexpr uint32_t k_rwByteBufferBinding = 327680;

	explicit ResourceViewRegistry(gal::GraphicsDevice *device) noexcept;
	ResourceViewRegistry(const ResourceViewRegistry &) = delete;
	ResourceViewRegistry(ResourceViewRegistry &&) = delete;
	ResourceViewRegistry &operator=(const ResourceViewRegistry &) = delete;
	ResourceViewRegistry &operator=(ResourceViewRegistry &&) = delete;
	~ResourceViewRegistry();
	TextureViewHandle createTextureViewHandle(gal::ImageView *imageView, bool transient = false) noexcept;
	RWTextureViewHandle createRWTextureViewHandle(gal::ImageView *imageView, bool transient = false) noexcept;
	TypedBufferViewHandle createTypedBufferViewHandle(gal::BufferView *bufferView, bool transient = false) noexcept;
	RWTypedBufferViewHandle createRWTypedBufferViewHandle(gal::BufferView *bufferView, bool transient = false) noexcept;
	ByteBufferViewHandle createByteBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient = false) noexcept;
	RWByteBufferViewHandle createRWByteBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient = false) noexcept;
	StructuredBufferViewHandle createStructuredBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient = false) noexcept;
	RWStructuredBufferViewHandle createRWStructuredBufferViewHandle(const gal::DescriptorBufferInfo &bufferInfo, bool transient = false) noexcept;
	void updateHandle(TextureViewHandle handle, gal::ImageView *imageView) noexcept;
	void updateHandle(RWTextureViewHandle handle, gal::ImageView *imageView) noexcept;
	void updateHandle(TypedBufferViewHandle handle, gal::BufferView *bufferView) noexcept;
	void updateHandle(RWTypedBufferViewHandle handle, gal::BufferView *bufferView) noexcept;
	void updateHandle(ByteBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept;
	void updateHandle(RWByteBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept;
	void updateHandle(StructuredBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept;
	void updateHandle(RWStructuredBufferViewHandle handle, const gal::DescriptorBufferInfo &bufferInfo) noexcept;
	void destroyHandle(TextureViewHandle handle) noexcept;
	void destroyHandle(RWTextureViewHandle handle) noexcept;
	void destroyHandle(TypedBufferViewHandle handle) noexcept;
	void destroyHandle(RWTypedBufferViewHandle handle) noexcept;
	void destroyHandle(ByteBufferViewHandle handle) noexcept;
	void destroyHandle(RWByteBufferViewHandle handle) noexcept;
	void destroyHandle(StructuredBufferViewHandle handle) noexcept;
	void destroyHandle(RWStructuredBufferViewHandle handle) noexcept;
	void flushChanges() noexcept;
	void swapSets() noexcept;
	gal::DescriptorSetLayout *getDescriptorSetLayout() const noexcept;
	gal::DescriptorSet *getCurrentFrameDescriptorSet() const noexcept;


private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::DescriptorSetPool *m_descriptorSetPool = nullptr;
	gal::DescriptorSetLayout *m_descriptorSetLayout = nullptr;
	gal::DescriptorSet *m_descriptorSets[2] = {};
	eastl::vector<gal::DescriptorSetUpdate> m_pendingUpdates[2];
	uint32_t m_frame = 0;
	HandleManager m_textureHandleManager;
	HandleManager m_rwTextureHandleManager;
	HandleManager m_typedBufferHandleManager;
	HandleManager m_rwTypedBufferHandleManager;
	HandleManager m_byteBufferHandleManager;
	HandleManager m_rwByteBufferHandleManager;
	SpinLock m_pendingUpdatesMutex[2];
	SpinLock m_textureHandleManagerMutex;
	SpinLock m_rwTextureHandleManagerMutex;
	SpinLock m_typedBufferHandleManagerMutex;
	SpinLock m_rwTypedBufferHandleManagerMutex;
	SpinLock m_byteBufferHandleManagerMutex;
	SpinLock m_rwByteBufferHandleManagerMutex;

	void addUpdate(const gal::DescriptorSetUpdate &update, bool transient);
	uint32_t createHandle(HandleManager &manager, SpinLock &managerMutex, uint32_t binding, bool transient, gal::DescriptorType descriptorType, gal::ImageView *imageView, gal::BufferView *bufferView, const gal::DescriptorBufferInfo *bufferInfo);
	void updateHandle(uint32_t handle, uint32_t binding, gal::DescriptorType descriptorType, gal::ImageView *imageView, gal::BufferView *bufferView, const gal::DescriptorBufferInfo *bufferInfo);
	void destroyHandle(HandleManager &manager, uint32_t handle, uint32_t binding);
};