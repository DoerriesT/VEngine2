#include "AnimationGraph.h"
#include "utility/Utility.h"
#include "ecs/ECS.h"
#include "script/LuaUtil.h"
#include "ecs/ECSComponentInfoTable.h"
#include "ecs/ECSLua.h"
#include "animation/AnimationGraphLua.h"
#include "Log.h"
#include "asset/AssetManager.h"

AnimationGraph::AnimationGraph(
	size_t rootNodeIndex,
	size_t nodeCount, 
	const AnimationGraphNode *nodes, 
	size_t parameterCount, 
	const AnimationGraphParameter *parameters, 
	size_t animationClipCount,
	const Asset<AnimationClipAssetData> *animationClips,
	const Asset<ScriptAssetData> &controllerScript
) noexcept
	:m_rootNodeIndex(rootNodeIndex),
	m_nodeCount(nodeCount),
	m_parameterCount(parameterCount),
	m_animationClipCount(animationClipCount),
	m_controllerScript(controllerScript)
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

	reloadScript();

	m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
	m_allAssetsLoaded = checkAllAssetsLoaded();
}

AnimationGraph::AnimationGraph(const AnimationGraph &other) noexcept
	:m_rootNodeIndex(other.m_rootNodeIndex),
	m_nodeCount(other.m_nodeCount),
	m_parameterCount(other.m_parameterCount),
	m_animationClipCount(other.m_animationClipCount),
	m_controllerScript(other.m_controllerScript)
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

	reloadScript();

	m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
	m_allAssetsLoaded = checkAllAssetsLoaded();
}

AnimationGraph::AnimationGraph(AnimationGraph &&other) noexcept
	:m_rootNodeIndex(other.m_rootNodeIndex),
	m_nodeCount(other.m_nodeCount),
	m_parameterCount(other.m_parameterCount),
	m_animationClipCount(other.m_animationClipCount),
	m_controllerScript(other.m_controllerScript)
{
	m_nodes = other.m_nodes;
	other.m_nodes = nullptr;

	m_parameters = other.m_parameters;
	other.m_parameters = nullptr;

	m_animationClipAssets = other.m_animationClipAssets;
	other.m_animationClipAssets = nullptr;

	reloadScript();

	m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
	m_allAssetsLoaded = checkAllAssetsLoaded();
}

AnimationGraph &AnimationGraph::operator=(const AnimationGraph &other) noexcept
{
	if (this != &other)
	{
		delete[] m_nodes;
		delete[] m_parameters;
		delete[] m_animationClipAssets;

		m_rootNodeIndex = other.m_rootNodeIndex;
		m_nodeCount = other.m_nodeCount;
		m_parameterCount = other.m_parameterCount;
		m_animationClipCount = other.m_animationClipCount;

		m_controllerScript = other.m_controllerScript;

		m_nodes = new AnimationGraphNode[m_nodeCount];
		memcpy(m_nodes, other.m_nodes, sizeof(m_nodes[0]) * m_nodeCount);
		m_parameters = new AnimationGraphParameter[m_parameterCount];
		memcpy(m_parameters, other.m_parameters, sizeof(m_parameters[0]) * m_parameterCount);
		
		m_animationClipAssets = new Asset<AnimationClipAssetData> [m_animationClipCount];
		for (size_t i = 0; i < m_animationClipCount; ++i)
		{
			m_animationClipAssets[i] = other.m_animationClipAssets[i];
		}

		reloadScript();

		m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
		m_allAssetsLoaded = checkAllAssetsLoaded();
	}

	return *this;
}

AnimationGraph &AnimationGraph::operator=(AnimationGraph &&other) noexcept
{
	if (this != &other)
	{
		delete[] m_nodes;
		delete[] m_parameters;
		delete[] m_animationClipAssets;

		m_rootNodeIndex = other.m_rootNodeIndex;
		m_nodeCount = other.m_nodeCount;
		m_parameterCount = other.m_parameterCount;
		m_animationClipCount = other.m_animationClipCount;

		m_controllerScript = other.m_controllerScript;

		m_nodes = other.m_nodes;
		other.m_nodes = nullptr;
		m_parameters = other.m_parameters;
		other.m_parameters = nullptr;
		m_animationClipAssets = other.m_animationClipAssets;
		other.m_animationClipAssets = nullptr;

		reloadScript();

		m_isValid = m_rootNodeIndex != -1 && validate(m_rootNodeIndex);
		m_allAssetsLoaded = checkAllAssetsLoaded();
	}

	return *this;
}

AnimationGraph::~AnimationGraph() noexcept
{
	delete[] m_nodes;
	delete[] m_parameters;
	delete[] m_animationClipAssets;
	if (m_scriptLuaState)
	{
		lua_close(m_scriptLuaState);
	}
}

