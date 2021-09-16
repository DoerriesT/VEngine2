#pragma once
#include "graphics/gal/FwdDecl.h"
#include <stdint.h>

class BufferStackAllocator;
struct ImDrawData;

class ImGuiPass
{
public:
	struct Data
	{
		void *m_profilingCtx;
		BufferStackAllocator *m_vertexBufferAllocator;
		BufferStackAllocator *m_indexBufferAllocator;
		gal::DescriptorSet *m_bindlessSet;
		uint32_t m_width;
		uint32_t m_height;
		gal::ImageView *m_colorAttachment;
		bool m_clear;
		ImDrawData *m_imGuiDrawData;
	};

	explicit ImGuiPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *bindlessSetLayout);
	~ImGuiPass();
	void record(gal::CommandList *cmdList, const Data &data);

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_pipeline = nullptr;
};