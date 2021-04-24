#include "PipelineVk.h"
#include "GraphicsDeviceVk.h"
#include "UtilityVk.h"
#include "utility/Utility.h"
#include "RenderPassDescriptionVk.h"

using namespace gal;

static void createShaderStage(VkDevice device, const ShaderStageCreateInfo &stageDesc, VkShaderStageFlagBits stageFlag, VkShaderModule &shaderModule, VkPipelineShaderStageCreateInfo &stageCreateInfo);
static void createPipelineLayout(VkDevice device,
	const PipelineLayoutCreateInfo &layoutCreateInfo,
	VkPipelineLayout &pipelineLayout,
	VkDescriptorSetLayout &staticSamplerDescriptorSetLayout,
	VkDescriptorPool &staticSamplerDescriptorPool,
	VkDescriptorSet &staticSamplerDescriptorSet,
	std::vector<VkSampler> &staticSamplers);

gal::GraphicsPipelineVk::GraphicsPipelineVk(GraphicsDeviceVk &device, const GraphicsPipelineCreateInfo &createInfo)
	:m_pipeline(VK_NULL_HANDLE),
	m_pipelineLayout(VK_NULL_HANDLE),
	m_device(&device)
{
	VkDevice deviceVk = m_device->getDevice();
	uint32_t stageCount = 0;
	VkShaderModule shaderModules[5] = {};
	VkPipelineShaderStageCreateInfo shaderStages[5] = {};
	VkRenderPass renderPass;

	// create shaders and perform reflection
	{
		if (createInfo.m_vertexShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_vertexShader, VK_SHADER_STAGE_VERTEX_BIT, shaderModules[stageCount], shaderStages[stageCount]);
			++stageCount;
		}

		if (createInfo.m_hullShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_hullShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shaderModules[stageCount], shaderStages[stageCount]);
			++stageCount;
		}

		if (createInfo.m_domainShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_domainShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shaderModules[stageCount], shaderStages[stageCount]);
			++stageCount;
		}

		if (createInfo.m_geometryShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT, shaderModules[stageCount], shaderStages[stageCount]);
			++stageCount;
		}

		if (createInfo.m_pixelShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_pixelShader, VK_SHADER_STAGE_FRAGMENT_BIT, shaderModules[stageCount], shaderStages[stageCount]);
			++stageCount;
		}
	}

	// get compatible renderpass
	{
		RenderPassDescriptionVk::ColorAttachmentDescriptionVk colorAttachments[8];
		RenderPassDescriptionVk::DepthStencilAttachmentDescriptionVk depthStencilAttachment;

		uint32_t attachmentCount = 0;

		VkSampleCountFlagBits samples = static_cast<VkSampleCountFlagBits>(createInfo.m_multiSampleState.m_rasterizationSamples);

		// fill out color attachment info structs
		for (uint32_t i = 0; i < createInfo.m_attachmentFormats.m_colorAttachmentCount; ++i)
		{
			auto &attachmentDesc = colorAttachments[i];
			attachmentDesc = {};
			attachmentDesc.m_format = UtilityVk::translate(createInfo.m_attachmentFormats.m_colorAttachmentFormats[i]);
			attachmentDesc.m_samples = samples;

			// these values dont actually matter, because they dont affect renderpass compatibility and are provided later by the actual renderpass used when drawing with this pipeline.
			// still, we should try to guess the most likely values in order to reduce the need for creating additional renderpasses
			attachmentDesc.m_loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachmentDesc.m_storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			++attachmentCount;
		}

		// fill out depth/stencil attachment info struct
		if (createInfo.m_depthStencilState.m_depthTestEnable)
		{
			auto &attachmentDesc = depthStencilAttachment;
			attachmentDesc = {};
			attachmentDesc.m_format = UtilityVk::translate(createInfo.m_attachmentFormats.m_depthStencilFormat);
			attachmentDesc.m_samples = samples;

			// these values dont actually matter, because they dont affect renderpass compatibility and are provided later by the actual renderpass used when drawing with this pipeline.
			// still, we should try to guess the most likely values in order to reduce the need for creating additional renderpasses
			attachmentDesc.m_loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachmentDesc.m_storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDesc.m_stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDesc.m_stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDesc.m_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			++attachmentCount;
		}

		// get renderpass
		{
			RenderPassDescriptionVk renderPassDescription;
			renderPassDescription.setColorAttachments(createInfo.m_attachmentFormats.m_colorAttachmentCount, colorAttachments);
			if (createInfo.m_depthStencilState.m_depthTestEnable)
			{
				renderPassDescription.setDepthStencilAttachment(depthStencilAttachment);
			}
			renderPassDescription.finalize();

			renderPass = m_device->getRenderPass(renderPassDescription);
		}
	}

	// create pipeline layout
	m_staticSamplerDescriptorSetIndex = createInfo.m_layoutCreateInfo.m_staticSamplerSet;
	createPipelineLayout(
		deviceVk, 
		createInfo.m_layoutCreateInfo, 
		m_pipelineLayout,
		m_staticSamplerDescriptorSetLayout,
		m_staticSamplerDescriptorPool,
		m_staticSamplerDescriptorSet,
		m_staticSamplers);

	VkPipelineVertexInputStateCreateInfo vertexInputState{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkVertexInputBindingDescription vertexBindingDescriptions[VertexInputState::MAX_VERTEX_BINDING_DESCRIPTIONS];
	VkVertexInputAttributeDescription vertexAttributeDescriptions[VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS];
	{
		vertexInputState.vertexBindingDescriptionCount = createInfo.m_vertexInputState.m_vertexBindingDescriptionCount;
		vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions;
		vertexInputState.vertexAttributeDescriptionCount = createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount;
		vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions;

		for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexBindingDescriptionCount; ++i)
		{
			auto &binding = vertexBindingDescriptions[i];
			binding.binding = createInfo.m_vertexInputState.m_vertexBindingDescriptions[i].m_binding;
			binding.stride = createInfo.m_vertexInputState.m_vertexBindingDescriptions[i].m_stride;
			binding.inputRate = UtilityVk::translate(createInfo.m_vertexInputState.m_vertexBindingDescriptions[i].m_inputRate);
		}

		for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount; ++i)
		{
			auto &attribute = vertexAttributeDescriptions[i];
			attribute.location = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_location;
			attribute.binding = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_binding;
			attribute.format = UtilityVk::translate(createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_format);
			attribute.offset = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_offset;
		}
	}


	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	{
		inputAssemblyState.topology = UtilityVk::translate(createInfo.m_inputAssemblyState.m_primitiveTopology);
		inputAssemblyState.primitiveRestartEnable = createInfo.m_inputAssemblyState.m_primitiveRestartEnable;
	}

	VkPipelineTessellationStateCreateInfo tesselatationState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
	{
		tesselatationState.patchControlPoints = createInfo.m_tesselationState.m_patchControlPoints;
	}

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	{
		viewportState.viewportCount = createInfo.m_viewportState.m_viewportCount;
		viewportState.pViewports = reinterpret_cast<const VkViewport *>(createInfo.m_viewportState.m_viewports);
		viewportState.scissorCount = createInfo.m_viewportState.m_viewportCount;
		viewportState.pScissors = reinterpret_cast<const VkRect2D *>(createInfo.m_viewportState.m_scissors);
	}


	VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	{
		rasterizationState.depthClampEnable = createInfo.m_rasterizationState.m_depthClampEnable;
		rasterizationState.rasterizerDiscardEnable = createInfo.m_rasterizationState.m_rasterizerDiscardEnable;
		rasterizationState.polygonMode = UtilityVk::translate(createInfo.m_rasterizationState.m_polygonMode);
		rasterizationState.cullMode = UtilityVk::translateCullModeFlags(createInfo.m_rasterizationState.m_cullMode);
		rasterizationState.frontFace = UtilityVk::translate(createInfo.m_rasterizationState.m_frontFace);
		rasterizationState.depthBiasEnable = createInfo.m_rasterizationState.m_depthBiasEnable;
		rasterizationState.depthBiasConstantFactor = createInfo.m_rasterizationState.m_depthBiasConstantFactor;
		rasterizationState.depthBiasClamp = createInfo.m_rasterizationState.m_depthBiasClamp;
		rasterizationState.depthBiasSlopeFactor = createInfo.m_rasterizationState.m_depthBiasSlopeFactor;
		rasterizationState.lineWidth = createInfo.m_rasterizationState.m_lineWidth;
	}


	VkPipelineMultisampleStateCreateInfo multisamplingState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	{
		multisamplingState.rasterizationSamples = static_cast<VkSampleCountFlagBits>(createInfo.m_multiSampleState.m_rasterizationSamples);
		multisamplingState.sampleShadingEnable = createInfo.m_multiSampleState.m_sampleShadingEnable;
		multisamplingState.minSampleShading = createInfo.m_multiSampleState.m_minSampleShading;
		multisamplingState.pSampleMask = &createInfo.m_multiSampleState.m_sampleMask;
		multisamplingState.alphaToCoverageEnable = createInfo.m_multiSampleState.m_alphaToCoverageEnable;
		multisamplingState.alphaToOneEnable = createInfo.m_multiSampleState.m_alphaToOneEnable;
	}


	VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	{
		auto translateStencilOpState = [](const StencilOpState &state)
		{
			VkStencilOpState result{};
			result.failOp = UtilityVk::translate(state.m_failOp);
			result.passOp = UtilityVk::translate(state.m_passOp);
			result.depthFailOp = UtilityVk::translate(state.m_depthFailOp);
			result.compareOp = UtilityVk::translate(state.m_compareOp);
			result.compareMask = state.m_compareMask;
			result.writeMask = state.m_writeMask;
			result.reference = state.m_reference;
			return result;
		};

		depthStencilState.depthTestEnable = createInfo.m_depthStencilState.m_depthTestEnable;
		depthStencilState.depthWriteEnable = createInfo.m_depthStencilState.m_depthWriteEnable;
		depthStencilState.depthCompareOp = UtilityVk::translate(createInfo.m_depthStencilState.m_depthCompareOp);
		depthStencilState.depthBoundsTestEnable = createInfo.m_depthStencilState.m_depthBoundsTestEnable;
		depthStencilState.stencilTestEnable = createInfo.m_depthStencilState.m_stencilTestEnable;
		depthStencilState.front = translateStencilOpState(createInfo.m_depthStencilState.m_front);
		depthStencilState.back = translateStencilOpState(createInfo.m_depthStencilState.m_back);
		depthStencilState.minDepthBounds = createInfo.m_depthStencilState.m_minDepthBounds;
		depthStencilState.maxDepthBounds = createInfo.m_depthStencilState.m_maxDepthBounds;
	}


	VkPipelineColorBlendStateCreateInfo blendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[8];
	{
		blendState.logicOpEnable = createInfo.m_blendState.m_logicOpEnable;
		blendState.logicOp = UtilityVk::translate(createInfo.m_blendState.m_logicOp);
		blendState.attachmentCount = createInfo.m_blendState.m_attachmentCount;
		blendState.pAttachments = colorBlendAttachmentStates;
		blendState.blendConstants[0] = createInfo.m_blendState.m_blendConstants[0];
		blendState.blendConstants[1] = createInfo.m_blendState.m_blendConstants[1];
		blendState.blendConstants[2] = createInfo.m_blendState.m_blendConstants[2];
		blendState.blendConstants[3] = createInfo.m_blendState.m_blendConstants[3];

		for (size_t i = 0; i < createInfo.m_blendState.m_attachmentCount; ++i)
		{
			auto &state = colorBlendAttachmentStates[i];
			state.blendEnable = createInfo.m_blendState.m_attachments[i].m_blendEnable;
			state.srcColorBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_srcColorBlendFactor);
			state.dstColorBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_dstColorBlendFactor);
			state.colorBlendOp = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_colorBlendOp);
			state.srcAlphaBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_srcAlphaBlendFactor);
			state.dstAlphaBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_dstAlphaBlendFactor);
			state.alphaBlendOp = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_alphaBlendOp);
			state.colorWriteMask = UtilityVk::translateColorComponentFlags(createInfo.m_blendState.m_attachments[i].m_colorWriteMask);
		}
	}

	VkDynamicState dynamicStatesArray[9];
	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.pDynamicStates = dynamicStatesArray;
	UtilityVk::translateDynamicStateFlags(createInfo.m_dynamicStateFlags, dynamicState.dynamicStateCount, dynamicStatesArray);


	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.flags = 0;
	pipelineInfo.stageCount = stageCount;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputState;
	pipelineInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineInfo.pTessellationState = &tesselatationState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizationState;
	pipelineInfo.pMultisampleState = &multisamplingState;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &blendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	UtilityVk::checkResult(vkCreateGraphicsPipelines(deviceVk, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create pipeline!");

	for (uint32_t i = 0; i < stageCount; ++i)
	{
		vkDestroyShaderModule(deviceVk, shaderModules[i], nullptr);
	}
}

gal::GraphicsPipelineVk::~GraphicsPipelineVk()
{
	VkDevice deviceVk = m_device->getDevice();
	vkDestroyPipeline(deviceVk, m_pipeline, nullptr);
	vkDestroyPipelineLayout(deviceVk, m_pipelineLayout, nullptr);
	vkDestroyDescriptorPool(deviceVk, m_staticSamplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(deviceVk, m_staticSamplerDescriptorSetLayout, nullptr);

	for (auto sampler : m_staticSamplers)
	{
		vkDestroySampler(deviceVk, sampler, nullptr);
	}
}

void *gal::GraphicsPipelineVk::getNativeHandle() const
{
	return m_pipeline;
}

uint32_t gal::GraphicsPipelineVk::getDescriptorSetLayoutCount() const
{
	return 0;
}

const DescriptorSetLayout *gal::GraphicsPipelineVk::getDescriptorSetLayout(uint32_t index) const
{
	return nullptr;
}

VkPipelineLayout gal::GraphicsPipelineVk::getLayout() const
{
	return m_pipelineLayout;
}

void gal::GraphicsPipelineVk::bindStaticSamplerSet(VkCommandBuffer cmdBuf) const
{
	if (m_staticSamplerDescriptorSet != VK_NULL_HANDLE)
	{
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, m_staticSamplerDescriptorSetIndex, 1, &m_staticSamplerDescriptorSet, 0, nullptr);
	}
}

gal::ComputePipelineVk::ComputePipelineVk(GraphicsDeviceVk &device, const ComputePipelineCreateInfo &createInfo)
	:m_pipeline(VK_NULL_HANDLE),
	m_pipelineLayout(VK_NULL_HANDLE),
	m_device(&device)
{
	VkDevice deviceVk = m_device->getDevice();
	VkShaderModule compShaderModule;
	VkPipelineShaderStageCreateInfo compShaderStageInfo;

	createShaderStage(deviceVk, createInfo.m_computeShader, VK_SHADER_STAGE_COMPUTE_BIT, compShaderModule, compShaderStageInfo);

	m_staticSamplerDescriptorSetIndex = createInfo.m_layoutCreateInfo.m_staticSamplerSet;
	createPipelineLayout(
		deviceVk, 
		createInfo.m_layoutCreateInfo, 
		m_pipelineLayout,
		m_staticSamplerDescriptorSetLayout,
		m_staticSamplerDescriptorPool,
		m_staticSamplerDescriptorSet,
		m_staticSamplers);

	VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = m_pipelineLayout;

	UtilityVk::checkResult(vkCreateComputePipelines(deviceVk, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create pipeline!");

	vkDestroyShaderModule(deviceVk, compShaderModule, nullptr);
}

gal::ComputePipelineVk::~ComputePipelineVk()
{
	VkDevice deviceVk = m_device->getDevice();
	vkDestroyPipeline(deviceVk, m_pipeline, nullptr);
	vkDestroyPipelineLayout(deviceVk, m_pipelineLayout, nullptr);
	vkDestroyDescriptorPool(deviceVk, m_staticSamplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(deviceVk, m_staticSamplerDescriptorSetLayout, nullptr);

	for (auto sampler : m_staticSamplers)
	{
		vkDestroySampler(deviceVk, sampler, nullptr);
	}
}

void *gal::ComputePipelineVk::getNativeHandle() const
{
	return m_pipeline;
}

uint32_t gal::ComputePipelineVk::getDescriptorSetLayoutCount() const
{
	return 0;
}

const DescriptorSetLayout *gal::ComputePipelineVk::getDescriptorSetLayout(uint32_t index) const
{
	return nullptr;
}

VkPipelineLayout gal::ComputePipelineVk::getLayout() const
{
	return m_pipelineLayout;
}

void gal::ComputePipelineVk::bindStaticSamplerSet(VkCommandBuffer cmdBuf) const
{
	if (m_staticSamplerDescriptorSet != VK_NULL_HANDLE)
	{
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, m_staticSamplerDescriptorSetIndex, 1, &m_staticSamplerDescriptorSet, 0, nullptr);
	}
}

static void createShaderStage(VkDevice device, const ShaderStageCreateInfo &stageDesc, VkShaderStageFlagBits stageFlag, VkShaderModule &shaderModule, VkPipelineShaderStageCreateInfo &stageCreateInfo)
{
	char path[gal::ShaderStageCreateInfo::MAX_PATH_LENGTH + 5];
	strcpy_s(path, stageDesc.m_path);
	strcat_s(path, ".spv");
	size_t codeSize = 0;
	char *code = util::readBinaryFile(path, &codeSize);
	VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = codeSize;
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code);

	gal::UtilityVk::checkResult(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module!");

	delete[] code;

	stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stageCreateInfo.stage = stageFlag;
	stageCreateInfo.module = shaderModule;
	stageCreateInfo.pName = "main";
}

static void createPipelineLayout(
	VkDevice device,
	const PipelineLayoutCreateInfo &layoutCreateInfo,
	VkPipelineLayout &pipelineLayout,
	VkDescriptorSetLayout &staticSamplerDescriptorSetLayout,
	VkDescriptorPool &staticSamplerDescriptorPool,
	VkDescriptorSet &staticSamplerDescriptorSet,
	std::vector<VkSampler> &staticSamplers)
{
	staticSamplerDescriptorSetLayout = VK_NULL_HANDLE;
	staticSamplerDescriptorPool = VK_NULL_HANDLE;
	staticSamplerDescriptorSet = VK_NULL_HANDLE;
	staticSamplers.clear();

	// create static sampler set
	if (layoutCreateInfo.m_staticSamplerCount > 0)
	{
		staticSamplers.reserve(layoutCreateInfo.m_staticSamplerCount);
		std::vector<VkDescriptorSetLayoutBinding> staticSamplerBindings;
		staticSamplerBindings.reserve(layoutCreateInfo.m_staticSamplerCount);

		for (size_t i = 0; i < layoutCreateInfo.m_staticSamplerCount; ++i)
		{
			const auto &staticSamplerDesc = layoutCreateInfo.m_staticSamplerDescriptions[i];

			VkSamplerCreateInfo samplerCreateInfoVk{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			samplerCreateInfoVk.magFilter = UtilityVk::translate(staticSamplerDesc.m_magFilter);
			samplerCreateInfoVk.minFilter = UtilityVk::translate(staticSamplerDesc.m_minFilter);
			samplerCreateInfoVk.mipmapMode = UtilityVk::translate(staticSamplerDesc.m_mipmapMode);
			samplerCreateInfoVk.addressModeU = UtilityVk::translate(staticSamplerDesc.m_addressModeU);
			samplerCreateInfoVk.addressModeV = UtilityVk::translate(staticSamplerDesc.m_addressModeV);
			samplerCreateInfoVk.addressModeW = UtilityVk::translate(staticSamplerDesc.m_addressModeW);
			samplerCreateInfoVk.mipLodBias = staticSamplerDesc.m_mipLodBias;
			samplerCreateInfoVk.anisotropyEnable = staticSamplerDesc.m_anisotropyEnable;
			samplerCreateInfoVk.maxAnisotropy = staticSamplerDesc.m_maxAnisotropy;
			samplerCreateInfoVk.compareEnable = staticSamplerDesc.m_compareEnable;
			samplerCreateInfoVk.compareOp = UtilityVk::translate(staticSamplerDesc.m_compareOp);
			samplerCreateInfoVk.minLod = staticSamplerDesc.m_minLod;
			samplerCreateInfoVk.maxLod = staticSamplerDesc.m_maxLod;
			samplerCreateInfoVk.borderColor = UtilityVk::translate(staticSamplerDesc.m_borderColor);
			samplerCreateInfoVk.unnormalizedCoordinates = staticSamplerDesc.m_unnormalizedCoordinates;

			VkSampler sampler = {};
			UtilityVk::checkResult(vkCreateSampler(device, &samplerCreateInfoVk, nullptr, &sampler), "Failed to create Sampler!");
			staticSamplers.push_back(sampler);

			VkDescriptorSetLayoutBinding bindingVk{};
			bindingVk.binding = staticSamplerDesc.m_binding;
			bindingVk.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			bindingVk.descriptorCount = 1;
			bindingVk.stageFlags = UtilityVk::translateShaderStageFlags(staticSamplerDesc.m_stageFlags);
			bindingVk.pImmutableSamplers = &staticSamplers.back();

			staticSamplerBindings.push_back(bindingVk);
		}

		VkDescriptorSetLayoutCreateInfo samplerSetLayoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		samplerSetLayoutCreateInfo.bindingCount = layoutCreateInfo.m_staticSamplerCount;
		samplerSetLayoutCreateInfo.pBindings = staticSamplerBindings.data();

		gal::UtilityVk::checkResult(vkCreateDescriptorSetLayout(device, &samplerSetLayoutCreateInfo, nullptr, &staticSamplerDescriptorSetLayout), "Failed to create static sampler descriptor set layout!");

		VkDescriptorPoolSize descriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, layoutCreateInfo.m_staticSamplerCount };
		VkDescriptorPoolCreateInfo staticSamplerDescriptorPoolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		staticSamplerDescriptorPoolCreateInfo.maxSets = 1;
		staticSamplerDescriptorPoolCreateInfo.poolSizeCount = 1;
		staticSamplerDescriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;

		gal::UtilityVk::checkResult(vkCreateDescriptorPool(device, &staticSamplerDescriptorPoolCreateInfo, nullptr, &staticSamplerDescriptorPool), "Failed to create static sampler descriptor pool!");

		VkDescriptorSetAllocateInfo staticSamplerDescriptorSetAllocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		staticSamplerDescriptorSetAllocateInfo.descriptorPool = staticSamplerDescriptorPool;
		staticSamplerDescriptorSetAllocateInfo.descriptorSetCount = 1;
		staticSamplerDescriptorSetAllocateInfo.pSetLayouts = &staticSamplerDescriptorSetLayout;

		gal::UtilityVk::checkResult(vkAllocateDescriptorSets(device, &staticSamplerDescriptorSetAllocateInfo, &staticSamplerDescriptorSet), "Failed to allocate static sampler descriptor set!");
	}

	VkDescriptorSetLayout layoutsVk[5];
	for (size_t i = 0; i < layoutCreateInfo.m_descriptorSetLayoutCount; ++i)
	{
		DescriptorSetLayoutVk *layoutVk = dynamic_cast<DescriptorSetLayoutVk *>(layoutCreateInfo.m_descriptorSetLayoutDeclarations[i].m_layout);
		assert(layoutVk);
		layoutsVk[i] = (VkDescriptorSetLayout)layoutVk->getNativeHandle();
	}
	if (staticSamplerDescriptorSetLayout != VK_NULL_HANDLE)
	{
		layoutsVk[layoutCreateInfo.m_staticSamplerSet] = staticSamplerDescriptorSetLayout;
	}

	VkPushConstantRange pushConstantRange;
	pushConstantRange.stageFlags = UtilityVk::translateShaderStageFlags(layoutCreateInfo.m_pushConstStageFlags);
	pushConstantRange.offset = 0;
	pushConstantRange.size = layoutCreateInfo.m_pushConstRange;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = (staticSamplerDescriptorSetLayout != VK_NULL_HANDLE) ? (layoutCreateInfo.m_descriptorSetLayoutCount + 1) : layoutCreateInfo.m_descriptorSetLayoutCount;
	pipelineLayoutCreateInfo.pSetLayouts = layoutsVk;
	pipelineLayoutCreateInfo.pushConstantRangeCount = layoutCreateInfo.m_pushConstRange > 0 ? 1 : 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = layoutCreateInfo.m_pushConstRange > 0 ? &pushConstantRange : nullptr;

	gal::UtilityVk::checkResult(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout), "Failed to create PipelineLayout!");
}
