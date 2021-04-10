#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include "ViewHandles.h"

class BufferStackAllocator;
class ResourceViewRegistry;

class RendererResources
{
public:
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_resourceViewRegistry = nullptr;

	gal::CommandListPool *m_commandListPool = nullptr;
	gal::CommandList *m_commandList = nullptr;

	gal::Buffer *m_mappableConstantBuffers[2] = {};
	gal::Buffer *m_mappableIndexBuffers[2] = {};
	gal::Buffer *m_mappableVertexBuffers[2] = {};
	BufferStackAllocator *m_constantBufferStackAllocators[2] = {};
	BufferStackAllocator *m_indexBufferStackAllocators[2] = {};
	BufferStackAllocator *m_vertexBufferStackAllocators[2] = {};
	gal::Image *m_imguiFontTexture = nullptr;
	gal::ImageView *m_imguiFontTextureView = nullptr;
	TextureViewHandle m_imguiFontTextureViewHandle = {};

	gal::DescriptorSetLayout *m_offsetBufferDescriptorSetLayout = nullptr;
	gal::DescriptorSetPool *m_offsetBufferDescriptorSetPool = nullptr;
	gal::DescriptorSet *m_offsetBufferDescriptorSets[2] = {};

	explicit RendererResources(gal::GraphicsDevice *device, ResourceViewRegistry *resourceViewRegistry) noexcept;
	~RendererResources();
};