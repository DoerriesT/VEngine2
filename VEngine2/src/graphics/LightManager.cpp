#include "LightManager.h"
#include <glm/mat4x4.hpp>
#include <glm/packing.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <EASTL/sort.h>
#include <EASTL/fixed_vector.h>
#include "gal/Initializers.h"
#include "component/LightComponent.h"
#include "RenderWorld.h"
#include "RenderData.h"
#include "RenderGraph.h"
#include "Mesh.h"
#include "CommonViewData.h"
#include "RendererResources.h"
#include "ResourceViewRegistry.h"
#include "BufferStackAllocator.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "utility/Utility.h"


using namespace gal;

static constexpr size_t k_numDepthBins = 8192;
static constexpr float k_depthBinRange = 1.0f; // each bin covers a depth range of 1 unit
static constexpr uint32_t k_emptyBin = ((~0u & 0xFFFFu) << 16u);
static constexpr uint32_t k_lightingTileSize = 8;

static float calculateMaxPenumbraPercentage(float bulbRadius = 0.1f) noexcept
{
	constexpr float k_minPenumbra = 0.5f / 256.0f;
	constexpr float k_minBulbRadius = 0.1f;
	const float k_minBulbRadiusSqrt = sqrtf(k_minBulbRadius);

	float clampedBulbRadius = fmaxf(bulbRadius, k_minBulbRadius);
	float bulbPenumbraScale = sqrtf(clampedBulbRadius) / k_minBulbRadiusSqrt;
	float maxPenumbraPercentage = bulbPenumbraScale * k_minPenumbra;

	return maxPenumbraPercentage;
}

static float calculatePenumbraRadiusNDC(float maxPenumbraPercentage, float shadowHalfFov) noexcept
{
	constexpr float k_minPenumbra = 0.5f / 256.0f;
	float fovScale = tanf(shadowHalfFov);
	float clampedPenumbraPercentage = fmaxf(maxPenumbraPercentage, k_minPenumbra);

	float penumbraRadiusNDC = (clampedPenumbraPercentage / fovScale);
	return penumbraRadiusNDC;
}


static uint64_t computePunctualLightSortValue(const RenderWorld::PunctualLight &light, const glm::vec4 &viewMatDepthRow, size_t index) noexcept
{
	uint32_t depthUI = util::asuint(-glm::dot(glm::vec4(light.m_position, 1.0f), viewMatDepthRow));

	uint64_t packedDepthAndIndex = 0;
	packedDepthAndIndex |= static_cast<uint64_t>(index) & 0xFFFFFFFF; // index
	// floats interpreted as integers keep the less-than ordering, so we can just put the depth
	// in the most significant bits and use that to sort by depth
	packedDepthAndIndex |= static_cast<uint64_t>(depthUI) << 32ull;

	return packedDepthAndIndex;
}

static PunctualLightGPU createPunctualLightGPU(const RenderWorld::PunctualLight &light) noexcept
{
	float intensity = light.m_spotLight ? (1.0f / (glm::pi<float>())) : (1.0f / (4.0f * glm::pi<float>()));
	intensity *= light.m_intensity;
	PunctualLightGPU punctualLight{};
	punctualLight.m_color = glm::vec3(glm::unpackUnorm4x8(light.m_color)) * intensity;
	punctualLight.m_invSqrAttRadius = 1.0f / (light.m_radius * light.m_radius);
	punctualLight.m_position = light.m_position;
	if (light.m_spotLight)
	{
		punctualLight.m_angleScale = 1.0f / fmaxf(0.001f, cosf(light.m_innerAngle * 0.5f) - cosf(light.m_outerAngle * 0.5f));
		punctualLight.m_angleOffset = -cosf(light.m_outerAngle * 0.5f) * punctualLight.m_angleScale;
		punctualLight.m_direction = light.m_direction;
	}
	else
	{
		punctualLight.m_angleScale = -1.0f; // special value to mark this as a point light
	}

	return punctualLight;
}

static void insertPunctualLightIntoDepthBins(const RenderWorld::PunctualLight &light, size_t lightIndex, uint64_t sortIndex, uint32_t *depthBins) noexcept
{
	float lightDepthVS = -util::asfloat(static_cast<uint32_t>(sortIndex >> 32ull));
	float nearestPoint = -lightDepthVS - light.m_radius;
	float farthestPoint = -lightDepthVS + light.m_radius;

	size_t minBin = eastl::min<size_t>(static_cast<size_t>(fmaxf(nearestPoint / k_depthBinRange, 0.0f)), size_t(k_numDepthBins - 1));
	size_t maxBin = eastl::min<size_t>(static_cast<size_t>(fmaxf(farthestPoint / k_depthBinRange, 0.0f)), size_t(k_numDepthBins - 1));

	for (size_t j = minBin; j <= maxBin; ++j)
	{
		uint32_t &val = depthBins[j];
		uint32_t minIndex = (val & 0xFFFF0000) >> 16;
		uint32_t maxIndex = val & 0xFFFF;
		minIndex = eastl::min<uint32_t>(minIndex, static_cast<uint32_t>(lightIndex));
		maxIndex = eastl::max<uint32_t>(maxIndex, static_cast<uint32_t>(lightIndex));
		val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
	}
}

