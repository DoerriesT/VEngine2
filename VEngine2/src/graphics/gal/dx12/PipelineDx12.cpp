#include "PipelineDx12.h"
#include "utility/Utility.h"
#include "UtilityDx12.h"
#include "utility/Memory.h"
//#include <vector>


using namespace gal;

static constexpr UINT s_rootConstRegister = 0;
static constexpr UINT s_rootConstSpace = 5000;

static char *loadShaderFile(const char *filename, size_t *shaderFileSize);
static ID3D12RootSignature *createRootSignature(ID3D12Device *device,
	bool useIA,
	uint32_t &rootDescriptorSetIndex,
	uint32_t &rootDescriptorCount,
	uint32_t &descriptorTableOffset,
	const PipelineLayoutCreateInfo &layoutCreateInfo);

gal::GraphicsPipelineDx12::GraphicsPipelineDx12(ID3D12Device *device, const GraphicsPipelineCreateInfo &createInfo)
	:m_pipeline(),
	m_rootSignature(),
	m_descriptorTableOffset(),
	m_primitiveTopology(),
	m_vertexBufferStrides(),
	m_blendFactors(),
	m_stencilRef()
{
	size_t vsCodeSize = 0;
	char *vsCode = nullptr;

	size_t dsCodeSize = 0;
	char *dsCode = nullptr;

	size_t hsCodeSize = 0;
	char *hsCode = nullptr;

	size_t gsCodeSize = 0;
	char *gsCode = nullptr;

	size_t psCodeSize = 0;
	char *psCode = nullptr;

	if (createInfo.m_vertexShader.m_path[0])
	{
		vsCode = loadShaderFile(createInfo.m_vertexShader.m_path, &vsCodeSize);
	}
	if (createInfo.m_hullShader.m_path[0])
	{
		hsCode = loadShaderFile(createInfo.m_hullShader.m_path, &dsCodeSize);
	}
	if (createInfo.m_domainShader.m_path[0])
	{
		dsCode = loadShaderFile(createInfo.m_domainShader.m_path, &hsCodeSize);
	}
	if (createInfo.m_geometryShader.m_path[0])
	{
		gsCode = loadShaderFile(createInfo.m_geometryShader.m_path, &gsCodeSize);
	}
	if (createInfo.m_pixelShader.m_path[0])
	{
		psCode = loadShaderFile(createInfo.m_pixelShader.m_path, &psCodeSize);
	}

	// create root signature from reflection data
	m_rootSignature = createRootSignature(device,
		createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount > 0,
		m_rootDescriptorSetIndex,
		m_rootDescriptorCount,
		m_descriptorTableOffset,
		createInfo.m_layoutCreateInfo);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateDesc{};
	stateDesc.pRootSignature = m_rootSignature;

	// shaders
	{
		stateDesc.VS.pShaderBytecode = vsCode;
		stateDesc.VS.BytecodeLength = vsCodeSize;
		stateDesc.PS.pShaderBytecode = psCode;
		stateDesc.PS.BytecodeLength = psCodeSize;
		stateDesc.DS.pShaderBytecode = dsCode;
		stateDesc.DS.BytecodeLength = dsCodeSize;
		stateDesc.HS.pShaderBytecode = hsCode;
		stateDesc.HS.BytecodeLength = hsCodeSize;
		stateDesc.GS.pShaderBytecode = gsCode;
		stateDesc.GS.BytecodeLength = gsCodeSize;
	}

	// stream output
	{
		stateDesc.StreamOutput.pSODeclaration = nullptr;
		stateDesc.StreamOutput.NumEntries = 0;
		stateDesc.StreamOutput.pBufferStrides = nullptr;
		stateDesc.StreamOutput.NumStrides = 0;
		stateDesc.StreamOutput.RasterizedStream = 0;
	}

	// blend state
	{
		stateDesc.BlendState.AlphaToCoverageEnable = createInfo.m_multiSampleState.m_alphaToCoverageEnable;
		stateDesc.BlendState.IndependentBlendEnable = false;
		for (size_t i = 0; i < 8; ++i)
		{
			const auto &blendDesc = createInfo.m_blendState.m_attachments[i];
			const uint32_t attachmentCount = createInfo.m_blendState.m_attachmentCount;

			auto &blendDescDx = stateDesc.BlendState.RenderTarget[i];
			memset(&blendDescDx, 0, sizeof(blendDescDx));

			blendDescDx.BlendEnable = i < attachmentCount ? blendDesc.m_blendEnable : false;
			blendDescDx.LogicOpEnable = i < attachmentCount ? createInfo.m_blendState.m_logicOpEnable : false;
			blendDescDx.SrcBlend = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_srcColorBlendFactor) : D3D12_BLEND_ONE;
			blendDescDx.DestBlend = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_dstColorBlendFactor) : D3D12_BLEND_ZERO;
			blendDescDx.BlendOp = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_colorBlendOp) : D3D12_BLEND_OP_ADD;
			blendDescDx.SrcBlendAlpha = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_srcAlphaBlendFactor) : D3D12_BLEND_ONE;
			blendDescDx.DestBlendAlpha = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_dstAlphaBlendFactor) : D3D12_BLEND_ZERO;
			blendDescDx.BlendOpAlpha = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_alphaBlendOp) : D3D12_BLEND_OP_ADD;
			blendDescDx.LogicOp = i < attachmentCount ? UtilityDx12::translate(createInfo.m_blendState.m_logicOp) : D3D12_LOGIC_OP_NOOP;
			blendDescDx.RenderTargetWriteMask = i < attachmentCount ? static_cast<UINT8>(blendDesc.m_colorWriteMask) : D3D12_COLOR_WRITE_ENABLE_ALL;

			stateDesc.BlendState.IndependentBlendEnable = (i < attachmentCount &&i > 0 && memcmp(&stateDesc.BlendState.RenderTarget[i - 1], &blendDescDx, sizeof(blendDescDx)) != 0)
				? true : stateDesc.BlendState.IndependentBlendEnable;
		}
		memcpy(m_blendFactors, createInfo.m_blendState.m_blendConstants, sizeof(m_blendFactors));
	}

	stateDesc.SampleMask = createInfo.m_multiSampleState.m_sampleMask;

	// rasterizer state
	{
		stateDesc.RasterizerState.FillMode = createInfo.m_rasterizationState.m_polygonMode == PolygonMode::FILL ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
		stateDesc.RasterizerState.CullMode = createInfo.m_rasterizationState.m_cullMode == CullModeFlags::NONE
			? D3D12_CULL_MODE_NONE : createInfo.m_rasterizationState.m_cullMode == CullModeFlags::FRONT_BIT ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
		stateDesc.RasterizerState.FrontCounterClockwise = createInfo.m_rasterizationState.m_frontFace == FrontFace::COUNTER_CLOCKWISE;
		stateDesc.RasterizerState.DepthBias = createInfo.m_rasterizationState.m_depthBiasEnable ? static_cast<INT>(createInfo.m_rasterizationState.m_depthBiasConstantFactor) : 0;
		stateDesc.RasterizerState.DepthBiasClamp = createInfo.m_rasterizationState.m_depthBiasEnable ? createInfo.m_rasterizationState.m_depthBiasClamp : 0.0f;
		stateDesc.RasterizerState.SlopeScaledDepthBias = createInfo.m_rasterizationState.m_depthBiasEnable ? createInfo.m_rasterizationState.m_depthBiasSlopeFactor : 0.0f;
		stateDesc.RasterizerState.DepthClipEnable = createInfo.m_rasterizationState.m_depthClampEnable;
		stateDesc.RasterizerState.MultisampleEnable = createInfo.m_multiSampleState.m_rasterizationSamples != SampleCount::_1;
		stateDesc.RasterizerState.AntialiasedLineEnable = false;
		stateDesc.RasterizerState.ForcedSampleCount = 0;
		stateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		if (createInfo.m_attachmentFormats.m_colorAttachmentCount == 0
			&& createInfo.m_attachmentFormats.m_depthStencilFormat == Format::UNDEFINED
			&& !createInfo.m_depthStencilState.m_depthTestEnable)
		{
			stateDesc.RasterizerState.ForcedSampleCount = (UINT)createInfo.m_multiSampleState.m_rasterizationSamples;
		}
	}

	// depth stencil state
	{
		stateDesc.DepthStencilState.DepthEnable = createInfo.m_depthStencilState.m_depthTestEnable;
		stateDesc.DepthStencilState.DepthWriteMask = createInfo.m_depthStencilState.m_depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		stateDesc.DepthStencilState.DepthFunc = UtilityDx12::translate(createInfo.m_depthStencilState.m_depthCompareOp);
		stateDesc.DepthStencilState.StencilEnable = createInfo.m_depthStencilState.m_stencilTestEnable;
		stateDesc.DepthStencilState.StencilReadMask = static_cast<UINT8>(createInfo.m_depthStencilState.m_front.m_compareMask);
		stateDesc.DepthStencilState.StencilWriteMask = static_cast<UINT8>(createInfo.m_depthStencilState.m_front.m_writeMask);
		stateDesc.DepthStencilState.FrontFace.StencilFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_failOp);
		stateDesc.DepthStencilState.FrontFace.StencilDepthFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_depthFailOp);
		stateDesc.DepthStencilState.FrontFace.StencilPassOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_passOp);
		stateDesc.DepthStencilState.FrontFace.StencilFunc = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_compareOp);
		stateDesc.DepthStencilState.BackFace.StencilFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_failOp);
		stateDesc.DepthStencilState.BackFace.StencilDepthFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_depthFailOp);
		stateDesc.DepthStencilState.BackFace.StencilPassOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_passOp);
		stateDesc.DepthStencilState.BackFace.StencilFunc = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_compareOp);
	}

	m_stencilRef = createInfo.m_depthStencilState.m_front.m_reference;

	D3D12_INPUT_ELEMENT_DESC inputElements[VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS];
	for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount; ++i)
	{
		const auto &desc = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i];
		auto &descDx = inputElements[i];
		descDx = {};
		descDx.SemanticName = desc.m_semanticName;
		descDx.SemanticIndex = 0;
		descDx.Format = UtilityDx12::translate(desc.m_format);
		descDx.InputSlot = desc.m_binding;
		descDx.AlignedByteOffset = desc.m_offset;
		descDx.InputSlotClass = createInfo.m_vertexInputState.m_vertexBindingDescriptions[desc.m_binding].m_inputRate == VertexInputRate::VERTEX ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

		m_vertexBufferStrides[descDx.InputSlot] = createInfo.m_vertexInputState.m_vertexBindingDescriptions[desc.m_binding].m_stride;
	}

	stateDesc.InputLayout.NumElements = createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount;
	stateDesc.InputLayout.pInputElementDescs = stateDesc.InputLayout.NumElements > 0 ? inputElements : nullptr;

	stateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // TODO
	stateDesc.PrimitiveTopologyType = UtilityDx12::getTopologyType(createInfo.m_inputAssemblyState.m_primitiveTopology);
	m_primitiveTopology = UtilityDx12::translate(createInfo.m_inputAssemblyState.m_primitiveTopology, createInfo.m_tesselationState.m_patchControlPoints);


	stateDesc.NumRenderTargets = createInfo.m_attachmentFormats.m_colorAttachmentCount;
	for (size_t i = 0; i < 8; ++i)
	{
		stateDesc.RTVFormats[i] = UtilityDx12::translate(createInfo.m_attachmentFormats.m_colorAttachmentFormats[i]);
	}
	stateDesc.DSVFormat = UtilityDx12::translate(createInfo.m_attachmentFormats.m_depthStencilFormat);

	stateDesc.SampleDesc.Count = stateDesc.RasterizerState.ForcedSampleCount != 0 ? 1 : static_cast<UINT>(createInfo.m_multiSampleState.m_rasterizationSamples);
	stateDesc.SampleDesc.Quality = 0;// createInfo.m_multiSampleState.m_rasterizationSamples != SampleCount::_1 ? 1 : 0;

	stateDesc.NodeMask = 0;

	stateDesc.CachedPSO.pCachedBlob = nullptr;
	stateDesc.CachedPSO.CachedBlobSizeInBytes = 0;

	stateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	UtilityDx12::checkResult(device->CreateGraphicsPipelineState(&stateDesc, __uuidof(ID3D12PipelineState), (void **)&m_pipeline), "Failed to create pipeline!");

	delete[] vsCode;
	delete[] dsCode;
	delete[] hsCode;
	delete[] gsCode;
	delete[] psCode;
}

