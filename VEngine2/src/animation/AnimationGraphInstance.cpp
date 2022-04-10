#include "AnimationGraphInstance.h"
#include "script/LuaUtil.h"
#include "utility/Utility.h"
#include "ecs/ECS.h"
#include "ecs/ECSComponentInfoTable.h"
#include "ecs/ECSLua.h"
#include "animation/AnimationGraphInstanceLua.h"
#include "asset/AssetManager.h"
#include "Log.h"

AnimationGraphInstance::AnimationGraphInstance(const Asset<AnimationGraphAsset> &graphAsset) noexcept
	:m_graphAsset(graphAsset)
{
	if (m_graphAsset)
	{
		auto *graph = m_graphAsset->getAnimationGraph();
		m_rootNodeIndex = graph->getRootNodeIndex();
		m_nodeCount = graph->getNodeCount();
		m_parameterCount = graph->getParameterCount();
		m_animationClipCount = graph->getAnimationClipAssetCount();
		m_nodes = graph->getNodes();
		m_parameters.clear();
		m_parameters.insert(m_parameters.end(), graph->getParameters(), graph->getParameters() + m_parameterCount);
		m_animationClipAssets = graph->getAnimationClipAssets();
		m_controllerScript = graph->getControllerScript();
	}
}

AnimationGraphInstance::AnimationGraphInstance(const AnimationGraphInstance &other) noexcept
	:m_graphAsset(other.m_graphAsset),
	m_parameters(other.m_parameters)
{
	if (m_graphAsset)
	{
		auto *graph = m_graphAsset->getAnimationGraph();
		m_rootNodeIndex = graph->getRootNodeIndex();
		m_nodeCount = graph->getNodeCount();
		m_parameterCount = graph->getParameterCount();
		m_animationClipCount = graph->getAnimationClipAssetCount();
		m_nodes = graph->getNodes();
		m_animationClipAssets = graph->getAnimationClipAssets();
		m_controllerScript = graph->getControllerScript();
	}
}

AnimationGraphInstance::AnimationGraphInstance(AnimationGraphInstance &&other) noexcept
	:m_graphAsset(other.m_graphAsset),
	m_parameters(other.m_parameters),
	m_scriptLuaState(other.m_scriptLuaState)
{
	other.m_scriptLuaState = nullptr;

	if (m_graphAsset)
	{
		auto *graph = m_graphAsset->getAnimationGraph();
		m_rootNodeIndex = graph->getRootNodeIndex();
		m_nodeCount = graph->getNodeCount();
		m_parameterCount = graph->getParameterCount();
		m_animationClipCount = graph->getAnimationClipAssetCount();
		m_nodes = graph->getNodes();
		m_animationClipAssets = graph->getAnimationClipAssets();
		m_controllerScript = graph->getControllerScript();
	}
}

AnimationGraphInstance &AnimationGraphInstance::operator=(const AnimationGraphInstance &other) noexcept
{
	if (this != &other)
	{
		if (m_scriptLuaState)
		{
			lua_close(m_scriptLuaState);
			m_scriptLuaState = nullptr;
		}

		m_graphAsset = other.m_graphAsset;
		m_parameters = other.m_parameters;
		m_scriptLuaState = other.m_scriptLuaState;

		if (m_graphAsset)
		{
			auto *graph = m_graphAsset->getAnimationGraph();
			m_rootNodeIndex = graph->getRootNodeIndex();
			m_nodeCount = graph->getNodeCount();
			m_parameterCount = graph->getParameterCount();
			m_animationClipCount = graph->getAnimationClipAssetCount();
			m_nodes = graph->getNodes();
			m_animationClipAssets = graph->getAnimationClipAssets();
			m_controllerScript = graph->getControllerScript();
		}
	}

	return *this;
}

