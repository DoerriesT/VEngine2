#pragma once
#include "JointPose.h"
#include "asset/AnimationClipAsset.h"
#include "asset/ScriptAsset.h"
#include "utility/StringID.h"

enum class AnimationGraphNodeType : uint32_t
{
	AnimClip = 0,
	Lerp = 1,
	Lerp1DArray = 2,
	Lerp2D = 3
};

union AnimationGraphNodeData
{
	using ParameterIndex = uint32_t;
	using AnimationClipIndex = uint32_t;
	using NodeIndex = uint32_t;

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
		Float,
		Int,
		Bool
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

struct AnimationGraphCreateInfo
{
	uint32_t m_rootNodeIndex;
	uint32_t m_nodeCount;
	uint32_t m_parameterCount;
	uint32_t m_animationClipCount;
	const AnimationGraphNode *m_nodes;
	const AnimationGraphParameter *m_parameters;
	const Asset<AnimationClipAsset> *m_animationClips;
	Asset<ScriptAsset> m_controllerScript;
	const char *m_memory;
};

/// <summary>
/// Represents a directed acyclic graph of nodes for blending animation clip poses.
/// </summary>
class AnimationGraph
{
public:
	explicit AnimationGraph() = default;
	explicit AnimationGraph(const AnimationGraphCreateInfo &createInfo) noexcept;
	AnimationGraph(AnimationGraph &&other) noexcept;
	AnimationGraph &operator=(AnimationGraph &&other) noexcept;
	DELETED_COPY(AnimationGraph);
	~AnimationGraph() noexcept;
	uint32_t getNodeCount() const noexcept;
	uint32_t getParameterCount() const noexcept;
	uint32_t getAnimationClipAssetCount() const noexcept;
	const AnimationGraphNode *getNodes() const noexcept;
	const AnimationGraphParameter *getParameters() const noexcept;
	const Asset<AnimationClipAsset> *getAnimationClipAssets() const noexcept;
	Asset<ScriptAsset> getControllerScript() const noexcept;
	uint32_t getRootNodeIndex() const noexcept;
	bool isValid() const noexcept;

private:
	uint32_t m_rootNodeIndex = -1;
	uint32_t m_nodeCount = 0;
	uint32_t m_parameterCount = 0;
	uint32_t m_animationClipCount = 0;
	const AnimationGraphNode *m_nodes = nullptr;
	const AnimationGraphParameter *m_parameters = nullptr;
	const Asset<AnimationClipAsset> *m_animationClipAssets = nullptr;
	Asset<ScriptAsset> m_controllerScript = nullptr;
	const char *m_memory = nullptr;
	bool m_isValid = false;

	bool validate(AnimationGraphNodeData::NodeIndex idx) const noexcept;
};