static void calculateCascadeViewProjectionMatrices(const glm::vec3 &lightDir,
	float nearPlane,
	float farPlane,
	float fovy,
	const glm::mat4 &invViewMatrix,
	uint32_t width,
	uint32_t height,
	float maxShadowDistance,
	float splitLambda,
	float shadowTextureSize,
	size_t cascadeCount,
	glm::mat4 *viewProjectionMatrices,
	glm::vec4 *cascadeParams,
	glm::vec4 *viewMatrixDepthRows) noexcept
{
	float splits[LightComponent::k_maxCascades];

	assert(cascadeCount <= LightComponent::k_maxCascades);

	// compute split distances
	farPlane = fminf(farPlane, maxShadowDistance);
	const float range = farPlane - nearPlane;
	const float ratio = farPlane / nearPlane;
	const float invCascadeCount = 1.0f / cascadeCount;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		float p = (i + 1) * invCascadeCount;
		float log = nearPlane * std::pow(ratio, p);
		float uniform = nearPlane + range * p;
		splits[i] = splitLambda * (log - uniform) + uniform;
	}

	const float aspectRatio = width * (1.0f / height);
	float previousSplit = nearPlane;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
		const float n = previousSplit;
		const float f = splits[i];
		const float k = sqrtf(1.0f + aspectRatio * aspectRatio) * tanf(fovy * 0.5f);

		glm::vec3 center;
		float radius;
		const float k2 = k * k;
		const float fnSum = f + n;
		const float fnDiff = f - n;
		const float fnSum2 = fnSum * fnSum;
		const float fnDiff2 = fnDiff * fnDiff;
		if (k2 >= fnDiff / fnSum)
		{
			center = glm::vec3(0.0f, 0.0f, -f);
			radius = f * k;
		}
		else
		{
			center = glm::vec3(0.0f, 0.0f, -0.5f * fnSum * (1.0f + k2));
			radius = 0.5f * sqrtf(fnDiff2 + 2 * (f * f + n * n) * k2 + fnSum2 * k2 * k2);
		}
		glm::vec4 tmp = invViewMatrix * glm::vec4(center, 1.0f);
		center = glm::vec3(tmp / tmp.w);

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (fabsf(lightDir.x) < 0.001f && fabsf(lightDir.z) < 0.001f)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		glm::mat4 lightView = glm::lookAt(center + lightDir * 150.0f, center, upDir);

		constexpr float depthRange = 300.0f;
		constexpr float precisionRange = 65536.0f;

		// snap to shadow map texel to avoid shimmering
		lightView[3].x -= fmodf(lightView[3].x, (radius / 256.0f) * 2.0f);
		lightView[3].y -= fmodf(lightView[3].y, (radius / 256.0f) * 2.0f);
		lightView[3].z -= fmodf(lightView[3].z, depthRange / precisionRange);

		viewMatrixDepthRows[i] = glm::vec4(lightView[0][2], lightView[1][2], lightView[2][2], lightView[3][2]);

		viewProjectionMatrices[i] = glm::ortho(-radius, radius, -radius, radius, depthRange, 0.1f) * lightView;

		// depthNormalBiases[i] holds the depth/normal offset biases in texel units
		const float unitsPerTexel = radius * 2.0f / shadowTextureSize;
		cascadeParams[i].x = unitsPerTexel * -cascadeParams[i].x / depthRange;
		cascadeParams[i].y = unitsPerTexel * cascadeParams[i].y;
		cascadeParams[i].z = 1.0f / unitsPerTexel;

		previousSplit = splits[i];
	}
}

static uint32_t calculateProjectedLightSize(const CommonViewData &viewData, const glm::mat4 &shadowMatrix) noexcept
{
	glm::mat4 shadowToScreenSpace = viewData.m_viewProjectionMatrix * glm::inverse(shadowMatrix);

	float maxX = -FLT_MAX;
	float maxY = -FLT_MAX;
	float minX = FLT_MAX;
	float minY = FLT_MAX;

	glm::vec4 positions[]
	{
		glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, -1.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
	};

	for (auto &pos : positions)
	{
		glm::vec4 ssPos = shadowToScreenSpace * pos;
		float x = (ssPos.x / ssPos.w) * 0.5f + 0.5f;
		float y = (ssPos.y / ssPos.w) * 0.5f + 0.5f;

		maxX = fmaxf(maxX, x);
		maxY = fmaxf(maxY, y);
		minX = fminf(minX, x);
		minY = fminf(minY, y);
	}

	float width = (maxX - minX) * viewData.m_width;
	float height = (maxY - minY) * viewData.m_height;
	float maxDim = eastl::clamp(fmaxf(width, height), 16.0f, 2048.0f);

	return util::alignUp<uint32_t>(static_cast<uint32_t>(ceilf(maxDim)), 16);
}

