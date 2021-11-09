#include "PostProcessModule.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "graphics/Mesh.h"
#include <EASTL/iterator.h> // eastl::size()
#include "utility/Utility.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"

using namespace gal;

namespace
{
	struct PushConsts
	{
		uint32_t resolution[2];
		float texelSize[2];
		float time;
		uint32_t inputImageIndex;
		uint32_t outputImageIndex;
	};
}

PostProcessModule::PostProcessModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	DescriptorSetLayoutBinding usedBindlessBindings[] =
	{
		{ DescriptorType::TEXTURE, 0, 0, 65536, ShaderStageFlags::PIXEL_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
		{ DescriptorType::RW_TEXTURE, 65536, 0, 65536, ShaderStageFlags::VERTEX_BIT, DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | DescriptorBindingFlags::PARTIALLY_BOUND_BIT },
	};

	DescriptorSetLayoutDeclaration layoutDecls[]
	{
		{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
	};

	ComputePipelineCreateInfo pipelineCreateInfo{};
	ComputePipelineBuilder builder(pipelineCreateInfo);
	builder.setComputeShader("assets/shaders/tonemap_cs");
	builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(PushConsts), ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

	device->createComputePipelines(1, &pipelineCreateInfo, &m_tonemapPipeline);
}

PostProcessModule::~PostProcessModule() noexcept
{
	m_device->destroyComputePipeline(m_tonemapPipeline);
}

void PostProcessModule::record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept
{
	rg::ResourceUsageDesc usageDescs[] =
	{
		{data.m_lightingImageView, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{data.m_resultImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("Post-Process", rg::QueueType::GRAPHICS, eastl::size(usageDescs), usageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Post-Process");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Post-Process");
			PROFILING_ZONE_SCOPED;

			cmdList->bindPipeline(m_tonemapPipeline);

			cmdList->bindDescriptorSets(m_tonemapPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.resolution[0] = data.m_width;
			pushConsts.resolution[1] = data.m_height;
			pushConsts.texelSize[0] = 1.0f / data.m_width;
			pushConsts.texelSize[1] = 1.0f / data.m_height;
			pushConsts.time = 0.0f; // TODO
			pushConsts.inputImageIndex = registry.getBindlessHandle(data.m_lightingImageView, DescriptorType::TEXTURE);
			pushConsts.outputImageIndex = registry.getBindlessHandle(data.m_resultImageViewHandle, DescriptorType::RW_TEXTURE);

			cmdList->pushConstants(m_tonemapPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((data.m_width + 7) / 8, (data.m_height + 7) / 8, 1);
		});
}