gal::GraphicsPipelineDx12::~GraphicsPipelineDx12()
{
	D3D12_SAFE_RELEASE(m_pipeline);
	D3D12_SAFE_RELEASE(m_rootSignature);
}

void *gal::GraphicsPipelineDx12::getNativeHandle() const
{
	return m_pipeline;
}

uint32_t gal::GraphicsPipelineDx12::getDescriptorSetLayoutCount() const
{
	return uint32_t();
}

const DescriptorSetLayout *gal::GraphicsPipelineDx12::getDescriptorSetLayout(uint32_t index) const
{
	return nullptr;
}

ID3D12RootSignature *gal::GraphicsPipelineDx12::getRootSignature() const
{
	return m_rootSignature;
}

uint32_t gal::GraphicsPipelineDx12::getDescriptorTableOffset() const
{
	return m_descriptorTableOffset;
}

uint32_t gal::GraphicsPipelineDx12::getVertexBufferStride(uint32_t bufferBinding) const
{
	assert(bufferBinding < 32);
	return m_vertexBufferStrides[bufferBinding];
}

void gal::GraphicsPipelineDx12::initializeState(ID3D12GraphicsCommandList *cmdList) const
{
	cmdList->IASetPrimitiveTopology(m_primitiveTopology);
	cmdList->OMSetBlendFactor(m_blendFactors);
	cmdList->OMSetStencilRef(m_stencilRef);
}