LightManager::LightManager(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept
	:m_device(device)
{
	// light tile assignment
	{
		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("assets/shaders/lightTileAssignment_vs");
		builder.setFragmentShader("assets/shaders/lightTileAssignment_ps");
		builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
		builder.setVertexAttributeDescription({ "POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 });
		builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlags::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
		builder.setMultisampleState(SampleCount::_4, false, 0.0f, 0xFFFFFFFF, false, false);
		builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);

		DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::ALL_STAGES };
		DescriptorSetLayoutBinding usedBindlessBindings[] =
		{
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 0, ShaderStageFlags::VERTEX_BIT), // transforms
			Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::RW_TEXTURE, 1, ShaderStageFlags::PIXEL_BIT), // result textures
		};

		DescriptorSetLayoutDeclaration layoutDecls[]
		{
			{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
			{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
		};

		uint32_t pushConstSize = sizeof(uint32_t) * 2;
		builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, nullptr, -1);

		device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_lightTileAssignmentPipeline);
	}

	// shadows
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
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(uint32_t) * 2, VertexInputRate::VERTEX });
				++curVertexBufferIndex;

				attributeDescs.push_back({ "JOINT_WEIGHTS", curVertexBufferIndex, curVertexBufferIndex, Format::R8G8B8A8_UNORM, 0 });
				bindingDescs.push_back({ curVertexBufferIndex, sizeof(uint32_t), VertexInputRate::VERTEX });
				++curVertexBufferIndex;
			}

			GraphicsPipelineCreateInfo pipelineCreateInfo{};
			GraphicsPipelineBuilder builder(pipelineCreateInfo);

			if (isAlphaTested)
			{
				builder.setVertexShader(isSkinned ? "assets/shaders/shadow_skinned_alpha_tested_vs" : "assets/shaders/shadow_alpha_tested_vs");
				builder.setFragmentShader("assets/shaders/shadow_ps");
			}
			else
			{
				builder.setVertexShader(isSkinned ? "assets/shaders/shadow_skinned_vs" : "assets/shaders/shadow_vs");
			}

			builder.setVertexBindingDescriptions(static_cast<uint32_t>(bindingDescs.size()), bindingDescs.data());
			builder.setVertexAttributeDescriptions(static_cast<uint32_t>(attributeDescs.size()), attributeDescs.data());
			builder.setDepthTest(true, true, CompareOp::GREATER);
			builder.setDynamicState(DynamicStateFlags::VIEWPORT_BIT | DynamicStateFlags::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(Format::D16_UNORM);
			builder.setPolygonModeCullMode(gal::PolygonMode::FILL, isAlphaTested ? gal::CullModeFlags::NONE : gal::CullModeFlags::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);

			DescriptorSetLayoutBinding usedOffsetBufferBinding = { DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, ShaderStageFlags::VERTEX_BIT };
			DescriptorSetLayoutBinding usedBindlessBindings[] =
			{
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::TEXTURE, 0, ShaderStageFlags::PIXEL_BIT), // textures
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 1, ShaderStageFlags::VERTEX_BIT), // model/skinning matrices
				Initializers::bindlessDescriptorSetLayoutBinding(DescriptorType::STRUCTURED_BUFFER, 2, ShaderStageFlags::VERTEX_BIT), // material buffer
			};

			DescriptorSetLayoutDeclaration layoutDecls[]
			{
				{ offsetBufferSetLayout, 1, &usedOffsetBufferBinding },
				{ bindlessSetLayout, (uint32_t)eastl::size(usedBindlessBindings), usedBindlessBindings },
			};

			gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticAnisotropicRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT);

			uint32_t pushConstSize = sizeof(uint32_t);
			pushConstSize += isSkinned ? sizeof(uint32_t) : 0;
			pushConstSize += isAlphaTested ? sizeof(uint32_t) : 0;
			builder.setPipelineLayoutDescription(2, layoutDecls, pushConstSize, ShaderStageFlags::VERTEX_BIT, 1, &staticSamplerDesc, 2);

			GraphicsPipeline *pipeline = nullptr;
			device->createGraphicsPipelines(1, &pipelineCreateInfo, &pipeline);
			if (isSkinned)
			{
				if (isAlphaTested)
				{
					m_shadowSkinnedAlphaTestedPipeline = pipeline;
				}
				else
				{
					m_shadowSkinnedPipeline = pipeline;
				}
			}
			else
			{
				if (isAlphaTested)
				{
					m_shadowAlphaTestedPipeline = pipeline;
				}
				else
				{
					m_shadowPipeline = pipeline;
				}
			}
		}
	}
}

LightManager::~LightManager()
{
	m_device->destroyGraphicsPipeline(m_lightTileAssignmentPipeline);
	m_device->destroyGraphicsPipeline(m_shadowPipeline);
	m_device->destroyGraphicsPipeline(m_shadowSkinnedPipeline);
	m_device->destroyGraphicsPipeline(m_shadowAlphaTestedPipeline);
	m_device->destroyGraphicsPipeline(m_shadowSkinnedAlphaTestedPipeline);
}