AnimationGraphInstance &AnimationGraphInstance::operator=(AnimationGraphInstance &&other) noexcept
{
	if (this != &other)
	{
		if (m_scriptLuaState)
		{
			lua_close(m_scriptLuaState);
			m_scriptLuaState = nullptr;
		}

		m_graphAsset = other.m_graphAsset;
		m_parameters = other.m_parameters;
		m_scriptLuaState = other.m_scriptLuaState;

		other.m_scriptLuaState = nullptr;

		if (m_graphAsset)
		{
			auto *graph = m_graphAsset->getAnimationGraph();
			m_rootNodeIndex = graph->getRootNodeIndex();
			m_nodeCount = graph->getNodeCount();
			m_parameterCount = graph->getParameterCount();
			m_animationClipCount = graph->getAnimationClipAssetCount();
			m_nodes = graph->getNodes();
			m_animationClipAssets = graph->getAnimationClipAssets();
			m_controllerScript = graph->getControllerScript();
		}
	}

	return *this;
}

AnimationGraphInstance::~AnimationGraphInstance() noexcept
{
	if (m_scriptLuaState)
	{
		lua_close(m_scriptLuaState);
		m_scriptLuaState = nullptr;
	}
}

Asset<AnimationGraphAsset> AnimationGraphInstance::getGraphAsset() const noexcept
{
	return m_graphAsset;
}

void AnimationGraphInstance::setGraphAsset(const Asset<AnimationGraphAsset> &graphAsset) noexcept
{
	if (m_graphAsset != graphAsset)
	{
		m_graphAsset = graphAsset;

		if (m_scriptLuaState)
		{
			lua_close(m_scriptLuaState);
			m_scriptLuaState = nullptr;
		}

		if (m_graphAsset)
		{
			auto *graph = m_graphAsset->getAnimationGraph();
			m_rootNodeIndex = graph->getRootNodeIndex();
			m_nodeCount = graph->getNodeCount();
			m_parameterCount = graph->getParameterCount();
			m_animationClipCount = graph->getAnimationClipAssetCount();
			m_nodes = graph->getNodes();
			m_animationClipAssets = graph->getAnimationClipAssets();
			m_controllerScript = graph->getControllerScript();
		}
	}
}

void AnimationGraphInstance::preEvaluate(ECS *ecs, EntityID entity, float deltaTime) noexcept
{
	// check if we can get a reloaded version if this asset
	if (m_controllerScript->isReloadedAssetAvailable())
	{
		if (m_scriptLuaState)
		{
			lua_close(m_scriptLuaState);
			m_scriptLuaState = nullptr;
		}
		m_controllerScript = AssetManager::get()->getAsset<ScriptAsset>(m_controllerScript->getAssetID());
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
		AnimationGraphInstanceLua::createInstance(L, this);

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
		Log::err("AnimationGraphInstance: Failed to execute controller script \"%s\" with error: %s", m_controllerScript->getAssetID().m_string, lua_tostring(L, lua_gettop(L)));
	}

	lua_settop(L, 1);

	//m_logicCallback(this, ecs, entity, deltaTime);

	//const auto &mc = ecs->getComponent<CharacterMovementComponent>(entity);
	//graph->setFloatParam(SID("speed"), glm::length(glm::vec2(mc->m_velocityX, mc->m_velocityZ)));
}

JointPose AnimationGraphInstance::evaluate(size_t jointIdx) const noexcept
{
	return m_graphAsset.isLoaded() && m_graphAsset->getAnimationGraph()->isValid() ? evaluate(jointIdx, m_nodes[m_rootNodeIndex]) : JointPose{};
}

void AnimationGraphInstance::updatePhase(float deltaTime) noexcept
{
	const float duration = evaluateDuration(m_nodes[m_rootNodeIndex]);
	m_phase += deltaTime / duration;

	if (m_phase > 1.0f || m_phase < 0.0f)
	{
		m_phase -= floorf(m_phase);
	}
}

void AnimationGraphInstance::setPhase(float phase) noexcept
{
	m_phase = phase;

	if (m_phase > 1.0f || m_phase < 0.0f)
	{
		m_phase -= floorf(m_phase);
	}
}

bool AnimationGraphInstance::setFloatParam(const StringID &paramName, float value) noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::Float)
			{
				m_parameters[i].m_data.f = value;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraphInstance::setIntParam(const StringID &paramName, int32_t value) noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::Int)
			{
				m_parameters[i].m_data.i = value;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraphInstance::setBoolParam(const StringID &paramName, bool value) noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::Bool)
			{
				m_parameters[i].m_data.b = value;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraphInstance::getFloatParam(const StringID &paramName, float *value) const noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::Float)
			{
				*value = m_parameters[i].m_data.f;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraphInstance::getIntParam(const StringID &paramName, int32_t *value) const noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::Int)
			{
				*value = m_parameters[i].m_data.i;
				return true;
			}
			return false;
		}
	}

	return false;
}

