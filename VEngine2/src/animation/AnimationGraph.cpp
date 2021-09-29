#include "AnimationGraph.h"
#include "utility/Utility.h"
#include <ecs/ECS.h>

AnimationGraph::AnimationGraph(
	size_t nodeCount, 
	const AnimationGraphNode *nodes, 
	size_t parameterCount, 
	const AnimationGraphParameter *parameters, 
	size_t animationClipCount,
	const Asset<AnimationClipAssetData> *animationClips,
	AnimationGraphLogicCallback logicCallback
) noexcept
	:m_nodeCount(nodeCount),
	m_parameterCount(parameterCount),
	m_animationClipCount(animationClipCount),
	m_logicCallback(logicCallback)
{
	m_nodes = new AnimationGraphNode[m_nodeCount];
	memcpy(m_nodes, nodes, sizeof(m_nodes[0]) * m_nodeCount);

	m_parameters = new AnimationGraphParameter[m_parameterCount];
	memcpy(m_parameters, parameters, sizeof(m_parameters[0]) * m_parameterCount);

	m_animationClipAssets = new Asset<AnimationClipAssetData>[m_animationClipCount];
	for (size_t i = 0; i < m_animationClipCount; ++i)
	{
		m_animationClipAssets[i] = animationClips[i];
	}
}

AnimationGraph::AnimationGraph(const AnimationGraph &other) noexcept
	:m_nodeCount(other.m_nodeCount),
	m_parameterCount(other.m_parameterCount),
	m_animationClipCount(other.m_animationClipCount),
	m_logicCallback(other.m_logicCallback)
{
	m_nodes = new AnimationGraphNode[m_nodeCount];
	memcpy(m_nodes, other.m_nodes, sizeof(m_nodes[0]) * m_nodeCount);

	m_parameters = new AnimationGraphParameter[m_parameterCount];
	memcpy(m_parameters, other.m_parameters, sizeof(m_parameters[0]) * m_parameterCount);

	m_animationClipAssets = new Asset<AnimationClipAssetData>[m_animationClipCount];
	for (size_t i = 0; i < m_animationClipCount; ++i)
	{
		m_animationClipAssets[i] = other.m_animationClipAssets[i];
	}
}

AnimationGraph::AnimationGraph(AnimationGraph &&other) noexcept
	:m_nodeCount(other.m_nodeCount),
	m_parameterCount(other.m_parameterCount),
	m_animationClipCount(other.m_animationClipCount),
	m_logicCallback(other.m_logicCallback)
{
	m_nodes = other.m_nodes;
	other.m_nodes = nullptr;

	m_parameters = other.m_parameters;
	other.m_parameters = nullptr;

	m_animationClipAssets = other.m_animationClipAssets;
	other.m_animationClipAssets = nullptr;
}

AnimationGraph &AnimationGraph::operator=(const AnimationGraph &other) noexcept
{
	if (this != &other)
	{
		delete[] m_nodes;
		delete[] m_parameters;
		delete[] m_animationClipAssets;

		m_nodeCount = other.m_nodeCount;
		m_parameterCount = other.m_parameterCount;
		m_animationClipCount = other.m_animationClipCount;

		m_logicCallback = other.m_logicCallback;

		m_nodes = new AnimationGraphNode[m_nodeCount];
		memcpy(m_nodes, other.m_nodes, sizeof(m_nodes[0]) * m_nodeCount);
		m_parameters = new AnimationGraphParameter[m_parameterCount];
		memcpy(m_parameters, other.m_parameters, sizeof(m_parameters[0]) * m_parameterCount);
		
		m_animationClipAssets = new Asset<AnimationClipAssetData> [m_animationClipCount];
		for (size_t i = 0; i < m_animationClipCount; ++i)
		{
			m_animationClipAssets[i] = other.m_animationClipAssets[i];
		}
	}

	return *this;
}

AnimationGraph &AnimationGraph::operator=(AnimationGraph &&other) noexcept
{
	if (this != &other)
	{
		delete[] m_nodes;
		delete[] m_parameters;

		m_nodeCount = other.m_nodeCount;
		m_parameterCount = other.m_parameterCount;
		m_animationClipCount = other.m_animationClipCount;

		m_logicCallback = other.m_logicCallback;

		m_nodes = other.m_nodes;
		other.m_nodes = nullptr;
		m_parameters = other.m_parameters;
		other.m_parameters = nullptr;
		m_animationClipAssets = other.m_animationClipAssets;
		other.m_animationClipAssets = nullptr;
	}

	return *this;
}

