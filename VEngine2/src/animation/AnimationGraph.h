#pragma once
#include "JointPose.h"
#include "AnimationClip.h"
#include "asset/AnimationClipAsset.h"
#include "asset/ScriptAsset.h"
#include "utility/StringID.h"
#include "ecs/ECSCommon.h"

struct lua_State;
struct AnimationGraphNode;
class ECS;
class AnimationGraph;

/// <summary>
/// Callback that is called before evaluating the AnimationGraph. This is the place to update AnimationGraphParameters.
/// TODO: replace with script.
/// </summary>
using AnimationGraphLogicCallback = void(*)(AnimationGraph *graph, ECS *ecs, EntityID entity, float deltaTime);

enum class AnimationGraphNodeType
{
	ANIM_CLIP,
	LERP,
	LERP_1D_ARRAY,
	LERP_2D
};

union AnimationGraphNodeData
{
	using ParameterIndex = size_t;
	using AnimationClipIndex = size_t;
	using NodeIndex = size_t;

	struct ClipNodeData
	{
		AnimationClipIndex m_animClip;
		ParameterIndex m_loop;
	};

	struct LerpNodeData
	{
		NodeIndex m_inputA;
		NodeIndex m_inputB;
		ParameterIndex m_alpha;
	};

	struct Lerp1DArrayNodeData
	{
		static constexpr size_t k_maxInputs = 8;
		NodeIndex m_inputs[k_maxInputs];
		float m_inputKeys[k_maxInputs];
		size_t m_inputCount;
		ParameterIndex m_alpha;
	};

	struct Lerp2DNodeData
	{
		NodeIndex m_inputTL;
		NodeIndex m_inputTR;
		NodeIndex m_inputBL;
		NodeIndex m_inputBR;
		ParameterIndex m_alphaX;
		ParameterIndex m_alphaY;
	};

	ClipNodeData m_clipNodeData;
	LerpNodeData m_lerpNodeData;
	Lerp1DArrayNodeData m_lerp1DArrayNodeData;
	Lerp2DNodeData m_lerp2DNodeData;
};

/// <summary>
/// A node in the directed acyclic AnimationGraph. The node is used to blend animation clip poses.
/// We deliberately did not use polymorphism here so that we can place the nodes in a linear array
/// and iterate over them during evaluation without having to call any virtual functions to objectes
/// scattered all over memory.
/// </summary>
struct AnimationGraphNode
{
	AnimationGraphNodeType m_nodeType;
	AnimationGraphNodeData m_nodeData;
};

/// <summary>
/// Named and tagged union of float, int32 and bool values that is used to control the behavior of
/// AnimationGraphNodes in the AnimationGraph. Can be set/get by their name.
/// </summary>
struct AnimationGraphParameter
{
	enum class Type
	{
		FLOAT,
		INT,
		BOOL
	};

	union Data
	{
		float f;
		int32_t i;
		bool b;
	};

	StringID m_name;
	Type m_type;
	Data m_data;
};

/// <summary>
/// Represents a directed acyclic graph of nodes for blending animation clip poses.
/// </summary>
/// <returns></returns>
class AnimationGraph
{
public:
	explicit AnimationGraph() = default;
	explicit AnimationGraph(
		size_t rootNodeIndex,
		size_t nodeCount, 
		const AnimationGraphNode *nodes, 
		size_t parameterCount, 
		const AnimationGraphParameter *parameters, 
		size_t animationClipCount, 
		const Asset<AnimationClipAsset> *animationClips,
		const Asset<ScriptAsset> &controllerScript) noexcept;
	AnimationGraph(const AnimationGraph &other) noexcept;
	AnimationGraph(AnimationGraph &&other) noexcept;
	AnimationGraph &operator=(const AnimationGraph &other) noexcept;
	AnimationGraph &operator=(AnimationGraph &&other) noexcept;
	~AnimationGraph() noexcept;
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
	size_t getNodeCount() const noexcept;
	const AnimationGraphNode *getNodes() const noexcept;
	size_t getParameterCount() const noexcept;
	const AnimationGraphParameter *getParameters() const noexcept;
	size_t getAnimationClipAssetCount() const noexcept;
	const Asset<AnimationClipAsset> *getAnimationClipAssets() const noexcept;
	Asset<ScriptAsset> getControllerScript() const noexcept;
	size_t getRootNodeIndex() const noexcept;
	bool isValid() const noexcept;
	bool isLoaded() noexcept;

private:
	size_t m_rootNodeIndex = -1;
	AnimationGraphNode *m_nodes = nullptr;
	size_t m_nodeCount = 0;
	AnimationGraphParameter *m_parameters = nullptr;
	size_t m_parameterCount = 0;
	Asset<AnimationClipAsset> *m_animationClipAssets = nullptr;
	size_t m_animationClipCount;
	Asset<ScriptAsset> m_controllerScript;
	lua_State *m_scriptLuaState = nullptr;
	float m_phase = 0.0f;
	bool m_isValid = false;
	bool m_allAssetsLoaded = false;

	JointPose evaluate(size_t jointIdx, const AnimationGraphNode &node) const noexcept;
	float evaluateDuration(const AnimationGraphNode &node) const noexcept;
	JointPose lerp(size_t jointIdx, const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept;
	float lerpDuration(const AnimationGraphNode &x, const AnimationGraphNode &y, float alpha) const noexcept;
	float getFloatParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept;
	int32_t getIntParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept;
	bool getBoolParam(AnimationGraphNodeData::ParameterIndex idx) const noexcept;
	bool validate(AnimationGraphNodeData::NodeIndex idx) const noexcept;
	bool checkAllAssetsLoaded() const noexcept;
	void reloadScript() noexcept;
};