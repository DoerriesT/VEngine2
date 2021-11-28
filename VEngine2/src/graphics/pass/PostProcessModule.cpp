#include "PostProcessModule.h"
#include "graphics/gal/GraphicsAbstractionLayer.h"
#include "graphics/gal/Initializers.h"
#include "graphics/BufferStackAllocator.h"
#include "graphics/Mesh.h"
#include <EASTL/iterator.h> // eastl::size()
#include <EASTL/fixed_vector.h>
#include "utility/Utility.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "graphics/RenderData.h"
#include <glm/packing.hpp>

using namespace gal;

namespace
{
	struct LuminanceHistogramPushConsts
	{
		uint32_t width;
		float scale;
		float bias;
		uint32_t inputTextureIndex;
		uint32_t exposureBufferIndex;
		uint32_t resultLuminanceBufferIndex;
	};

	struct AutoExposurePushConsts
	{
		float precomputedTermUp;
		float precomputedTermDown;
		float invScale;
		float bias;
		uint32_t lowerBound;
		uint32_t upperBound;
		float exposureCompensation;
		float exposureMin;
		float exposureMax;
		uint32_t fixExposureToMax;
		uint32_t inputHistogramBufferIndex;
		uint32_t resultExposureBufferIndex;
	};

	struct TonemapPushConsts
	{
		uint32_t resolution[2];
		float texelSize[2];
		float time;
		uint32_t inputImageIndex;
		uint32_t exposureBufferIndex;
		uint32_t outputImageIndex;
	};
}