void LightManager::update(const CommonViewData &viewData, const RenderWorld &renderWorld, rg::RenderGraph *graph) noexcept
{
	m_shadowMatrices.clear();
	m_shadowTextureRenderHandles.clear();
	m_shadowTextureSampleHandles.clear();
	m_directionalLights.clear();
	m_shadowedDirectionalLights.clear();
	m_punctualLights.clear();
	m_punctualLightsShadowed.clear();
	m_lightTransforms.clear();

	// process directional lights
	for (const auto &dirLight : renderWorld.m_directionalLights)
	{
		DirectionalLightGPU directionalLight{};
		directionalLight.m_color = glm::vec3(glm::unpackUnorm4x8(dirLight.m_color)) * dirLight.m_intensity;
		directionalLight.m_direction = dirLight.m_direction;
		directionalLight.m_cascadeCount = dirLight.m_shadowsEnabled ? dirLight.m_cascadeCount : 0;

		m_directionalLights.push_back(directionalLight);
	}

	// process shadowed directional lights
	for (const auto &dirLight : renderWorld.m_directionalLightsShadowed)
	{
		DirectionalLightGPU directionalLight{};
		directionalLight.m_color = glm::vec3(glm::unpackUnorm4x8(dirLight.m_color)) * dirLight.m_intensity;
		directionalLight.m_direction = dirLight.m_direction;
		directionalLight.m_cascadeCount = dirLight.m_shadowsEnabled ? dirLight.m_cascadeCount : 0;

		glm::vec4 cascadeParams[LightComponent::k_maxCascades];
		glm::vec4 depthRows[LightComponent::k_maxCascades];

		assert(dirLight.m_cascadeCount <= LightComponent::k_maxCascades);

		for (size_t j = 0; j < dirLight.m_cascadeCount; ++j)
		{
			cascadeParams[j].x = dirLight.m_depthBias[j];
			cascadeParams[j].y = dirLight.m_normalOffsetBias[j];
		}

		calculateCascadeViewProjectionMatrices(directionalLight.m_direction,
			viewData.m_near,
			viewData.m_far,
			viewData.m_fovy,
			viewData.m_invViewMatrix,
			viewData.m_width,
			viewData.m_height,
			dirLight.m_maxShadowDistance,
			dirLight.m_splitLambda,
			2048.0f,
			dirLight.m_cascadeCount,
			directionalLight.m_shadowMatrices,
			cascadeParams,
			depthRows);

		rg::ResourceHandle shadowMapHandle = graph->createImage(rg::ImageDesc::create("Cascaded Shadow Maps", Format::D16_UNORM, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, 2048, 2048, 1, dirLight.m_cascadeCount));
		rg::ResourceViewHandle shadowMapViewHandle = graph->createImageView(rg::ImageViewDesc::create("Cascaded Shadow Maps", shadowMapHandle, { 0, 1, 0, dirLight.m_cascadeCount }, ImageViewType::_2D_ARRAY));
		static_assert(sizeof(TextureViewHandle) == sizeof(shadowMapViewHandle));
		directionalLight.m_shadowTextureHandle = static_cast<TextureViewHandle>(shadowMapViewHandle); // we later correct these to be actual TextureViewHandles
		m_shadowTextureSampleHandles.push_back(shadowMapViewHandle);

		for (size_t j = 0; j < dirLight.m_cascadeCount; ++j)
		{
			m_shadowMatrices.push_back(directionalLight.m_shadowMatrices[j]);
			m_shadowTextureRenderHandles.push_back(graph->createImageView(rg::ImageViewDesc::create("Shadow Cascade", shadowMapHandle, { 0, 1, static_cast<uint32_t>(j), 1 }, ImageViewType::_2D)));
		}

		m_shadowedDirectionalLights.push_back(directionalLight);
	}

	// process punctual lights
	{
		eastl::vector<uint64_t> punctualLightsOrder;
		for (size_t i = 0; i < renderWorld.m_punctualLights.size(); ++i)
		{
			// TODO: cull lights here
			punctualLightsOrder.push_back(computePunctualLightSortValue(renderWorld.m_punctualLights[i], viewData.m_viewMatrixDepthRow, i));
		}

		// sort by distance to camera
		eastl::sort(punctualLightsOrder.begin(), punctualLightsOrder.end());

		// clear depth bins
		m_punctualLightsDepthBins.resize(k_numDepthBins);
		eastl::fill(m_punctualLightsDepthBins.begin(), m_punctualLightsDepthBins.end(), k_emptyBin);

		for (auto sortIndex : punctualLightsOrder)
		{
			const size_t lightIndex = static_cast<size_t>(sortIndex & 0xFFFFFFFF);
			const auto &light = renderWorld.m_punctualLights[lightIndex];

			PunctualLightGPU punctualLight = createPunctualLightGPU(light);
			m_punctualLights.push_back(punctualLight);

			if (light.m_spotLight)
			{
				m_lightTransforms.push_back(viewData.m_viewProjectionMatrix * glm::translate(light.m_position) * glm::mat4_cast(glm::rotation(glm::vec3(0.0f, -1.0f, 0.0f), light.m_direction)) * glm::scale(glm::vec3(light.m_radius)));
			}
			else
			{
				m_lightTransforms.push_back(viewData.m_viewProjectionMatrix * glm::translate(light.m_position) * glm::scale(glm::vec3(light.m_radius)));
			}

			insertPunctualLightIntoDepthBins(light, lightIndex, sortIndex, m_punctualLightsDepthBins.data());
		}
	}

	// process shadowed punctual lights
	{
		eastl::vector<uint64_t> punctualLightsOrder;
		for (size_t i = 0; i < renderWorld.m_punctualLightsShadowed.size(); ++i)
		{
			// TODO: cull lights here
			punctualLightsOrder.push_back(computePunctualLightSortValue(renderWorld.m_punctualLightsShadowed[i], viewData.m_viewMatrixDepthRow, i));
		}

		// sort by distance to camera
		eastl::sort(punctualLightsOrder.begin(), punctualLightsOrder.end());

		for (auto sortIndex : punctualLightsOrder)
		{
			const size_t lightIndex = static_cast<size_t>(sortIndex & 0xFFFFFFFF);
			const auto &light = renderWorld.m_punctualLightsShadowed[lightIndex];

			constexpr float k_nearPlane = 0.5f;
			glm::mat4 projection = glm::perspective(light.m_spotLight ? light.m_outerAngle : glm::radians(90.0f), 1.0f, light.m_radius, k_nearPlane);
			glm::mat4 invProjection = glm::inverse(projection);

			PunctualLightShadowedGPU punctualLight{};
			punctualLight.m_light = createPunctualLightGPU(light);
			punctualLight.m_depthProjectionParam0 = projection[2][2];
			punctualLight.m_depthProjectionParam1 = projection[3][2];
			punctualLight.m_depthUnprojectParam0 = invProjection[2][3];
			punctualLight.m_depthUnprojectParam1 = invProjection[3][3];
			punctualLight.m_lightRadius = light.m_radius;
			punctualLight.m_invLightRadius = 1.0f / light.m_radius;
			punctualLight.m_pcfRadius = calculatePenumbraRadiusNDC(calculateMaxPenumbraPercentage(0.5f), light.m_spotLight ? (0.5f * light.m_outerAngle) : glm::quarter_pi<float>());

			// spot light
			if (light.m_spotLight)
			{
				glm::vec3 upDir(0.0f, 1.0f, 0.0f);
				// choose different up vector if light direction would be linearly dependent otherwise
				if (fabsf(light.m_direction.x) < 0.001f && fabsf(light.m_direction.z) < 0.001f)
				{
					upDir = glm::vec3(1.0f, 0.0f, 0.0f);
				}

				glm::mat4 shadowViewMatrix = glm::lookAt(light.m_position, light.m_position + light.m_direction, upDir);
				glm::mat4 shadowMatrix = projection * shadowViewMatrix;
				glm::mat4 shadowMatrixTransposed = glm::transpose(shadowMatrix);

				// project shadow far plane to screen space to get the optimal size of the shadow map
				uint32_t resolution = calculateProjectedLightSize(viewData, shadowMatrix);

				rg::ResourceHandle shadowMapHandle = graph->createImage(rg::ImageDesc::create("Spot Light Shadow Map", Format::D16_UNORM, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, resolution, resolution));
				rg::ResourceViewHandle shadowMapViewHandle = graph->createImageView(rg::ImageViewDesc::create("Spot Light Shadow Map", shadowMapHandle));
				static_assert(sizeof(TextureViewHandle) == sizeof(shadowMapViewHandle));

				m_shadowMatrices.push_back(shadowMatrix);
				m_shadowTextureSampleHandles.push_back(shadowMapViewHandle);
				m_shadowTextureRenderHandles.push_back(graph->createImageView(rg::ImageViewDesc::create("Spot Light Shadow Map", shadowMapHandle)));

				punctualLight.m_shadowMatrix0 = shadowMatrixTransposed[0];
				punctualLight.m_shadowMatrix1 = shadowMatrixTransposed[1];
				punctualLight.m_shadowMatrix2 = shadowMatrixTransposed[2];
				punctualLight.m_shadowMatrix3 = shadowMatrixTransposed[3];
				punctualLight.m_shadowTextureHandle = shadowMapViewHandle; // we later correct these to be actual TextureViewHandles
			}
			// point light
			else
			{
				glm::mat4 viewMatrices[6];
				viewMatrices[0] = glm::lookAt(light.m_position, light.m_position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				viewMatrices[1] = glm::lookAt(light.m_position, light.m_position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				viewMatrices[2] = glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
				viewMatrices[3] = glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				viewMatrices[4] = glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				viewMatrices[5] = glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

				constexpr const char *resourceNames[]
				{
					"Point Light Shadow Map +X",
					"Point Light Shadow Map -X",
					"Point Light Shadow Map +Y",
					"Point Light Shadow Map -Y",
					"Point Light Shadow Map +Z",
					"Point Light Shadow Map -Z",
				};

				for (size_t i = 0; i < 6; ++i)
				{
					glm::mat4 shadowMatrix = projection * viewMatrices[i];

					// project shadow far plane to screen space to get the optimal size of the shadow map
					uint32_t resolution = calculateProjectedLightSize(viewData, shadowMatrix);

					rg::ResourceHandle shadowMapHandle = graph->createImage(rg::ImageDesc::create(resourceNames[i], Format::D16_UNORM, ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlags::TEXTURE_BIT, resolution, resolution));
					rg::ResourceViewHandle shadowMapViewHandle = graph->createImageView(rg::ImageViewDesc::create(resourceNames[i], shadowMapHandle));
					static_assert(sizeof(TextureViewHandle) == sizeof(shadowMapViewHandle));

					m_shadowMatrices.push_back(shadowMatrix);
					m_shadowTextureSampleHandles.push_back(shadowMapViewHandle);
					m_shadowTextureRenderHandles.push_back(graph->createImageView(rg::ImageViewDesc::create(resourceNames[i], shadowMapHandle)));

					// we later correct these to be actual TextureViewHandles
					if (i < 4)
					{
						punctualLight.m_shadowMatrix0[i] = util::asfloat(shadowMapViewHandle);
					}
					else
					{
						punctualLight.m_shadowMatrix1[i - 4] = util::asfloat(shadowMapViewHandle);
					}
				}
			}

			m_punctualLightsShadowed.push_back(punctualLight);

			if (light.m_spotLight)
			{
				m_lightTransforms.push_back(viewData.m_viewProjectionMatrix * glm::translate(light.m_position) * glm::mat4_cast(glm::rotation(glm::vec3(0.0f, -1.0f, 0.0f), light.m_direction)) * glm::scale(glm::vec3(light.m_radius)));
			}
			else
			{
				m_lightTransforms.push_back(viewData.m_viewProjectionMatrix * glm::translate(light.m_position) * glm::scale(glm::vec3(light.m_radius)));
			}

			insertPunctualLightIntoDepthBins(light, lightIndex, sortIndex, m_punctualLightsDepthBins.data());
		}
	}

	auto createShaderResourceBuffer = [&](DescriptorType descriptorType, auto dataAsVector, bool copyNow = true, void **resultBufferPtr = nullptr)
	{
		const size_t elementSize = sizeof(dataAsVector[0]);

		// prepare DescriptorBufferInfo
		DescriptorBufferInfo bufferInfo = Initializers::structuedBufferInfo(elementSize, dataAsVector.size());
		bufferInfo.m_buffer = viewData.m_rendererResources->m_shaderResourceBufferStackAllocators[viewData.m_resIdx]->getBuffer();

		// allocate memory
		uint64_t alignment = viewData.m_device->getBufferAlignment(descriptorType, elementSize);
		uint8_t *bufferPtr = viewData.m_rendererResources->m_shaderResourceBufferStackAllocators[viewData.m_resIdx]->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

		// copy to destination
		if (copyNow && !dataAsVector.empty())
		{
			memcpy(bufferPtr, dataAsVector.data(), dataAsVector.size() * elementSize);
		}

		if (resultBufferPtr)
		{
			*resultBufferPtr = bufferPtr;
		}

		// create a transient bindless handle
		return descriptorType == DescriptorType::STRUCTURED_BUFFER ?
			viewData.m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true) :
			viewData.m_viewRegistry->createByteBufferViewHandle(bufferInfo, true);
	};

	// punctual lights tile texture
	const uint32_t punctualLightsTileTextureWidth = (viewData.m_width + k_lightingTileSize - 1) / k_lightingTileSize;
	const uint32_t punctualLightsTileTextureHeight = (viewData.m_height + k_lightingTileSize - 1) / k_lightingTileSize;
	const uint32_t punctualLightsTileTextureLayerCount = eastl::max<uint32_t>(1, ((uint32_t)m_punctualLights.size() + 31) / 32);
	rg::ResourceHandle punctualLightsTileTextureHandle = graph->createImage(rg::ImageDesc::create("Punctual Lights Tile Image", Format::R32_UINT, ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TEXTURE_BIT, punctualLightsTileTextureWidth, punctualLightsTileTextureHeight, 1, punctualLightsTileTextureLayerCount));
	rg::ResourceViewHandle punctualLightsTileTextureViewHandle = graph->createImageView(rg::ImageViewDesc::create("Punctual Lights Tile Image", punctualLightsTileTextureHandle, { 0, 1, 0, punctualLightsTileTextureLayerCount }, ImageViewType::_2D_ARRAY));

	// punctual lights shadowed tile texture
	const uint32_t punctualLightsShadowedTileTextureLayerCount = eastl::max<uint32_t>(1, ((uint32_t)m_punctualLightsShadowed.size() + 31) / 32);
	rg::ResourceHandle punctualLightsShadowedTileTextureHandle = graph->createImage(rg::ImageDesc::create("Punctual Lights Shadowed Tile Image", Format::R32_UINT, ImageUsageFlags::RW_TEXTURE_BIT | ImageUsageFlags::TEXTURE_BIT, punctualLightsTileTextureWidth, punctualLightsTileTextureHeight, 1, punctualLightsShadowedTileTextureLayerCount));
	rg::ResourceViewHandle punctualLightsShadowedTileTextureViewHandle = graph->createImageView(rg::ImageViewDesc::create("Punctual Lights Shadowed Tile Image", punctualLightsShadowedTileTextureHandle, { 0, 1, 0, punctualLightsTileTextureLayerCount }, ImageViewType::_2D_ARRAY));

	m_lightRecordData.m_directionalLightCount = static_cast<uint32_t>(m_directionalLights.size());
	m_lightRecordData.m_directionalLightsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_directionalLights);

	m_lightRecordData.m_directionalLightShadowedCount = static_cast<uint32_t>(m_shadowedDirectionalLights.size());
	void *directionalLightsShadowedBufferPtr = nullptr;
	m_lightRecordData.m_directionalLightsShadowedBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_shadowedDirectionalLights, false, &directionalLightsShadowedBufferPtr);

	m_lightRecordData.m_punctualLightCount = static_cast<uint32_t>(m_punctualLights.size());
	m_lightRecordData.m_punctualLightsTileTextureViewHandle = punctualLightsTileTextureViewHandle;
	m_lightRecordData.m_punctualLightsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_punctualLights);
	m_lightRecordData.m_punctualLightsDepthBinsBufferViewHandle = (ByteBufferViewHandle)createShaderResourceBuffer(DescriptorType::BYTE_BUFFER, m_punctualLightsDepthBins);

	m_lightRecordData.m_punctualLightShadowedCount = static_cast<uint32_t>(m_punctualLightsShadowed.size());
	m_lightRecordData.m_punctualLightsShadowedTileTextureViewHandle = punctualLightsShadowedTileTextureViewHandle;
	void *punctualLightsShadowedBufferPtr = nullptr;
	m_lightRecordData.m_punctualLightsShadowedBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_punctualLightsShadowed, false, &punctualLightsShadowedBufferPtr);
	m_lightRecordData.m_punctualLightsShadowedDepthBinsBufferViewHandle = (ByteBufferViewHandle)createShaderResourceBuffer(DescriptorType::BYTE_BUFFER, m_punctualLightsShadowedDepthBins);

	m_lightRecordData.m_shadowMapViewHandles = m_shadowTextureSampleHandles.data();
	m_lightRecordData.m_shadowMapViewHandleCount = static_cast<uint32_t>(m_shadowTextureSampleHandles.size());

	m_lightTransformsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_lightTransforms);

	graph->addPass("Prepare Light Data", rg::QueueType::GRAPHICS, 0, nullptr, [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			// correct render graph handles to be actual bindless handles
			for (auto &dirLight : m_shadowedDirectionalLights)
			{
				dirLight.m_shadowTextureHandle = static_cast<TextureViewHandle>(registry.getBindlessHandle(static_cast<rg::ResourceViewHandle>(dirLight.m_shadowTextureHandle), DescriptorType::TEXTURE));
			}

			if (!m_shadowedDirectionalLights.empty())
			{
				memcpy(directionalLightsShadowedBufferPtr, m_shadowedDirectionalLights.data(), m_shadowedDirectionalLights.size() * sizeof(DirectionalLightGPU));
			}


			// correct render graph handles to be actual bindless handles
			for (auto &punctualLight : m_punctualLightsShadowed)
			{
				if (punctualLight.m_light.m_angleScale != -1.0f)
				{
					punctualLight.m_shadowTextureHandle = static_cast<TextureViewHandle>(registry.getBindlessHandle(static_cast<rg::ResourceViewHandle>(punctualLight.m_shadowTextureHandle), DescriptorType::TEXTURE));
				}
				else
				{
					for (size_t i = 0; i < 6; ++i)
					{
						if (i < 4)
						{
							punctualLight.m_shadowMatrix0[i] = util::asfloat(registry.getBindlessHandle(static_cast<rg::ResourceViewHandle>(util::asuint(punctualLight.m_shadowMatrix0[i])), DescriptorType::TEXTURE));
						}
						else
						{
							punctualLight.m_shadowMatrix1[i - 4] = util::asfloat(registry.getBindlessHandle(static_cast<rg::ResourceViewHandle>(util::asuint(punctualLight.m_shadowMatrix1[i - 4])), DescriptorType::TEXTURE));
						}
					}
				}
			}

			if (!m_punctualLightsShadowed.empty())
			{
				memcpy(punctualLightsShadowedBufferPtr, m_punctualLightsShadowed.data(), m_punctualLightsShadowed.size() * sizeof(PunctualLightShadowedGPU));
			}
		});
}