void gal::GraphicsPipelineDx12::getRootDescriptorInfo(uint32_t &setIndex, uint32_t &descriptorCount) const
{
	setIndex = m_rootDescriptorSetIndex;
	descriptorCount = m_rootDescriptorCount;
}

gal::ComputePipelineDx12::ComputePipelineDx12(ID3D12Device *device, const ComputePipelineCreateInfo &createInfo)
	:m_pipeline(),
	m_rootSignature(),
	m_descriptorTableOffset()
{
	size_t csCodeSize = 0;
	char *csCode = loadShaderFile(createInfo.m_computeShader.m_path, &csCodeSize);

	// create root signature
	m_rootSignature = createRootSignature(device, false, m_rootDescriptorSetIndex, m_rootDescriptorCount, m_descriptorTableOffset, createInfo.m_layoutCreateInfo);

	D3D12_COMPUTE_PIPELINE_STATE_DESC stateDesc{};
	stateDesc.pRootSignature = m_rootSignature;
	stateDesc.CS.pShaderBytecode = csCode;
	stateDesc.CS.BytecodeLength = csCodeSize;
	stateDesc.NodeMask = 0;
	stateDesc.CachedPSO.pCachedBlob = nullptr;
	stateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	stateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	UtilityDx12::checkResult(device->CreateComputePipelineState(&stateDesc, __uuidof(ID3D12PipelineState), (void **)&m_pipeline), "Failed to create pipeline!");

	delete[] csCode;
}

