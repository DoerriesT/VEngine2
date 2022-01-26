#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include "ViewHandles.h"

class LinearGPUBufferAllocator;
class ResourceViewRegistry;
class TextureLoader;

struct ProxyMeshInfo
{
	uint32_t m_indexCount;
	uint32_t m_firstIndex;
	uint32_t m_vertexOffset;
};

class RendererResources
{
public:
	static constexpr uint32_t k_brdfLUTResolution = 128;

	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_resourceViewRegistry = nullptr;

	gal::CommandListPool *m_commandListPool = nullptr;
	gal::CommandList *m_commandList = nullptr;

	gal::Buffer *m_mappableConstantBuffers[2] = {};
	gal::Buffer *m_mappableShaderResourceBuffers[2] = {};
	gal::Buffer *m_mappableIndexBuffers[2] = {};
	gal::Buffer *m_mappableVertexBuffers[2] = {};
	LinearGPUBufferAllocator *m_constantBufferLinearAllocators[2] = {};
	LinearGPUBufferAllocator *m_shaderResourceBufferLinearAllocators[2] = {};
	LinearGPUBufferAllocator *m_indexBufferLinearAllocators[2] = {};
	LinearGPUBufferAllocator *m_vertexBufferLinearAllocators[2] = {};
	gal::Image *m_imguiFontTexture = nullptr;
	gal::ImageView *m_imguiFontTextureView = nullptr;
	TextureViewHandle m_imguiFontTextureViewHandle = {};
	gal::Image *m_blueNoiseTexture = nullptr;
	gal::ImageView *m_blueNoiseTextureView = nullptr;
	TextureViewHandle m_blueNoiseTextureViewHandle = {};
	gal::Image *m_brdfLUT = nullptr;
	gal::ImageView *m_brdfLUTView = nullptr;
	TextureViewHandle m_brdfLUTTextureViewHandle = {};
	RWTextureViewHandle m_brdfLUTRWTextureViewHandle = {};

	gal::Buffer *m_proxyMeshVertexBuffer = {};
	gal::Buffer *m_proxyMeshIndexBuffer = {};

	// proxy mesh info
	ProxyMeshInfo m_icoSphereProxyMeshInfo;
	ProxyMeshInfo m_cone180ProxyMeshInfo;
	ProxyMeshInfo m_cone135ProxyMeshInfo;
	ProxyMeshInfo m_cone90ProxyMeshInfo;
	ProxyMeshInfo m_cone45ProxyMeshInfo;
	ProxyMeshInfo m_boxProxyMeshInfo;

	gal::Buffer *m_materialsBuffer = nullptr;
	StructuredBufferViewHandle m_materialsBufferViewHandle = {};

	gal::DescriptorSetLayout *m_offsetBufferDescriptorSetLayout = nullptr;
	gal::DescriptorSetPool *m_offsetBufferDescriptorSetPool = nullptr;
	gal::DescriptorSet *m_offsetBufferDescriptorSets[2] = {};

	explicit RendererResources(gal::GraphicsDevice *device, ResourceViewRegistry *resourceViewRegistry, TextureLoader *textureLoader) noexcept;
	~RendererResources();
};