AnimationGraph::~AnimationGraph() noexcept
{
	delete[] m_nodes;
	delete[] m_parameters;
	delete[] m_animationClipAssets;
}

void AnimationGraph::preEvaluate(ECS *ecs, EntityID entity, float deltaTime) noexcept
{
	m_logicCallback(this, ecs, entity, deltaTime);
}

JointPose AnimationGraph::evaluate(size_t jointIdx) const noexcept
{
	return evaluate(jointIdx, m_nodes[0]);
}

void AnimationGraph::updatePhase(float deltaTime) noexcept
{
	const float duration = evaluateDuration(m_nodes[0]);
	m_phase += deltaTime / duration;

	if (m_phase > 1.0f || m_phase < 0.0f)
	{
		m_phase -= floorf(m_phase);
	}
}

void AnimationGraph::setPhase(float phase) noexcept
{
	m_phase = phase;

	if (m_phase > 1.0f || m_phase < 0.0f)
	{
		m_phase -= floorf(m_phase);
	}
}

bool AnimationGraph::setFloatParam(const StringID &paramName, float value) noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::FLOAT)
			{
				m_parameters[i].m_data.f = value;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraph::setIntParam(const StringID &paramName, int32_t value) noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::INT)
			{
				m_parameters[i].m_data.i = value;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraph::setBoolParam(const StringID &paramName, bool value) noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::BOOL)
			{
				m_parameters[i].m_data.b = value;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraph::getFloatParam(const StringID &paramName, float *value) const noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::FLOAT)
			{
				*value = m_parameters[i].m_data.f;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraph::getIntParam(const StringID &paramName, int32_t *value) const noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::INT)
			{
				*value = m_parameters[i].m_data.i;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraph::getBoolParam(const StringID &paramName, bool *value) const noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::BOOL)
			{
				*value = m_parameters[i].m_data.b;
				return true;
			}
			return false;
		}
	}

	return false;
}

size_t AnimationGraph::getNodeCount() const noexcept
{
	return m_nodeCount;
}

const AnimationGraphNode *AnimationGraph::getNodes() const noexcept
{
	return m_nodes;
}

size_t AnimationGraph::getParameterCount() const noexcept
{
	return m_parameterCount;
}

const AnimationGraphParameter *AnimationGraph::getParameters() const noexcept
{
	return m_parameters;
}

JointPose AnimationGraph::evaluate(size_t jointIdx, const AnimationGraphNode &node) const noexcept
{
	switch (node.m_nodeType)
	{
	case AnimationGraphNodeType::ANIM_CLIP:
	{
		const auto nodeData = node.m_nodeData.m_clipNodeData;
		const auto *animClip = m_animationClipAssets[nodeData.m_animClip]->getAnimationClip();
		const float clipDuration = animClip->getDuration();
		return animClip->getJointPose(jointIdx, m_phase * clipDuration, getBoolParam(nodeData.m_loop), false);
	}
	case AnimationGraphNodeType::LERP:
	{
		const auto nodeData = node.m_nodeData.m_lerpNodeData;
		return lerp(jointIdx, m_nodes[nodeData.m_inputA], m_nodes[nodeData.m_inputB], getFloatParam(nodeData.m_alpha));
	}
	case AnimationGraphNodeType::LERP_1D_ARRAY:
	{
		const auto nodeData = node.m_nodeData.m_lerp1DArrayNodeData;

		assert(nodeData.m_inputCount <= AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs);

		size_t index0;
		size_t index1;
		float alpha;
		util::findPieceWiseLinearCurveIndicesAndAlpha(nodeData.m_inputCount, nodeData.m_inputKeys, getFloatParam(nodeData.m_alpha), false, &index0, &index1, &alpha);

		if (index0 == index1)
		{
			return evaluate(jointIdx, m_nodes[nodeData.m_inputs[index0]]);
		}
		else
		{
			return lerp(jointIdx, m_nodes[nodeData.m_inputs[index0]], m_nodes[nodeData.m_inputs[index1]], alpha);
		}
	}
	case AnimationGraphNodeType::LERP_2D:
	{
		const auto nodeData = node.m_nodeData.m_lerp2DNodeData;

		const float alphaX = getFloatParam(nodeData.m_alphaX);
		const float alphaY = getFloatParam(nodeData.m_alphaY);

		if (alphaY == 0.0f)
		{
			return lerp(jointIdx, m_nodes[nodeData.m_inputTL], m_nodes[nodeData.m_inputTR], alphaX);
		}
		else if (alphaY == 1.0f)
		{
			return lerp(jointIdx, m_nodes[nodeData.m_inputBL], m_nodes[nodeData.m_inputBR], alphaX);
		}
		else
		{
			JointPose t = lerp(jointIdx, m_nodes[nodeData.m_inputTL], m_nodes[nodeData.m_inputTR], alphaX);
			JointPose b = lerp(jointIdx, m_nodes[nodeData.m_inputBL], m_nodes[nodeData.m_inputBR], alphaX);
			return JointPose::lerp(t, b, alphaY);
		}
	}
	default:
		assert(false);
		break;
	}
	return JointPose();
}