gal::ComputePipelineDx12::~ComputePipelineDx12()
{
	D3D12_SAFE_RELEASE(m_pipeline);
	D3D12_SAFE_RELEASE(m_rootSignature);
}

void *gal::ComputePipelineDx12::getNativeHandle() const
{
	return m_pipeline;
}

uint32_t gal::ComputePipelineDx12::getDescriptorSetLayoutCount() const
{
	return uint32_t();
}

const DescriptorSetLayout *gal::ComputePipelineDx12::getDescriptorSetLayout(uint32_t index) const
{
	return nullptr;
}

ID3D12RootSignature *gal::ComputePipelineDx12::getRootSignature() const
{
	return m_rootSignature;
}

uint32_t gal::ComputePipelineDx12::getDescriptorTableOffset() const
{
	return m_descriptorTableOffset;
}

void gal::ComputePipelineDx12::getRootDescriptorInfo(uint32_t &setIndex, uint32_t &descriptorCount) const
{
	setIndex = m_rootDescriptorSetIndex;
	descriptorCount = m_rootDescriptorCount;
}

static char *loadShaderFile(const char *filename, size_t *shaderFileSize)
{
	char path[ShaderStageCreateInfo::MAX_PATH_LENGTH + 5];
	strcpy_s(path, filename);
	strcat_s(path, ".cso");
	return util::readBinaryFile(path, shaderFileSize);
}


