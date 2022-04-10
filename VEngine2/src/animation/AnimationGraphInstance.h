#pragma once
#include "ecs/ECSCommon.h"
#include "utility/StringID.h"
#include "JointPose.h"
#include "asset/AnimationGraphAsset.h"
#include <EASTL/vector.h>

struct lua_State;
class ECS;

class AnimationGraphInstance
{
public:
	AnimationGraphInstance() = default;
	explicit AnimationGraphInstance(const Asset<AnimationGraphAsset> &graphAsset) noexcept;
	AnimationGraphInstance(const AnimationGraphInstance &other) noexcept;
	AnimationGraphInstance(AnimationGraphInstance &&other) noexcept;
	AnimationGraphInstance &operator=(const AnimationGraphInstance &other) noexcept;
	AnimationGraphInstance &operator=(AnimationGraphInstance &&other) noexcept;
	~AnimationGraphInstance() noexcept;
	void setGraphAsset(const Asset<AnimationGraphAsset> &graphAsset) noexcept;
	Asset<AnimationGraphAsset> getGraphAsset() const noexcept;
	void preEvaluate(ECS *ecs, EntityID entity, float deltaTime) noexcept;
	JointPose evaluate(size_t jointIdx) const noexcept;
	void updatePhase(float deltaTime) noexcept;
	void setPhase(float phase) noexcept;
	bool setFloatParam(const StringID &paramName, float value) noexcept;
	bool setIntParam(const StringID &paramName, int32_t value) noexcept;
	bool setBoolParam(const StringID &paramName, bool value) noexcept;
	bool getFloatParam(const StringID &paramName, float *value) const noexcept;
	bool getIntParam(const StringID &paramName, int32_t *value) const noexcept;
	bool getBoolParam(const StringID &paramName, bool *value) const noexcept;

private:
	Asset<AnimationGraphAsset> m_graphAsset = {};
	uint32_t m_rootNodeIndex = -1;
	uint32_t m_nodeCount = 0;
	uint32_t m_parameterCount = 0;
	uint32_t m_animationClipCount = 0;
	const AnimationGraphNode *m_nodes = nullptr;
	eastl::vector<AnimationGraphParameter> m_parameters; // each AnimationGraphInstance has its own set of modifiable parameters
	const Asset<AnimationClipAsset> *m_animationClipAssets = nullptr;
	Asset<ScriptAsset> m_controllerScript = nullptr;
	lua_State *m_scriptLuaState = nullptr;
	float m_phase = 0.0f;

	JointPose evaluate(size_t jointIdx, const AnimationGraphNode &node) const noexcept;
	float evaluateDuration(const AnimationGraphNode &node) const noexcept;
	JointPose lerp(size_t jointIdx, const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept;
	float lerpDuration(const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept;
	float getFloatParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept;
	int32_t getIntParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept;
	bool getBoolParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept;
	bool checkAllAssetsLoaded() const noexcept;
	void reloadScript() noexcept;
};