float AnimationGraph::evaluateDuration(const AnimationGraphNode &node) const noexcept
{
	switch (node.m_nodeType)
	{
	case AnimationGraphNodeType::ANIM_CLIP:
	{
		const auto nodeData = node.m_nodeData.m_clipNodeData;
		const auto *animClip = m_animationClipAssets[nodeData.m_animClip]->getAnimationClip();
		return animClip->getDuration();
	}
	case AnimationGraphNodeType::LERP:
	{
		const auto nodeData = node.m_nodeData.m_lerpNodeData;
		return lerpDuration(m_nodes[nodeData.m_inputA], m_nodes[nodeData.m_inputB], getFloatParam(nodeData.m_alpha));
	}
	case AnimationGraphNodeType::LERP_1D_ARRAY:
	{
		const auto nodeData = node.m_nodeData.m_lerp1DArrayNodeData;

		assert(nodeData.m_inputCount <= AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs);

		size_t index0;
		size_t index1;
		float alpha;
		util::findPieceWiseLinearCurveIndicesAndAlpha(nodeData.m_inputCount, nodeData.m_inputKeys, getFloatParam(nodeData.m_alpha), false, &index0, &index1, &alpha);

		if (index0 == index1)
		{
			return evaluateDuration(m_nodes[nodeData.m_inputs[index0]]);
		}
		else
		{
			return lerpDuration(m_nodes[nodeData.m_inputs[index0]], m_nodes[nodeData.m_inputs[index1]], alpha);
		}
	}
	case AnimationGraphNodeType::LERP_2D:
	{
		const auto nodeData = node.m_nodeData.m_lerp2DNodeData;

		const float alphaX = getFloatParam(nodeData.m_alphaX);
		const float alphaY = getFloatParam(nodeData.m_alphaY);

		if (alphaY == 0.0f)
		{
			return lerpDuration(m_nodes[nodeData.m_inputTL], m_nodes[nodeData.m_inputTR], alphaX);
		}
		else if (alphaY == 1.0f)
		{
			return lerpDuration(m_nodes[nodeData.m_inputBL], m_nodes[nodeData.m_inputBR], alphaX);
		}
		else
		{
			const float t = lerpDuration(m_nodes[nodeData.m_inputTL], m_nodes[nodeData.m_inputTR], alphaX);
			const float b = lerpDuration(m_nodes[nodeData.m_inputBL], m_nodes[nodeData.m_inputBR], alphaX);
			return t * (1.0f - alphaY) + b * alphaY;
		}
	}
	default:
		assert(false);
		break;
	}
	return 0.0f;
}

JointPose AnimationGraph::lerp(size_t jointIdx, const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept
{
	if (alpha == 0.0f || alpha == 1.0f)
	{
		return evaluate(jointIdx, (alpha == 0.0f) ? x : y);
	}
	else
	{
		const JointPose a = evaluate(jointIdx, x);
		const JointPose b = evaluate(jointIdx, y);
		return JointPose::lerp(a, b, alpha);
	}
}

float AnimationGraph::lerpDuration(const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept
{
	if (alpha == 0.0f || alpha == 1.0f)
	{
		return evaluateDuration((alpha == 0.0f) ? x : y);
	}
	else
	{
		const float a = evaluateDuration(x);
		const float b = evaluateDuration(y);
		return a * (1.0f - alpha) + b * alpha;
	}
}

float AnimationGraph::getFloatParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::FLOAT);
	return m_parameters[idx].m_data.f;
}

int32_t AnimationGraph::getIntParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::INT);
	return m_parameters[idx].m_data.i;
}

bool AnimationGraph::getBoolParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::BOOL);
	return m_parameters[idx].m_data.b;
}