static ID3D12RootSignature *createRootSignature(ID3D12Device *device,
	bool useIA,
	uint32_t &rootDescriptorSetIndex,
	uint32_t &rootDescriptorCount,
	uint32_t &descriptorTableOffset,
	const PipelineLayoutCreateInfo &layoutCreateInfo)
{
	rootDescriptorSetIndex = ~uint32_t(0);
	rootDescriptorCount = 0;
	descriptorTableOffset = 0;

	// process descriptor set layouts
	bool rootDescriptorSets[4] = {};
	size_t totalDescriptorRangeCount = 0;
	for (size_t i = 0; i < layoutCreateInfo.m_descriptorSetLayoutCount; ++i)
	{
		const auto &setDecl = layoutCreateInfo.m_descriptorSetLayoutDeclarations[i];

		bool hasSamplers = false;
		bool hasNonSamplers = false;
		bool hasRootDescriptors = false;
		bool hasNonRootDescriptors = false;

		for (size_t j = 0; j < setDecl.m_usedBindingCount; ++j)
		{
			auto type = setDecl.m_usedBindings[j].m_descriptorType;
			rootDescriptorSets[i] = rootDescriptorSets[i] || (type == DescriptorType::OFFSET_CONSTANT_BUFFER);
			hasSamplers = hasSamplers || type == DescriptorType::SAMPLER;
			hasNonSamplers = hasNonSamplers || type != DescriptorType::SAMPLER;
			hasRootDescriptors = hasRootDescriptors || type == DescriptorType::OFFSET_CONSTANT_BUFFER;
			hasNonRootDescriptors = hasNonRootDescriptors || type != DescriptorType::OFFSET_CONSTANT_BUFFER;
		}

		if (hasSamplers && hasNonSamplers)
		{
			util::fatalExit("Tried to create descriptor set layout with both sampler and non-sampler descriptors!", EXIT_FAILURE);
		}
		if (hasRootDescriptors && hasNonRootDescriptors)
		{
			util::fatalExit("Tried to create descriptor set layout with both root descriptors and non-root descriptors!", EXIT_FAILURE);
		}

		if (!rootDescriptorSets[i])
		{
			totalDescriptorRangeCount += setDecl.m_usedBindingCount;
		}
	}

	// check for root signature 1.1 support
	D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSigFeatureData{};
	rootSigFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSigFeatureData, sizeof(rootSigFeatureData))))
	{
		rootSigFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	UINT rootParamCount = 0;
	constexpr size_t rootParamsWorstCaseCount = 5 + 32; // 1 root constant and 4 descriptor tables maximum and 32 root descriptors maximum
	D3D12_ROOT_PARAMETER1 rootParams1_1[rootParamsWorstCaseCount];
	D3D12_ROOT_PARAMETER rootParams1_0[rootParamsWorstCaseCount];
	D3D12_DESCRIPTOR_RANGE1 *descriptorRanges1_1 = ALLOC_A_T(D3D12_DESCRIPTOR_RANGE1, totalDescriptorRangeCount);
	D3D12_DESCRIPTOR_RANGE *descriptorRanges1_0 = nullptr;
	size_t descriptorRangesCount = 0;
	D3D12_STATIC_SAMPLER_DESC *staticSamplerDescs = ALLOC_A_T(D3D12_STATIC_SAMPLER_DESC, layoutCreateInfo.m_staticSamplerCount);

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSig{ rootSigFeatureData.HighestVersion };

	// fill root signature with params
	{
		auto determineShaderVisibility = [](ShaderStageFlags stageFlags) ->D3D12_SHADER_VISIBILITY
		{
			switch (stageFlags)
			{
			case ShaderStageFlags::VERTEX_BIT:
				return D3D12_SHADER_VISIBILITY_VERTEX;
			case ShaderStageFlags::HULL_BIT:
				return D3D12_SHADER_VISIBILITY_HULL;
			case ShaderStageFlags::DOMAIN_BIT:
				return D3D12_SHADER_VISIBILITY_DOMAIN;
			case ShaderStageFlags::GEOMETRY_BIT:
				return D3D12_SHADER_VISIBILITY_GEOMETRY;
			case ShaderStageFlags::PIXEL_BIT:
				return D3D12_SHADER_VISIBILITY_PIXEL;
			case ShaderStageFlags::COMPUTE_BIT:
				return D3D12_SHADER_VISIBILITY_ALL;
			default:
				return D3D12_SHADER_VISIBILITY_ALL;
			}
		};

		// keep track of all stages requiring access to resources. we might be able to use DENY flags for some stages
		ShaderStageFlags mergedStageMask = (ShaderStageFlags)0;

		// root consts
		if (layoutCreateInfo.m_pushConstRange)
		{
			assert(layoutCreateInfo.m_pushConstRange % 4u == 0);

			auto &param = rootParams1_1[rootParamCount++];
			param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			param.Constants.ShaderRegister = s_rootConstRegister;
			param.Constants.RegisterSpace = s_rootConstSpace;
			param.Constants.Num32BitValues = layoutCreateInfo.m_pushConstRange / 4u;
			param.ShaderVisibility = determineShaderVisibility(layoutCreateInfo.m_pushConstStageFlags);

			mergedStageMask |= layoutCreateInfo.m_pushConstStageFlags;

			++descriptorTableOffset;
		}

		// root descriptors
		for (uint32_t i = 0; i < layoutCreateInfo.m_descriptorSetLayoutCount; ++i)
		{
			if (rootDescriptorSets[i])
			{
				// we currently only support a single set root descriptor set per pipeline
				if (rootDescriptorSetIndex != ~uint32_t(0))
				{
					util::fatalExit("Tried to create a pipeline with multiple root descriptor (offset buffer) sets!", EXIT_FAILURE);
					break;
				}

				descriptorTableOffset;
				rootDescriptorSetIndex = i;

				rootDescriptorCount = layoutCreateInfo.m_descriptorSetLayoutDeclarations[i].m_usedBindingCount;
				descriptorTableOffset += rootDescriptorCount;
				const auto *bindings = layoutCreateInfo.m_descriptorSetLayoutDeclarations[i].m_usedBindings;

				// iterate over all bindings
				for (uint32_t j = 0; j < rootDescriptorCount; ++j)
				{
					auto &param = rootParams1_1[rootParamCount++];
					param = {};
					param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
					param.Descriptor.ShaderRegister = bindings[j].m_binding;
					param.Descriptor.RegisterSpace = bindings[j].m_space;
					param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
					param.ShaderVisibility = determineShaderVisibility(bindings[j].m_stageFlags);

					mergedStageMask |= bindings[j].m_stageFlags;
				}
			}
		}

		// descriptor tables
		for (uint32_t i = 0; i < layoutCreateInfo.m_descriptorSetLayoutCount; ++i)
		{
			if (!rootDescriptorSets[i])
			{
				const size_t rangeOffset = descriptorRangesCount;
				ShaderStageFlags tableStageMask = (ShaderStageFlags)0;

				const uint32_t bindingCount = layoutCreateInfo.m_descriptorSetLayoutDeclarations[i].m_usedBindingCount;
				const auto *bindings = layoutCreateInfo.m_descriptorSetLayoutDeclarations[i].m_usedBindings;

				// iterate over all bindings
				for (uint32_t j = 0; j < bindingCount; ++j)
				{
					D3D12_DESCRIPTOR_RANGE1 range{};
					range.NumDescriptors = bindings[j].m_descriptorCount;
					range.BaseShaderRegister = bindings[j].m_binding;
					range.RegisterSpace = bindings[j].m_space;
					range.OffsetInDescriptorsFromTableStart = range.BaseShaderRegister;

					switch (bindings[j].m_descriptorType)
					{
					case DescriptorType::SAMPLER:
					{
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
						break;
					}
					case DescriptorType::TEXTURE:
					case DescriptorType::TYPED_BUFFER:
					case DescriptorType::BYTE_BUFFER:
					case DescriptorType::STRUCTURED_BUFFER:
					{
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
						break;
					}
					case DescriptorType::RW_TEXTURE:
					case DescriptorType::RW_TYPED_BUFFER:
					case DescriptorType::RW_BYTE_BUFFER:
					case DescriptorType::RW_STRUCTURED_BUFFER:
					{
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
						break;
					}
					case DescriptorType::CONSTANT_BUFFER:
					{
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
						break;
					}
					default:
						assert(false);
						break;
					}

					if ((bindings[j].m_bindingFlags & (gal::DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | gal::DescriptorBindingFlags::UPDATE_UNUSED_WHILE_PENDING_BIT | gal::DescriptorBindingFlags::PARTIALLY_BOUND_BIT)) != 0)
					{
						range.Flags |= D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
					}

					assert(descriptorRangesCount < totalDescriptorRangeCount);
					descriptorRanges1_1[descriptorRangesCount++] = range;
					tableStageMask |= bindings[j].m_stageFlags;
				}

				auto &param = rootParams1_1[rootParamCount++];
				param = {};
				param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorRangesCount - rangeOffset);
				param.DescriptorTable.pDescriptorRanges = descriptorRanges1_1 + rangeOffset;
				param.ShaderVisibility = determineShaderVisibility(tableStageMask);

				mergedStageMask |= tableStageMask;
			}
		}

		// static samplers
		for (size_t i = 0; i < layoutCreateInfo.m_staticSamplerCount; ++i)
		{
			D3D12_STATIC_BORDER_COLOR borderColors[]
			{
				D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
				D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
				D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
				D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
				D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
				D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
			};

			const auto &samplerDesc = layoutCreateInfo.m_staticSamplerDescriptions[i];

			D3D12_STATIC_SAMPLER_DESC samplerDescDx{};
			samplerDescDx.Filter = UtilityDx12::translate(samplerDesc.m_magFilter, samplerDesc.m_minFilter, samplerDesc.m_mipmapMode, samplerDesc.m_compareEnable, samplerDesc.m_anisotropyEnable);
			samplerDescDx.AddressU = UtilityDx12::translate(samplerDesc.m_addressModeU);
			samplerDescDx.AddressV = UtilityDx12::translate(samplerDesc.m_addressModeV);
			samplerDescDx.AddressW = UtilityDx12::translate(samplerDesc.m_addressModeW);
			samplerDescDx.MipLODBias = samplerDesc.m_mipLodBias;
			samplerDescDx.MaxAnisotropy = (UINT)samplerDesc.m_maxAnisotropy;
			samplerDescDx.ComparisonFunc = UtilityDx12::translate(samplerDesc.m_compareOp);
			samplerDescDx.BorderColor = borderColors[static_cast<size_t>(samplerDesc.m_borderColor)];
			samplerDescDx.MinLOD = samplerDesc.m_minLod;
			samplerDescDx.MaxLOD = samplerDesc.m_maxLod;
			samplerDescDx.ShaderRegister = samplerDesc.m_binding;
			samplerDescDx.RegisterSpace = samplerDesc.m_space;
			samplerDescDx.ShaderVisibility = UtilityDx12::translate(samplerDesc.m_stageFlags);

			staticSamplerDescs[i] = samplerDescDx;
		}

		D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		// try to add DENY flags
		if ((mergedStageMask & ShaderStageFlags::COMPUTE_BIT) == 0)
		{
			if ((mergedStageMask & ShaderStageFlags::VERTEX_BIT) == 0)
			{
				rootSigFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ShaderStageFlags::HULL_BIT) == 0)
			{
				rootSigFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ShaderStageFlags::DOMAIN_BIT) == 0)
			{
				rootSigFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ShaderStageFlags::GEOMETRY_BIT) == 0)
			{
				rootSigFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ShaderStageFlags::PIXEL_BIT) == 0)
			{
				rootSigFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
			}
		}

		// fallback for version 1.0: translate all data to their 1.0 version
		if (rootSig.Version == D3D_ROOT_SIGNATURE_VERSION_1_0)
		{
			// translate ranges
			descriptorRanges1_0 = ALLOC_A_T(D3D12_DESCRIPTOR_RANGE, descriptorRangesCount);
			for (size_t i = 0; i < descriptorRangesCount; ++i)
			{
				const auto &range1_1 = descriptorRanges1_1[i];

				D3D12_DESCRIPTOR_RANGE range1_0{};
				range1_0.RangeType = range1_1.RangeType;
				range1_0.NumDescriptors = range1_1.NumDescriptors;
				range1_0.BaseShaderRegister = range1_1.BaseShaderRegister;
				range1_0.RegisterSpace = range1_1.RegisterSpace;
				range1_0.OffsetInDescriptorsFromTableStart = range1_1.OffsetInDescriptorsFromTableStart;

				descriptorRanges1_0[i] = range1_0;
			}

			// translate params
			for (size_t i = 0; i < rootParamCount; ++i)
			{
				const auto &param1_1 = rootParams1_1[i];
				auto &param1_0 = rootParams1_0[i];
				param1_0 = {};
				param1_0.ParameterType = param1_1.ParameterType;
				param1_0.ShaderVisibility = param1_1.ShaderVisibility;

				switch (param1_1.ParameterType)
				{
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					param1_0.DescriptorTable.NumDescriptorRanges = param1_1.DescriptorTable.NumDescriptorRanges;
					param1_0.DescriptorTable.pDescriptorRanges = descriptorRanges1_0 + (param1_1.DescriptorTable.pDescriptorRanges - descriptorRanges1_1);
					break;
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
					param1_0.Constants = param1_1.Constants;
					break;
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
					param1_0.Descriptor.ShaderRegister = param1_1.Descriptor.ShaderRegister;
					param1_0.Descriptor.RegisterSpace = param1_1.Descriptor.RegisterSpace;
					break;
				default:
					assert(false);
					break;
				}
			}

			rootSig.Desc_1_0.NumParameters = rootParamCount;
			rootSig.Desc_1_0.pParameters = rootParams1_0;
			rootSig.Desc_1_0.NumStaticSamplers = (UINT)layoutCreateInfo.m_staticSamplerCount;
			rootSig.Desc_1_0.pStaticSamplers = staticSamplerDescs;
			rootSig.Desc_1_0.Flags = useIA ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;
			rootSig.Desc_1_0.Flags |= rootSigFlags;
		}
		else
		{
			rootSig.Desc_1_1.NumParameters = rootParamCount;
			rootSig.Desc_1_1.pParameters = rootParams1_1;
			rootSig.Desc_1_1.NumStaticSamplers = (UINT)layoutCreateInfo.m_staticSamplerCount;
			rootSig.Desc_1_1.pStaticSamplers = staticSamplerDescs;
			rootSig.Desc_1_1.Flags = useIA ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;
			rootSig.Desc_1_1.Flags |= rootSigFlags;
		}
	}

	{
		// create root signature
		ID3DBlob *serializedRootSig;
		ID3DBlob *errorBlob;
		if (!SUCCEEDED(UtilityDx12::checkResult(D3D12SerializeVersionedRootSignature(&rootSig, &serializedRootSig, &errorBlob), "Failed to serialize root signature", false)))
		{
			printf((char *)errorBlob->GetBufferPointer());
		}

		ID3D12RootSignature *rootSignature;
		UtilityDx12::checkResult(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&rootSignature));

		return rootSignature;
	}
}