void AnimationGraph::preEvaluate(ECS *ecs, EntityID entity, float deltaTime) noexcept
{
	// check if we can get a reloaded version if this asset
	if (m_controllerScript->isReloadedAssetAvailable())
	{
		if (m_scriptLuaState)
		{
			lua_close(m_scriptLuaState);
			m_scriptLuaState = nullptr;
		}
		m_controllerScript = AssetManager::get()->getAsset<ScriptAssetData>(m_controllerScript->getAssetID());
		m_allAssetsLoaded = !m_controllerScript.isLoaded() ? false : m_allAssetsLoaded;
	}

	if (!m_scriptLuaState)
	{
		reloadScript();
		if (!m_scriptLuaState)
		{
			return;
		}
	}
	auto &L = m_scriptLuaState;

	// the script has returned a table with the script functions
	// get the update() function
	lua_getfield(L, -1, "update");

	// create arguments (self, graph, ecs, entity, deltaTime)
	{
		// self
		lua_pushvalue(L, -2);

		// graph
		AnimationGraphLua::createInstance(L, this);

		// ecs
		ECSLua::createInstance(L, ecs);

		// entity
		lua_pushinteger(L, (lua_Integer)entity);

		// delta time
		lua_pushnumber(L, (lua_Number)deltaTime);
	}

	// call update() function
	if (lua_pcall(L, 5, 0, 0) != LUA_OK)
	{
		Log::err("AnimationGraph: Failed to execute controller script \"%s\" with error: %s", m_controllerScript->getAssetID().m_string, lua_tostring(L, lua_gettop(L)));
	}

	lua_settop(L, 1);

	//m_logicCallback(this, ecs, entity, deltaTime);

	//const auto &mc = ecs->getComponent<CharacterMovementComponent>(entity);
	//graph->setFloatParam(SID("speed"), glm::length(glm::vec2(mc->m_velocityX, mc->m_velocityZ)));
}

JointPose AnimationGraph::evaluate(size_t jointIdx) const noexcept
{
	return isValid() ? evaluate(jointIdx, m_nodes[m_rootNodeIndex]) : JointPose{};
}

void AnimationGraph::updatePhase(float deltaTime) noexcept
{
	const float duration = evaluateDuration(m_nodes[m_rootNodeIndex]);
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

size_t AnimationGraph::getAnimationClipAssetCount() const noexcept
{
	return m_animationClipCount;
}

const Asset<AnimationClipAssetData> *AnimationGraph::getAnimationClipAssets() const noexcept
{
	return m_animationClipAssets;
}

Asset<ScriptAssetData> AnimationGraph::getControllerScript() const noexcept
{
	return m_controllerScript;
}

size_t AnimationGraph::getRootNodeIndex() const noexcept
{
	return m_rootNodeIndex;
}

bool AnimationGraph::isValid() const noexcept
{
	return m_rootNodeIndex != -1 && m_isValid;
}

bool AnimationGraph::isLoaded() noexcept
{
	if (!m_allAssetsLoaded)
	{
		m_allAssetsLoaded = checkAllAssetsLoaded();
	}
	return m_allAssetsLoaded;
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
	if (idx == -1)
	{
		return 0.0f;
	}
	assert(idx < m_parameterCount);
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::FLOAT);
	return m_parameters[idx].m_data.f;
}

int32_t AnimationGraph::getIntParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	if (idx == -1)
	{
		return 0;
	}
	assert(idx < m_parameterCount);
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::INT);
	return m_parameters[idx].m_data.i;
}

bool AnimationGraph::getBoolParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	if (idx == -1)
	{
		return false;
	}
	assert(idx < m_parameterCount);
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::BOOL);
	return m_parameters[idx].m_data.b;
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
	case AnimationGraphNodeType::ANIM_CLIP:
	{
		return node.m_nodeData.m_clipNodeData.m_animClip != -1 && m_animationClipAssets[node.m_nodeData.m_clipNodeData.m_animClip].get() != nullptr;
	}
	case AnimationGraphNodeType::LERP:
	{
		return validate(node.m_nodeData.m_lerpNodeData.m_inputA) && validate(node.m_nodeData.m_lerpNodeData.m_inputB);
	}
	case AnimationGraphNodeType::LERP_1D_ARRAY:
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
	case AnimationGraphNodeType::LERP_2D:
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

bool AnimationGraph::checkAllAssetsLoaded() const noexcept
{
	assert(m_controllerScript);
	if (!m_controllerScript.isLoaded())
	{
		return false;
	}

	for (size_t i = 0; i < m_animationClipCount; ++i)
	{
		assert(m_animationClipAssets[i]);
		if (!m_animationClipAssets[i].isLoaded())
		{
			return false;
		}
	}

	return true;
}

void AnimationGraph::reloadScript() noexcept
{
	if (!m_controllerScript || !m_controllerScript.isLoaded())
	{
		return;
	}

	auto &L = m_scriptLuaState;
	if (L)
	{
		lua_close(L);
		L = nullptr;
	}

	L = luaL_newstate();
	luaL_openlibs(L);
	ECSLua::open(L);
	AnimationGraphLua::open(L);

	if (luaL_dostring(L, m_controllerScript->getScriptString()) != LUA_OK)
	{
		Log::err("AnimationGraph: Failed to load controller script \"%s\" with error: %s", m_controllerScript->getAssetID().m_string, lua_tostring(L, lua_gettop(L)));

		lua_close(L);
		L = nullptr;

		return;
	}

	// the script has returned a table with the script functions which we can call later
}