PostProcessModule::PostProcessModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	// luminance histogram
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 1),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_BYTE_BUFFER, 0),
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/luminanceHistogram_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(LuminanceHistogramPushConsts), ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_luminanceHistogramPipeline);
	}

	// auto exposure
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 0),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_BYTE_BUFFER, 0),
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/autoExposure_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(AutoExposurePushConsts), ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_autoExposurePipeline);
	}

	// tonemap
	{
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::BYTE_BUFFER, 1),
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 0),
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		ComputePipelineCreateInfo pipelineCreateInfo{};
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("assets/shaders/tonemap_cs");
		builder.setPipelineLayoutDescription(1, layoutDecls, sizeof(TonemapPushConsts), ShaderStageFlags::COMPUTE_BIT, 0, nullptr, -1);

		device->createComputePipelines(1, &pipelineCreateInfo, &m_tonemapPipeline);
	}

	// outline ID
	for (size_t skinned = 0; skinned < 2; ++skinned)
	{
		for (size_t alphaTested = 0; alphaTested < 2; ++alphaTested)
		{
			const bool isSkinned = skinned != 0;
			const bool isAlphaTested = alphaTested != 0;

			uint32_t curVertexBufferIndex = 0;

			eastl::fixed_vector<VertexInputAttributeDescription, 6> attributeDescs;
			eastl::fixed_vector<VertexInputBindingDescription, 6> bindingDescs;

			attributeDescs.push_back({ "POSITION", curVertexBufferIndex, curVertexBufferIndex, Format::R32G32B32_SFLOAT, 0 });
			bindingDescs.push_back({ curVertexBufferIndex, sizeof(float) * 3, VertexInputRate::VERTEX });
			++curVertexBufferIndex;

			if (isAlphaTested)
			{
				attributeDescs.push_back({ "TEXCOORD", curVertexBufferIndex, curVertexBufferIndex, Format::R32G32_SFLOAT, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(float) * 2, VertexInputRate::VERTEX });
				++curVertexBufferIndex;
			}
			if (isSkinned)
			{
				attributeDescs.push_back({ "JOINT_INDICES", curVertexBufferIndex, curVertexBufferIndex, Format::R16G16B16A16_UINT, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(uint16_t) * 4, VertexInputRate::VERTEX });
				++curVertexBufferIndex;

				attributeDescs.push_back({ "JOINT_WEIGHTS", curVertexBufferIndex, curVertexBufferIndex, Format::R8G8B8A8_UNORM, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(uint32_t), VertexInputRate::VERTEX });
				++curVertexBufferIndex;
			}

			GraphicsPipelineCreateInfo pipelineCreateInfo{};
			GraphicsPipelineBuilder builder(pipelineCreateInfo);

			if (isAlphaTested)
			{
				builder.setVertexShader(isSkinned ? "assets/shaders/outlineID_skinned_alpha_tested_vs" : "assets/shaders/outlineID_alpha_tested_vs");
				builder.setFragmentShader("assets/shaders/outlineID_alpha_tested_ps");
			}
			else
			{
				builder.setVertexShader(isSkinned ? "assets/shaders/outlineID_skinned_vs" : "assets/shaders/outlineID_vs");
				builder.setFragmentShader("assets/shaders/outlineID_ps");
			}

			builder.setVertexBindingDescriptions(static_cast<uint32_t>(bindingDescs.size()), bindingDescs.data());
			builder.setVertexAttributeDescriptions(static_cast<uint32_t>(attributeDescs.size()), attributeDescs.data());
			builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
			builder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
			builder.setColorAttachmentFormat(Format::R8_UNORM);
			builder.setPolygonModeCullMode(gal::PolygonMode::FILL, isAlphaTested ? gal::CullModeFlags::NONE : gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::VERTEX_BIT };
			DescriptorSetLayoutBinding usedBindlessBindings[] =
			{
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1, ShaderStageFlags::VERTEX_BIT), // skinning matrices
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 2, ShaderStageFlags::VERTEX_BIT), // material buffer
			};

			DescriptorSetLayoutDeclaration layoutDecls[]
			{
				{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
				{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
			};

			gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticAnisotropicRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT);

			uint32_t pushConstSize = sizeof(float) * 16 + sizeof(uint32_t);
			pushConstSize += isSkinned ? sizeof(uint32_t) : 0;
			pushConstSize += isAlphaTested ? sizeof(uint32_t) : 0;
			builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT, 1, &staticSamplerDesc, 2);

			GraphicsPipeline *pipeline = nullptr;
			device->createGraphicsPipelines(1, &pipelineCreateInfo, &pipeline);
			if (isSkinned)
			{
				if (isAlphaTested)
				{
					m_outlineIDSkinnedAlphaTestedPipeline = pipeline;
				}
				else
				{
					m_outlineIDSkinnedPipeline = pipeline;
				}
			}
			else
			{
				if (isAlphaTested)
				{
					m_outlineIDAlphaTestedPipeline = pipeline;
				}
				else
				{
					m_outlineIDPipeline = pipeline;
				}
			}
		}
	}

	// outline blend
	{
		gal::PipelineColorBlendAttachmentState blendState{};
		blendState.m_blendEnable = true;
		blendState.m_srcColorBlendFactor = gal::BlendFactor::SRC_ALPHA;
		blendState.m_dstColorBlendFactor = gal::BlendFactor::ONE_MINUS_SRC_ALPHA;
		blendState.m_colorBlendOp = gal::BlendOp::ADD;
		blendState.m_srcAlphaBlendFactor = gal::BlendFactor::ZERO;
		blendState.m_dstAlphaBlendFactor = gal::BlendFactor::ONE;
		blendState.m_alphaBlendOp = gal::BlendOp::ADD;
		blendState.m_colorWriteMask = gal::ColorComponentFlags::ALL_BITS;

		gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
		gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/fullscreenTriangle_vs");
		builder.setFragmentShader("assets/shaders/outline_ps");
		builder.setColorBlendAttachment(blendState);
		builder.setDynamicState(gal::DynamicStateFlags::VIEWPORT_BIT | gal::DynamicStateFlags::SCISSOR_BIT);
		builder.setColorAttachmentFormat(gal::Format::B8G8R8A8_UNORM);

		DescriptorSetLayoutBinding usedBindlessBindings[]
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticLinearClampSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT);

		uint32_t pushConstSize = sizeof(float) * 6;
		builder.setPipelineLayoutDescription(1, layoutDecls, pushConstSize, ShaderStageFlags::PIXEL_BIT, 1, &staticSamplerDesc, 1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_outlinePipeline);
	}

	// debug normals
	for (size_t skinned = 0; skinned < 2; ++skinned)
	{
		VertexInputAttributeDescription attributeDescs[]
		{
			{ "POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 },
			{ "NORMAL", 1, 1, Format::R32G32B32_SFLOAT, 0 },
			{ "JOINT_INDICES", 2, 2, Format::R16G16B16A16_UINT, 0 },
			{ "JOINT_WEIGHTS", 3, 3, Format::R8G8B8A8_UNORM, 0 },
		};

		VertexInputBindingDescription bindingDescs[]
		{
			{ 0, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 1, sizeof(float) * 3, VertexInputRate::VERTEX },
			{ 2, sizeof(uint32_t) * 2, VertexInputRate::VERTEX },
			{ 3, sizeof(uint32_t), VertexInputRate::VERTEX },
		};

		bool isSkinned = skinned != 0;
		const size_t bindingDescCount = isSkinned ? eastl::size(bindingDescs) : eastl::size(bindingDescs) - 2;
		const size_t attributeDescCount = isSkinned ? eastl::size(attributeDescs) : eastl::size(attributeDescs) - 2;

		GraphicsPipelineCreateInfo pipelineCreateInfo{};
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader(isSkinned ? "assets/shaders/debugNormals_skinned_vs" : "assets/shaders/debugNormals_vs");
		builder.setGeometryShader("assets/shaders/debugNormals_gs");
		builder.setFragmentShader("assets/shaders/debugNormals_ps");
		builder.setVertexBindingDescriptions(bindingDescCount, bindingDescs);
		builder.setVertexAttributeDescriptions(attributeDescCount, attributeDescs);
		builder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
		builder.setDepthTest(true, false, CompareOp::GREATER_OR_EQUAL);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
		builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
		builder.setColorAttachmentFormat(Format::B8G8R8A8_UNORM);
		builder.setPolygonModeCullMode(gal::PolygonMode::FILL, gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 0, ShaderStageFlags::VERTEX_BIT), // skinning matrices
		};


		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		uint32_t pushConstSize = sizeof(float) * 16;
		pushConstSize += isSkinned ? sizeof(uint32_t) : 0;
		builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, isSkinned ? &m_debugNormalsSkinnedPipeline : &m_debugNormalsPipeline);
	}

	// debug draws
	for (size_t triangle = 0; triangle < 2; ++triangle)
	{
		for (size_t visType = 0; visType < 3; ++visType)
		{
			bool isTriangle = triangle != 0;
			DebugDrawVisibility visibility = static_cast<DebugDrawVisibility>(visType);

			VertexInputAttributeDescription attributeDesc = { "POSITION_AND_COLOR", 0, 0, Format::R32G32B32A32_SFLOAT, 0 };
			VertexInputBindingDescription bindingDesc = { 0, sizeof(float) * 4, VertexInputRate::VERTEX };

			GraphicsPipelineCreateInfo pipelineCreateInfo{};
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("assets/shaders/debugDraw_vs");
			builder.setFragmentShader("assets/shaders/debugDraw_ps");
			builder.setInputAssemblyState(isTriangle ? PrimitiveTopology::TRIANGLE_LIST : PrimitiveTopology::LINE_LIST, false);
			builder.setVertexBindingDescription(bindingDesc);
			builder.setVertexAttributeDescription(attributeDesc);
			builder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
			if (visibility != DebugDrawVisibility::Always)
			{
				builder.setDepthTest(true, false, visibility == DebugDrawVisibility::Visible ? CompareOp::GREATER_OR_EQUAL : CompareOp::LESS);
			}
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
			builder.setColorAttachmentFormat(Format::B8G8R8A8_UNORM);
			builder.setPolygonModeCullMode(gal::PolygonMode::FILL, gal::CullModeFlags::NONE, gal::FrontFace::COUNTER_CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
			DescriptorSetLayoutDeclaration layoutDecl = { offsetBufferSetLayout, 1, &usedOffsetBufferBinding };
			builder.setPipelineLayoutDescription(1, &layoutDecl, 0, {}, 0, nullptr, -1);

			device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_debugDrawPipelines[visType + (triangle * 3)]);
		}
	}
}