bool AnimationGraphInstance::getBoolParam(const StringID &paramName, bool *value) const noexcept
{
	for (size_t i = 0; i < m_parameterCount; ++i)
	{
		if (m_parameters[i].m_name == paramName)
		{
			if (m_parameters[i].m_type == AnimationGraphParameter::Type::Bool)
			{
				*value = m_parameters[i].m_data.b;
				return true;
			}
			return false;
		}
	}

	return false;
}

JointPose AnimationGraphInstance::evaluate(size_t jointIdx, const AnimationGraphNode &node) const noexcept
{
	switch (node.m_nodeType)
	{
	case AnimationGraphNodeType::AnimClip:
	{
		const auto nodeData = node.m_nodeData.m_clipNodeData;
		const auto *animClip = m_animationClipAssets[nodeData.m_animClip]->getAnimationClip();
		const float clipDuration = animClip->getDuration();
		return animClip->getJointPose(jointIdx, m_phase * clipDuration, getBoolParam(nodeData.m_loop), false);
	}
	case AnimationGraphNodeType::Lerp:
	{
		const auto nodeData = node.m_nodeData.m_lerpNodeData;
		return lerp(jointIdx, m_nodes[nodeData.m_inputA], m_nodes[nodeData.m_inputB], getFloatParam(nodeData.m_alpha));
	}
	case AnimationGraphNodeType::Lerp1DArray:
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
	case AnimationGraphNodeType::Lerp2D:
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

float AnimationGraphInstance::evaluateDuration(const AnimationGraphNode &node) const noexcept
{
	switch (node.m_nodeType)
	{
	case AnimationGraphNodeType::AnimClip:
	{
		const auto nodeData = node.m_nodeData.m_clipNodeData;
		const auto *animClip = m_animationClipAssets[nodeData.m_animClip]->getAnimationClip();
		return animClip->getDuration();
	}
	case AnimationGraphNodeType::Lerp:
	{
		const auto nodeData = node.m_nodeData.m_lerpNodeData;
		return lerpDuration(m_nodes[nodeData.m_inputA], m_nodes[nodeData.m_inputB], getFloatParam(nodeData.m_alpha));
	}
	case AnimationGraphNodeType::Lerp1DArray:
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
	case AnimationGraphNodeType::Lerp2D:
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

JointPose AnimationGraphInstance::lerp(size_t jointIdx, const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept
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

float AnimationGraphInstance::lerpDuration(const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept
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

float AnimationGraphInstance::getFloatParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	if (idx == -1)
	{
		return 0.0f;
	}
	assert(idx < m_parameterCount);
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::Float);
	return m_parameters[idx].m_data.f;
}

int32_t AnimationGraphInstance::getIntParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	if (idx == -1)
	{
		return 0;
	}
	assert(idx < m_parameterCount);
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::Int);
	return m_parameters[idx].m_data.i;
}

bool AnimationGraphInstance::getBoolParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept
{
	if (idx == -1)
	{
		return false;
	}
	assert(idx < m_parameterCount);
	assert(m_parameters[idx].m_type == AnimationGraphParameter::Type::Bool);
	return m_parameters[idx].m_data.b;
}

bool AnimationGraphInstance::checkAllAssetsLoaded() const noexcept
{
	return m_graphAsset.isLoaded();
}

void AnimationGraphInstance::reloadScript() noexcept
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
	AnimationGraphInstanceLua::open(L);

	if (luaL_dostring(L, m_controllerScript->getScriptString()) != LUA_OK)
	{
		Log::err("AnimationGraphInstance: Failed to load controller script \"%s\" with error: %s", m_controllerScript->getAssetID().m_string, lua_tostring(L, lua_gettop(L)));

		lua_close(L);
		L = nullptr;

		return;
	}

	// the script has returned a table with the script functions which we can call later
}