void LightManager::recordLightTileAssignment(rg::RenderGraph *graph, const CommonViewData &viewData) noexcept
{
	if (!m_punctualLights.empty() || !m_punctualLightsShadowed.empty())
	{
		eastl::fixed_vector<rg::ResourceUsageDesc, 2> usageDescs;
		if (!m_punctualLights.empty())
		{
			usageDescs.push_back({ m_lightRecordData.m_punctualLightsTileTextureViewHandle, {ResourceState::CLEAR_RESOURCE}, {ResourceState::RW_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
		}
		if (!m_punctualLightsShadowed.empty())
		{
			usageDescs.push_back({ m_lightRecordData.m_punctualLightsShadowedTileTextureViewHandle, {ResourceState::CLEAR_RESOURCE}, {ResourceState::RW_RESOURCE, PipelineStageFlags::PIXEL_SHADER_BIT} });
		}
		graph->addPass("Light Tile Assignment", rg::QueueType::GRAPHICS, usageDescs.size(), usageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
			{
				GAL_SCOPED_GPU_LABEL(cmdList, "Light Tile Assignment");
				PROFILING_GPU_ZONE_SCOPED_N(m_device->getProfilingContext(), cmdList, "Light Tile Assignment");
				PROFILING_ZONE_SCOPED;

				const uint32_t lightWordCount = eastl::max<uint32_t>(1, ((uint32_t)m_punctualLights.size() + 31) / 32);
				const uint32_t lightShadowedWordCount = eastl::max<uint32_t>(1, ((uint32_t)m_punctualLightsShadowed.size() + 31) / 32);

				// clear tile textures
				{
					eastl::fixed_vector<Image *, 2> images;
					eastl::fixed_vector<ImageSubresourceRange, 2> ranges;
					if (!m_punctualLights.empty())
					{
						Image *image = registry.getImage(m_lightRecordData.m_punctualLightsTileTextureViewHandle);
						ClearColorValue clearColor{};
						ImageSubresourceRange range = { 0, 1, 0, lightWordCount };
						cmdList->clearColorImage(image, &clearColor, 1, &range);
						images.push_back(image);
						ranges.push_back(range);
					}
					if (!m_punctualLightsShadowed.empty())
					{
						Image *image = registry.getImage(m_lightRecordData.m_punctualLightsShadowedTileTextureViewHandle);
						ClearColorValue clearColor{};
						ImageSubresourceRange range = { 0, 1, 0, lightShadowedWordCount };
						cmdList->clearColorImage(image, &clearColor, 1, &range);
						images.push_back(image);
						ranges.push_back(range);
					}

					eastl::fixed_vector<Barrier, 2> barriers;
					for (size_t i = 0; i < images.size(); ++i)
					{
						barriers.push_back(Initializers::imageBarrier(images[i], PipelineStageFlags::CLEAR_BIT, PipelineStageFlags::PIXEL_SHADER_BIT, ResourceState::CLEAR_RESOURCE, ResourceState::RW_RESOURCE, ranges[i]));
					}
					cmdList->barrier(static_cast<uint32_t>(barriers.size()), barriers.data());
				}

				struct PassConstants
				{
					uint32_t tileCountX;
					uint32_t transformBufferIndex;
					uint32_t wordCount;
					uint32_t resultTextureIndex;
				};

				ProxyMeshInfo &pointLightProxyMeshInfo = viewData.m_rendererResources->m_icoSphereProxyMeshInfo;
				ProxyMeshInfo *spotLightProxyMeshInfo[]
				{
					&viewData.m_rendererResources->m_cone45ProxyMeshInfo,
					&viewData.m_rendererResources->m_cone90ProxyMeshInfo,
					&viewData.m_rendererResources->m_cone135ProxyMeshInfo,
					&viewData.m_rendererResources->m_cone180ProxyMeshInfo,
				};

				const uint32_t width = (viewData.m_width + 1) / 2;
				const uint32_t height = (viewData.m_height + 1) / 2;
				Rect renderRect{ {0, 0}, {width, height} };

				cmdList->beginRenderPass(0, nullptr, nullptr, renderRect, true);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {width, height} };
					cmdList->setScissor(0, 1, &scissor);

					cmdList->bindPipeline(m_lightTileAssignmentPipeline);

					gal::DescriptorSet *sets[] = { viewData.m_rendererResources->m_offsetBufferDescriptorSets[viewData.m_resIdx], viewData.m_viewRegistry->getCurrentFrameDescriptorSet() };
					cmdList->bindDescriptorSets(m_lightTileAssignmentPipeline, 1, 1, &sets[1], 0, nullptr);

					cmdList->bindIndexBuffer(viewData.m_rendererResources->m_proxyMeshIndexBuffer, 0, IndexType::UINT16);

					uint64_t vertexOffset = 0;
					cmdList->bindVertexBuffers(0, 1, &viewData.m_rendererResources->m_proxyMeshVertexBuffer, &vertexOffset);

					for (size_t shadowed = 0; shadowed < 2; ++shadowed)
					{
						const bool isShadowed = shadowed != 0;

						const size_t lightCount = isShadowed ? m_punctualLightsShadowed.size() : m_punctualLights.size();

						if (lightCount == 0)
						{
							continue;
						}

						PassConstants passConsts{};
						passConsts.tileCountX = (viewData.m_width + k_lightingTileSize - 1) / k_lightingTileSize;
						passConsts.transformBufferIndex = m_lightTransformsBufferViewHandle;
						passConsts.wordCount = isShadowed ? lightShadowedWordCount : lightWordCount;
						passConsts.resultTextureIndex = registry.getBindlessHandle(isShadowed ? m_lightRecordData.m_punctualLightsShadowedTileTextureViewHandle : m_lightRecordData.m_punctualLightsTileTextureViewHandle, DescriptorType::RW_TEXTURE);

						uint32_t passConstsAddress = (uint32_t)viewData.m_rendererResources->m_constantBufferStackAllocators[viewData.m_resIdx]->uploadStruct(DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

						cmdList->bindDescriptorSets(m_lightTileAssignmentPipeline, 0, 1, &sets[0], 1, &passConstsAddress);

						for (size_t i = 0; i < lightCount; ++i)
						{
							struct PushConsts
							{
								uint32_t lightIndex;
								uint32_t transformIndex;
							};

							PushConsts pushConsts{};
							pushConsts.lightIndex = static_cast<uint32_t>(i);
							pushConsts.transformIndex = pushConsts.lightIndex + (isShadowed ? static_cast<uint32_t>(m_punctualLights.size()) : 0);

							cmdList->pushConstants(m_lightTileAssignmentPipeline, ShaderStageFlags::VERTEX_BIT | ShaderStageFlags::PIXEL_BIT, 0, sizeof(pushConsts), &pushConsts);

							const auto &light = isShadowed ? m_punctualLightsShadowed[i].m_light : m_punctualLights[i];
							const bool spotLight = light.m_angleScale != -1.0f;
							size_t spotLightProxyMeshIndex = -1;
							if (spotLight)
							{
								float outerAngle = glm::degrees(acosf(-(light.m_angleOffset / light.m_angleScale)) * 2.0f);
								spotLightProxyMeshIndex = static_cast<size_t>(floorf(outerAngle / 45.0f));
							}

							ProxyMeshInfo &proxyMeshInfo = !spotLight ? pointLightProxyMeshInfo : *spotLightProxyMeshInfo[spotLightProxyMeshIndex];

							cmdList->drawIndexed(proxyMeshInfo.m_indexCount, 1, proxyMeshInfo.m_firstIndex, proxyMeshInfo.m_vertexOffset, 0);
						}
					}
				}
				cmdList->endRenderPass();
			});
	}
}

void LightManager::recordShadows(rg::RenderGraph *graph, const CommonViewData &viewData, const ShadowRecordData &data) noexcept
{
	eastl::fixed_vector<rg::ResourceUsageDesc, 32> usageDescs;

	for (auto h : m_shadowTextureRenderHandles)
	{
		usageDescs.push_back({ h, {ResourceState::WRITE_DEPTH_STENCIL} });
	}

	graph->addPass("Shadows", rg::QueueType::GRAPHICS, usageDescs.size(), usageDescs.data(), [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "Shadows");
			PROFILING_GPU_ZONE_SCOPED_N(m_device->getProfilingContext(), cmdList, "Shadows");
			PROFILING_ZONE_SCOPED;

			size_t curDepthBufferHandleOffset = 0;

			// iterate over all shadow map jobs
			for (auto &shadowMatrix : m_shadowMatrices)
			{
				auto shadowTextureHandle = m_shadowTextureRenderHandles[curDepthBufferHandleOffset++];

				DepthStencilAttachmentDescription depthBufferDesc{ registry.getImageView(shadowTextureHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE };
				const auto &imageDesc = registry.getImage(shadowTextureHandle)->getDescription();
				Rect renderRect{ {0, 0}, {imageDesc.m_width, imageDesc.m_height} };

				cmdList->beginRenderPass(0, nullptr, &depthBufferDesc, renderRect, false);
				{
					Viewport viewport{ 0.0f, 0.0f, (float)imageDesc.m_width, (float)imageDesc.m_height, 0.0f, 1.0f };
					cmdList->setViewport(0, 1, &viewport);
					Rect scissor{ {0, 0}, {imageDesc.m_width, imageDesc.m_height} };
					cmdList->setScissor(0, 1, &scissor);

					struct PassConstants
					{
						float viewProjectionMatrix[16];
						uint32_t transformBufferIndex;
						uint32_t skinningMatricesBufferIndex;
						uint32_t materialBufferIndex;
					};

					PassConstants passConsts;
					memcpy(passConsts.viewProjectionMatrix, &shadowMatrix[0][0], sizeof(passConsts.viewProjectionMatrix));
					passConsts.transformBufferIndex = data.m_transformBufferHandle;
					passConsts.skinningMatricesBufferIndex = data.m_skinningMatrixBufferHandle;
					passConsts.materialBufferIndex = data.m_materialsBufferHandle;

					uint32_t passConstsAddress = (uint32_t)viewData.m_rendererResources->m_constantBufferStackAllocators[viewData.m_resIdx]->uploadStruct(gal::DescriptorType::OFFSET_CONSTANT_BUFFER, passConsts);

					const eastl::vector<SubMeshInstanceData> *instancesArr[]
					{
						&data.m_renderList->m_opaque,
						&data.m_renderList->m_opaqueAlphaTested,
						&data.m_renderList->m_opaqueSkinned,
						&data.m_renderList->m_opaqueSkinnedAlphaTested,
					};
					GraphicsPipeline *pipelines[]
					{
						m_shadowPipeline,
						m_shadowAlphaTestedPipeline,
						m_shadowSkinnedPipeline,
						m_shadowSkinnedAlphaTestedPipeline,
					};

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

						gal::DescriptorSet *sets[] = { viewData.m_rendererResources->m_offsetBufferDescriptorSets[viewData.m_resIdx], viewData.m_viewRegistry->getCurrentFrameDescriptorSet() };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets, 1, &passConstsAddress);

						for (const auto &instance : *instancesArr[listType])
						{
							struct MeshConstants
							{
								uint32_t transformIndex;
								uint32_t uintData[2];
							};

							MeshConstants consts{};
							consts.transformIndex = instance.m_transformIndex;

							size_t uintDataOffset = 0;
							if (skinned)
							{
								consts.uintData[uintDataOffset++] = instance.m_skinningMatricesOffset;
							}
							if (alphaTested)
							{
								consts.uintData[uintDataOffset++] = instance.m_materialHandle;
							}

							cmdList->pushConstants(pipeline, ShaderStageFlags::VERTEX_BIT, 0, static_cast<uint32_t>(sizeof(uint32_t) + sizeof(uint32_t) * uintDataOffset), &consts);


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
			}
		});
}

const LightRecordData *LightManager::getLightRecordData() const noexcept
{
	return &m_lightRecordData;
}