PostProcessModule::~PostProcessModule() noexcept
{
	m_device->destroyComputePipeline(m_luminanceHistogramPipeline);
	m_device->destroyComputePipeline(m_autoExposurePipeline);
	m_device->destroyComputePipeline(m_tonemapPipeline);
	m_device->destroyGraphicsPipeline(m_outlineIDPipeline);
	m_device->destroyGraphicsPipeline(m_outlineIDSkinnedPipeline);
	m_device->destroyGraphicsPipeline(m_outlineIDAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_outlineIDSkinnedAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_outlinePipeline);
	m_device->destroyGraphicsPipeline(m_debugNormalsPipeline);
	m_device->destroyGraphicsPipeline(m_debugNormalsSkinnedPipeline);
	for (auto &pso : m_debugDrawPipelines)
	{
		m_device->destroyGraphicsPipeline(pso);
	}
}

void PostProcessModule::record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept
{
	constexpr uint32_t k_histogramSize = 256;

	// histogram buffer
	rg::ResourceHandle histogramBufferHandle = graph->createBuffer(rg::BufferDesc::create("Luminance Histogram Buffer", k_histogramSize * sizeof(uint32_t), BufferUsageFlags::BYTE_BUFFER_BIT | BufferUsageFlags::RW_BYTE_BUFFER_BIT));
	rg::ResourceViewHandle histogramBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::createDefault("Luminance Histogram Buffer", histogramBufferHandle, graph));

	const float exposureLowPercentage = 0.0f;
	const float exposureHighPercentage = 1.0f;
	const float exposureSpeedUp = 3.0f;
	const float exposureSpeedDown = 1.0f;
	const float exposureCompensation = 1.0f;
	const float exposureMin = 0.0001f;
	const float exposureMax = 100.0f;
	const bool exposureFixed = false;
	const float exposureFixedValue = 2.0f;
	float exposureHistogramLogMin = -10.0f;
	float exposureHistogramLogMax = 17.0f;
	exposureHistogramLogMin = fminf(exposureHistogramLogMin, exposureHistogramLogMax - 1e-7f); // ensure logMin is a little bit less than logMax

	rg::ResourceUsageDesc luminanceHistogramUsageDescs[] =
	{
		{data.m_lightingImageView, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{data.m_exposureBufferViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{histogramBufferViewHandle, {ResourceState::CLEAR_RESOURCE}, {ResourceState::RW_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("Luminance Histogram", rg::QueueType::GRAPHICS, eastl::size(luminanceHistogramUsageDescs), luminanceHistogramUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Luminance Histogram");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Luminance Histogram");
			PROFILING_ZONE_SCOPED;

			// clear histogram
			{
				cmdList->fillBuffer(registry.getBuffer(histogramBufferHandle), 0, k_histogramSize * sizeof(uint32_t), 0);

				Barrier b = Initializers::bufferBarrier(registry.getBuffer(histogramBufferHandle), PipelineStageFlags::TRANSFER_BIT, PipelineStageFlags::COMPUTE_SHADER_BIT, ResourceState::CLEAR_RESOURCE, ResourceState::RW_RESOURCE);
				cmdList->barrier(1, &b);
			}

			cmdList->bindPipeline(m_luminanceHistogramPipeline);

			cmdList->bindDescriptorSets(m_luminanceHistogramPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

			LuminanceHistogramPushConsts pushConsts;
			pushConsts.width = data.m_width;
			pushConsts.scale = 1.0f / (exposureHistogramLogMax - exposureHistogramLogMin);
			pushConsts.bias = -exposureHistogramLogMin * pushConsts.scale;
			pushConsts.inputTextureIndex = registry.getBindlessHandle(data.m_lightingImageView, DescriptorType::TEXTURE);
			pushConsts.exposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferViewHandle, DescriptorType::BYTE_BUFFER);
			pushConsts.resultLuminanceBufferIndex = registry.getBindlessHandle(histogramBufferViewHandle, DescriptorType::RW_BYTE_BUFFER);

			cmdList->pushConstants(m_luminanceHistogramPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch(data.m_height, 1, 1);
		});

	rg::ResourceUsageDesc autoExposureUsageDescs[] =
	{
		{histogramBufferViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{data.m_exposureBufferViewHandle, {ResourceState::RW_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("Auto Exposure", rg::QueueType::GRAPHICS, eastl::size(autoExposureUsageDescs), autoExposureUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Auto Exposure");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Auto Exposure");
			PROFILING_ZONE_SCOPED;

			cmdList->bindPipeline(m_autoExposurePipeline);

			cmdList->bindDescriptorSets(m_autoExposurePipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

			AutoExposurePushConsts pushConsts;
			pushConsts.precomputedTermUp = 1.0f - expf(-data.m_deltaTime * exposureSpeedUp);
			pushConsts.precomputedTermDown = 1.0f - expf(-data.m_deltaTime * exposureSpeedDown);
			pushConsts.invScale = exposureHistogramLogMax - exposureHistogramLogMin;
			pushConsts.bias = -exposureHistogramLogMin * (1.0f / pushConsts.invScale);
			pushConsts.lowerBound = static_cast<uint32_t>(data.m_width * data.m_height * exposureLowPercentage);
			pushConsts.upperBound = static_cast<uint32_t>(data.m_width * data.m_height * exposureHighPercentage);

			// ensure at least one pixel passes
			if (pushConsts.lowerBound == pushConsts.upperBound)
			{
				pushConsts.lowerBound -= glm::min(pushConsts.lowerBound, 1u);
				pushConsts.upperBound += 1;
			}

			const bool exposureFixed = false;

			pushConsts.exposureCompensation = exposureCompensation;
			pushConsts.exposureMin = exposureMin;
			pushConsts.exposureMax = exposureFixed ? exposureFixedValue : exposureMax;
			pushConsts.exposureMax = fmaxf(pushConsts.exposureMax, 1e-7f);
			pushConsts.exposureMin = eastl::clamp(pushConsts.exposureMin, 1e-7f, pushConsts.exposureMax);
			pushConsts.fixExposureToMax = exposureFixed;
			pushConsts.inputHistogramBufferIndex = registry.getBindlessHandle(histogramBufferViewHandle, DescriptorType::BYTE_BUFFER);
			pushConsts.resultExposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferViewHandle, DescriptorType::RW_BYTE_BUFFER);

			cmdList->pushConstants(m_autoExposurePipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch(1, 1, 1);
		});

	rg::ResourceUsageDesc tonemapUsageDescs[] =
	{
		{data.m_lightingImageView, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{data.m_exposureBufferViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::COMPUTE_SHADER_BIT}},
		{data.m_resultImageViewHandle, {ResourceState::RW_RESOURCE_WRITE_ONLY, PipelineStageFlags::COMPUTE_SHADER_BIT}},
	};
	graph->addPass("Tonemap", rg::QueueType::GRAPHICS, eastl::size(tonemapUsageDescs), tonemapUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Tonemap");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Tonemap");
			PROFILING_ZONE_SCOPED;

			cmdList->bindPipeline(m_tonemapPipeline);

			cmdList->bindDescriptorSets(m_tonemapPipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

			TonemapPushConsts pushConsts;
			pushConsts.resolution[0] = data.m_width;
			pushConsts.resolution[1] = data.m_height;
			pushConsts.texelSize[0] = 1.0f / data.m_width;
			pushConsts.texelSize[1] = 1.0f / data.m_height;
			pushConsts.time = data.m_time;
			pushConsts.inputImageIndex = registry.getBindlessHandle(data.m_lightingImageView, DescriptorType::TEXTURE);
			pushConsts.exposureBufferIndex = registry.getBindlessHandle(data.m_exposureBufferViewHandle, DescriptorType::BYTE_BUFFER);
			pushConsts.outputImageIndex = registry.getBindlessHandle(data.m_resultImageViewHandle, DescriptorType::RW_TEXTURE);

			cmdList->pushConstants(m_tonemapPipeline, ShaderStageFlags::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((data.m_width + 7) / 8, (data.m_height + 7) / 8, 1);
		});

	if (data.m_renderOutlines)
	{
		// outline depth buffer
		rg::ResourceHandle outlineDepthBufferImageHandle = graph->createImage(rg::ImageDesc::create("Outline Depth Buffer", Format::D32_SFLOAT, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
		rg::ResourceViewHandle outlineDepthBufferImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Outline Depth Buffer", outlineDepthBufferImageHandle, graph));

		// outline ID texture
		rg::ResourceHandle outlineIDImageHandle = graph->createImage(rg::ImageDesc::create("Outline ID Image", Format::R8_UNORM, ImageUsageFlags::COLOR_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, data.m_width, data.m_height));
		rg::ResourceViewHandle outlineIDImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Outline ID Image", outlineIDImageHandle, graph));

		rg::ResourceUsageDesc outlineIDUsageDescs[]
		{
			{ outlineDepthBufferImageViewHandle, {ResourceState::WRITE_DEPTH_STENCIL} },
			{ outlineIDImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} },
		};
		graph->addPass("Outline IDs", rg::QueueType::GRAPHICS, eastl::size(outlineIDUsageDescs), outlineIDUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Outline IDs");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Outline IDs");
				PROFILING_ZONE_SCOPED;

				ColorAttachmentDescription attachmentDescs[]{ { registry.getImageView(outlineIDImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE } };
				DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(outlineDepthBufferImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE };
				Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

				cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
					cmdList->setScissor(0, 1, &scissor);

					struct PassConstants
					{
						float viewProjectionMatrix[16];
						uint32_t skinningMatricesBufferIndex;
						uint32_t materialBufferIndex;
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
					passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
					passConsts.materialBufferIndex = data.m_materialsBufferHandle;

					uint32_t passConstsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

					const eastl::vector<SubMeshInstanceData> *instancesArr[]
					{
						&data.m_outlineRenderList->m_opaque,
						&data.m_outlineRenderList->m_opaqueAlphaTested,
						&data.m_outlineRenderList->m_opaqueSkinned,
						&data.m_outlineRenderList->m_opaqueSkinnedAlphaTested,
					};
					GraphicsPipeline *pipelines[]
					{
						m_outlineIDPipeline,
						m_outlineIDAlphaTestedPipeline,
						m_outlineIDSkinnedPipeline,
						m_outlineIDSkinnedAlphaTestedPipeline,
					};

					uint32_t curOutlineID = 1;

					for (size_t listType = 0; listType < eastl::size(instancesArr); ++listType)
					{
						if (instancesArr[listType]->empty())
						{
							continue;
						}
						const bool skinned = listType >= 2;
						const bool alphaTested = (listType & 1) != 0;
						auto *pipeline = pipelines[listType];

						cmdList->bindPipeline(pipeline);

						gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

						for (const auto &instance : *instancesArr[listType])
						{
							struct MeshConstants
							{
								float modelMatrix[16];
								uint32_t outlineID;
								uint32_t uintData[2];
							};

							MeshConstants consts{};
							memcpy(consts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
							consts.outlineID = 1;// curOutlineID++;

							size_t uintDataOffset = 0;
							if (skinned)
							{
								consts.uintData[uintDataOffset++] = instance.m_skinningMatricesOffset;
							}
							if (alphaTested)
							{
								consts.uintData[uintDataOffset++] = instance.m_materialHandle;
							}

							cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT, 0, static_cast<uint32_t>(sizeof(float) * 16 + sizeof(uint32_t) * (uintDataOffset + 1)), &consts);


							Buffer *vertexBuffers[]
							{
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
							};

							const uint32_t vertexCount = data.m_meshDrawInfo[instance.m_subMeshHandle].m_vertexCount;

							const size_t alignedPositionsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
							const size_t alignedNormalsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
							const size_t alignedTangentsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 4, sizeof(float) * 4);
							const size_t alignedTexCoordsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 2, sizeof(float) * 4);
							const size_t alignedJointIndicesBufferSize = util::alignUp<size_t>(vertexCount * sizeof(uint32_t) * 2, sizeof(float) * 4);
							const size_t alignedJointWeightsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(uint32_t), sizeof(float) * 4);

							eastl::fixed_vector<uint64_t, 4> vertexBufferOffsets;
							vertexBufferOffsets.push_back(0); // positions
							if (alphaTested)
							{
								vertexBufferOffsets.push_back(alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize); // texcoords
							}
							if (skinned)
							{
								vertexBufferOffsets.push_back(alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize); // joint indices
								vertexBufferOffsets.push_back(alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize + alignedJointIndicesBufferSize); // joint weights
							}

							cmdList->bindIndexBuffer(data.m_meshBufferHandles[instance.m_subMeshHandle].m_indexBuffer, 0, IndexType::UINT16);
							cmdList->bindVertexBuffers(0, static_cast<uint32_t>(vertexBufferOffsets.size()), vertexBuffers, vertexBufferOffsets.data());
							cmdList->drawIndexed(data.m_meshDrawInfo[instance.m_subMeshHandle].m_indexCount, 1, 0, 0, 0);
						}
					}
				}
				cmdList->endRenderPass();
			});

		rg::ResourceUsageDesc outlineBlendUsageDescs[]
		{
			{ outlineDepthBufferImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
			{ outlineIDImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
			{ data.m_depthBufferImageViewHandle, {ResourceState::READ_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} },
			{ data.m_resultImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT} },
		};
		graph->addPass("Outline Blend", rg::QueueType::GRAPHICS, eastl::size(outlineBlendUsageDescs), outlineBlendUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Outline Blend");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Outline Blend");
				PROFILING_ZONE_SCOPED;

				ColorAttachmentDescription attachmentDescs[]{ { registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE } };
				Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

				cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, nullptr, renderRect, false);
				{
					cmdList->bindPipeline(m_outlinePipeline);

					gal::Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					gal::Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
					cmdList->setScissor(0, 1, &scissor);

					cmdList->bindDescriptorSets(m_outlinePipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);

					struct PushConsts
					{
						glm::vec3 outlineColor;
						uint32_t outlineIDTextureIndex;
						uint32_t outlineDepthTextureIndex;
						uint32_t sceneDepthTextureIndex;
					};

					PushConsts pushConsts{};
					pushConsts.outlineColor = glm::vec3(1.0f, 0.5f, 0.0f);
					pushConsts.outlineIDTextureIndex = registry.getBindlessHandle(outlineIDImageViewHandle, DescriptorType::TEXTURE);
					pushConsts.outlineDepthTextureIndex = registry.getBindlessHandle(outlineDepthBufferImageViewHandle, DescriptorType::TEXTURE);
					pushConsts.sceneDepthTextureIndex = registry.getBindlessHandle(data.m_depthBufferImageViewHandle, DescriptorType::TEXTURE);

					cmdList->pushConstants(m_outlinePipeline, ShaderStageFlags::PIXEL_BIT, 0, sizeof(pushConsts), &pushConsts);

					cmdList->draw(3, 1, 0, 0);
				}
				cmdList->endRenderPass();
			});
	}

	if (data.m_debugNormals)
	{
		rg::ResourceUsageDesc debugNormalsUsageDescs[] =
		{
			{data.m_resultImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
			{data.m_depthBufferImageViewHandle, {ResourceState::READ_DEPTH_STENCIL}},
		};
		graph->addPass("Debug Normals", rg::QueueType::GRAPHICS, eastl::size(debugNormalsUsageDescs), debugNormalsUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Debug Normals");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Debug Normals");
				PROFILING_ZONE_SCOPED;

				ColorAttachmentDescription attachmentDescs[]{ { registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE } };
				DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(data.m_depthBufferImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
				Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

				cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
					cmdList->setScissor(0, 1, &scissor);

					struct PassConstants
					{
						float viewProjectionMatrix[16];
						uint32_t skinningMatricesBufferIndex;
						float normalsLength;
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
					passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
					passConsts.normalsLength = 0.1f;

					uint32_t passConstsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

					const eastl::vector<SubMeshInstanceData> *instancesArr[]
					{
						&data.m_renderList->m_opaque,
						&data.m_renderList->m_opaqueAlphaTested,
						&data.m_renderList->m_opaqueSkinned,
						&data.m_renderList->m_opaqueSkinnedAlphaTested,
					};
					GraphicsPipeline *pipelines[]
					{
						m_debugNormalsPipeline,
						m_debugNormalsPipeline,
						m_debugNormalsSkinnedPipeline,
						m_debugNormalsSkinnedPipeline,
					};

					GraphicsPipeline *prevPipeline = nullptr;
					for (size_t listType = 0; listType < eastl::size(instancesArr); ++listType)
					{
						if (instancesArr[listType]->empty())
						{
							continue;
						}

						const bool skinned = listType >= 2;
						auto *pipeline = pipelines[listType];

						if (pipeline != prevPipeline)
						{
							cmdList->bindPipeline(pipeline);

							gal::DescriptorSet *sets[] = { data.m_offsetBufferSet, data.m_bindlessSet };
							cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

							prevPipeline = pipeline;
						}

						for (const auto &instance : *instancesArr[listType])
						{
							struct MeshConstants
							{
								float modelMatrix[16];
							};

							struct SkinnedMeshConstants
							{
								float modelMatrix[16];
								uint32_t skinningMatricesOffset;
							};

							MeshConstants consts{};
							SkinnedMeshConstants skinnedConsts{};

							if (skinned)
							{
								memcpy(skinnedConsts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
								skinnedConsts.skinningMatricesOffset = instance.m_skinningMatricesOffset;
							}
							else
							{
								memcpy(consts.modelMatrix, &data.m_modelMatrices[instance.m_transformIndex][0][0], sizeof(float) * 16);
							}

							cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT, 0, skinned ? sizeof(skinnedConsts) : sizeof(consts), skinned ? (void *)&skinnedConsts : (void *)&consts);


							Buffer *vertexBuffers[]
							{
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
								data.m_meshBufferHandles[instance.m_subMeshHandle].m_vertexBuffer,
							};

							const uint32_t vertexCount = data.m_meshDrawInfo[instance.m_subMeshHandle].m_vertexCount;

							const size_t alignedPositionsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
							const size_t alignedNormalsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 3, sizeof(float) * 4);
							const size_t alignedTangentsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 4, sizeof(float) * 4);
							const size_t alignedTexCoordsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(float) * 2, sizeof(float) * 4);
							const size_t alignedJointIndicesBufferSize = util::alignUp<size_t>(vertexCount * sizeof(uint32_t) * 2, sizeof(float) * 4);
							const size_t alignedJointWeightsBufferSize = util::alignUp<size_t>(vertexCount * sizeof(uint32_t), sizeof(float) * 4);

							uint64_t vertexBufferOffsets[]
							{
								0, // positions
								alignedPositionsBufferSize, // normals
								alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize, // joint indices
								alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize + alignedJointIndicesBufferSize, // joint weights
							};

							cmdList->bindIndexBuffer(data.m_meshBufferHandles[instance.m_subMeshHandle].m_indexBuffer, 0, IndexType::UINT16);
							cmdList->bindVertexBuffers(0, skinned ? 4 : 2, vertexBuffers, vertexBufferOffsets);
							cmdList->drawIndexed(data.m_meshDrawInfo[instance.m_subMeshHandle].m_indexCount, 1, 0, 0, 0);
						}
					}
				}
				cmdList->endRenderPass();
			});
	}

	bool anyDebugDraws = false;
	{
		for (const auto &vertices : m_debugDrawVertices)
		{
			if (!vertices.empty())
			{
				anyDebugDraws = true;
				break;
			}
		}
	}

	if (anyDebugDraws)
	{
		// compute vertex buffer size requirements
		size_t debugVertexCount = 0;
		for (const auto &vertices : m_debugDrawVertices)
		{
			debugVertexCount += vertices.size();
		}

		// allocate buffer and copy data
		uint32_t vertexOffsets[6] = {};
		uint64_t vertexBufferByteOffset;
		{
			uint64_t allocSize = (sizeof(float) * 4) * static_cast<uint64_t>(debugVertexCount);
			auto *mappedPtr = data.m_vertexBufferAllocator->allocate(sizeof(float) * 4, &allocSize, &vertexBufferByteOffset);
			assert(mappedPtr);
			if (mappedPtr)
			{
				uint32_t curVertexOffset = 0;
				for (size_t i = 0; i < 6; ++i)
				{
					if (!m_debugDrawVertices[i].empty())
					{
						vertexOffsets[i] = curVertexOffset;
						const size_t vertexCount = m_debugDrawVertices[i].size();
						const size_t dataSize = vertexCount * (sizeof(float) * 4);
						memcpy(mappedPtr, m_debugDrawVertices[i].data(), dataSize);
						mappedPtr += dataSize;
						curVertexOffset += static_cast<uint32_t>(vertexCount);
					}
				}
			}
		}

		rg::ResourceUsageDesc debugDrawsUsageDescs[] =
		{
			{data.m_resultImageViewHandle, {ResourceState::WRITE_COLOR_ATTACHMENT}},
			{data.m_depthBufferImageViewHandle, {ResourceState::READ_DEPTH_STENCIL}},
		};
		graph->addPass("Debug Draws", rg::QueueType::GRAPHICS, eastl::size(debugDrawsUsageDescs), debugDrawsUsageDescs, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Debug Draws");
				PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "Debug Draws");
				PROFILING_ZONE_SCOPED;

				ColorAttachmentDescription attachmentDescs[]{ { registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE } };
				DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(data.m_depthBufferImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
				Rect renderRect{ {0, 0}, {data.m_width, data.m_height} };

				cmdList->beginRenderPass(static_cast<uint32_t>(eastl::size(attachmentDescs)), attachmentDescs, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)data.m_width, (float)data.m_height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {data.m_width, data.m_height} };
					cmdList->setScissor(0, 1, &scissor);

					struct PassConstants
					{
						float viewProjectionMatrix[16];
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &data.m_viewProjectionMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));

					uint32_t passConstsAddress = (uint32_t)data.m_bufferAllocator->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

					for (size_t i = 0; i < 6; ++i)
					{
						if (m_debugDrawVertices[i].empty())
						{
							continue;
						}

						cmdList->bindPipeline(m_debugDrawPipelines[i]);
						cmdList->bindDescriptorSets(m_debugDrawPipelines[i], 0, 1, &data.m_offsetBufferSet, 1, &passConstsAddress);
						
						Buffer *vertexBuffer = data.m_vertexBufferAllocator->getBuffer();
						uint64_t vertexBufferOffset = vertexBufferByteOffset;
						cmdList->bindVertexBuffers(0, 1, &vertexBuffer, &vertexBufferOffset);

						uint32_t vertexDrawCount = static_cast<uint32_t>(m_debugDrawVertices[i].size());
						cmdList->draw(vertexDrawCount, 1, vertexOffsets[i], 0);
					}
				}
				cmdList->endRenderPass();
			});
	}
}

void PostProcessModule::clearDebugGeometry() noexcept
{
	for (auto &vertices : m_debugDrawVertices)
	{
		vertices.clear();
	}
}

void PostProcessModule::drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept
{
	m_debugDrawVertices[static_cast<size_t>(visibility)].push_back({ position0, glm::packUnorm4x8(color0) });
	m_debugDrawVertices[static_cast<size_t>(visibility)].push_back({ position1, glm::packUnorm4x8(color1) });
}

void PostProcessModule::drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept
{
	m_debugDrawVertices[static_cast<size_t>(visibility) + 3].push_back({ position0, glm::packUnorm4x8(color0) });
	m_debugDrawVertices[static_cast<size_t>(visibility) + 3].push_back({ position1, glm::packUnorm4x8(color1) });
	m_debugDrawVertices[static_cast<size_t>(visibility) + 3].push_back({ position2, glm::packUnorm4x8(color2) });
}
