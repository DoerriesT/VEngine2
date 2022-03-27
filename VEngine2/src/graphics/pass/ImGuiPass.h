#pragma once
#include "graphics/gal/FwdDecl.h"
#include <stdint.h>
#include "../RenderGraph.h"

class LinearGPUBufferAllocator;
struct ImDrawData;

class ImGuiPass
{
public:
	struct Data
	{
		void *m_profilingCtx;
		LinearGPUBufferAllocator *m_vertexBufferAllocator;
		LinearGPUBufferAllocator *m_indexBufferAllocator;
		gal::DescriptorSet *m_bindlessSet;
		uint32_t m_width;
		uint32_t m_height;
		rg::ResourceViewHandle m_renderTargetHandle;
		bool m_clear;
		ImDrawData *m_imGuiDrawData;
		size_t m_managedReadResourceCount; // graph managed resources that need to be in READ_RESOURCE / PIXEL_SHADER state for imgui
		rg::ResourceViewHandle *m_managedReadResources; // graph managed resources that need to be in READ_RESOURCE / PIXEL_SHADER state for imgui
	};

	explicit ImGuiPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *bindlessSetLayout);
	~ImGuiPass();
	void record(rg::RenderGraph *graph, const Data &data);

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_pipeline = nullptr;
};