#include "AnimationGraph.h"

AnimationGraph::AnimationGraph(const AnimationGraphCreateInfo &createInfo) noexcept
	:m_rootNodeIndex(createInfo.m_rootNodeIndex),
	m_nodeCount(createInfo.m_nodeCount),
	m_parameterCount(createInfo.m_parameterCount),
	m_animationClipCount(createInfo.m_animationClipCount),
	m_nodes(createInfo.m_nodes),
	m_parameters(createInfo.m_parameters),
	m_animationClipAssets(createInfo.m_animationClips),
	m_controllerScript(createInfo.m_controllerScript),
	m_memory(createInfo.m_memory)
{
	m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
}

AnimationGraph::AnimationGraph(AnimationGraph &&other) noexcept
	:m_rootNodeIndex(other.m_rootNodeIndex),
	m_nodeCount(other.m_nodeCount),
	m_parameterCount(other.m_parameterCount),
	m_animationClipCount(other.m_animationClipCount),
	m_nodes(other.m_nodes),
	m_parameters(other.m_parameters),
	m_animationClipAssets(other.m_animationClipAssets),
	m_controllerScript(other.m_controllerScript),
	m_memory(other.m_memory)
{
	other.m_memory = nullptr;

	m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
}

AnimationGraph &AnimationGraph::operator=(AnimationGraph &&other) noexcept
{
	if (this != &other)
	{
		if (m_memory)
		{
			for (size_t i = 0; i < m_animationClipCount; ++i)
			{
				m_animationClipAssets[i].~Asset();
			}
			delete[] m_memory;
		}

		m_rootNodeIndex = other.m_rootNodeIndex;
		m_nodeCount = other.m_nodeCount;
		m_parameterCount = other.m_parameterCount;
		m_animationClipCount = other.m_animationClipCount;
		m_nodes = other.m_nodes;
		m_parameters = other.m_parameters;
		m_animationClipAssets = other.m_animationClipAssets;
		m_controllerScript = other.m_controllerScript;
		m_memory = other.m_memory;

		other.m_memory = nullptr;

		m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
	}

	return *this;
}

AnimationGraph::~AnimationGraph() noexcept
{
	if (m_memory)
	{
		for (size_t i = 0; i < m_animationClipCount; ++i)
		{
			m_animationClipAssets[i].~Asset();
		}
		delete[] m_memory;
		m_memory = nullptr;
	}
}

uint32_t AnimationGraph::getNodeCount() const noexcept
{
	return m_nodeCount;
}

uint32_t AnimationGraph::getParameterCount() const noexcept
{
	return m_parameterCount;
}

const AnimationGraphNode *AnimationGraph::getNodes() const noexcept
{
	return m_nodes;
}

uint32_t AnimationGraph::getAnimationClipAssetCount() const noexcept
{
	return m_animationClipCount;
}

const AnimationGraphParameter *AnimationGraph::getParameters() const noexcept
{
	return m_parameters;
}

const Asset<AnimationClipAsset> *AnimationGraph::getAnimationClipAssets() const noexcept
{
	return m_animationClipAssets;
}

Asset<ScriptAsset> AnimationGraph::getControllerScript() const noexcept
{
	return m_controllerScript;
}

uint32_t AnimationGraph::getRootNodeIndex() const noexcept
{
	return m_rootNodeIndex;
}

bool AnimationGraph::isValid() const noexcept
{
	return m_rootNodeIndex != -1 && m_isValid;
}

bool AnimationGraph::validate(AnimationGraphNodeData::NodeIndex idx) const noexcept
{
	if (idx == -1)
	{
		return false;
	}

	const auto &node = m_nodes[idx];

	switch (node.m_nodeType)
	{
	case AnimationGraphNodeType::AnimClip:
	{
		return node.m_nodeData.m_clipNodeData.m_animClip != -1 && m_animationClipAssets[node.m_nodeData.m_clipNodeData.m_animClip].get() != nullptr;
	}
	case AnimationGraphNodeType::Lerp:
	{
		return validate(node.m_nodeData.m_lerpNodeData.m_inputA) && validate(node.m_nodeData.m_lerpNodeData.m_inputB);
	}
	case AnimationGraphNodeType::Lerp1DArray:
	{
		for (size_t i = 0; i < node.m_nodeData.m_lerp1DArrayNodeData.m_inputCount; ++i)
		{
			if (!validate(node.m_nodeData.m_lerp1DArrayNodeData.m_inputs[i]))
			{
				return false;
			}
		}
		return true;
	}
	case AnimationGraphNodeType::Lerp2D:
	{
		return validate(node.m_nodeData.m_lerp2DNodeData.m_inputTL)
			&& validate(node.m_nodeData.m_lerp2DNodeData.m_inputTR)
			&& validate(node.m_nodeData.m_lerp2DNodeData.m_inputBL)
			&& validate(node.m_nodeData.m_lerp2DNodeData.m_inputBR);
	}
	default:
		assert(false);
		break;
	}
	return false;
}