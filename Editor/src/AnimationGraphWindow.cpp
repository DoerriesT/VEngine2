#include "AnimationGraphWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/imnodes.h>
#include <animation/AnimationGraph.h>
#include <utility/memory.h>
#include <input/InputTokens.h>
#include <Log.h>
#include <graphics/imgui/gui_helpers.h>
#include <asset/AssetManager.h>

template<typename T>
static T convertIdxToPtr(size_t idx, const eastl::vector<T> &v)
{
	if (idx == -1 || idx >= v.size())
	{
		return nullptr;
	}
	else
	{
		return v[idx];
	}
};

template<typename T>
static size_t convertPtrToIdx(T ptr, const eastl::vector<T> &v)
{
	if (!ptr)
	{
		return -1;
	}
	else
	{
		auto it = eastl::find(v.begin(), v.end(), ptr);
		return it != v.end() ? (it - v.begin()) : -1;
	}
};

static const char *k_nodeTypeNames[]{ "Animation Clip", "Lerp", "Lerp 1D Array", "Lerp 2D" };

struct AnimationGraphEditorNode
{
	int m_nodeID;
	int m_outputPinID;
	int m_inputPinIDs[8];

	virtual void setFromRuntimeData(AnimationGraphWindow *window, const AnimationGraphNode &runtimeNode) noexcept = 0;
	virtual AnimationGraphNode createRuntimeData(AnimationGraphWindow *window) const noexcept = 0;
	virtual void draw(AnimationGraphWindow *window) noexcept = 0;
	virtual void computeNodePosition(int treeDepth, int siblingIndex) const noexcept = 0;
	virtual size_t getParameterReferenceCount(const AnimationGraphEditorParam *param) const noexcept = 0;
	virtual void deleteParameter(const AnimationGraphEditorParam *param) noexcept = 0;
	virtual size_t getAnimationClipReferenceCount(const Asset<AnimationClipAsset> *animClip) const noexcept { return 0; }
	virtual void deleteAnimationClip(const Asset<AnimationClipAsset> *animClip) noexcept {}
	virtual void clearInputPin(size_t pinIndex) noexcept = 0;
	virtual void setInputPin(size_t pinIndex, AnimationGraphEditorNode *inputNode) noexcept = 0;
	virtual bool findLoop(const AnimationGraphEditorNode *searchNode) const noexcept = 0;
};

struct AnimationGraphAnimClipEditorNode : AnimationGraphEditorNode
{
	Asset<AnimationClipAsset> *m_animClip;
	AnimationGraphEditorParam *m_loopParam;

	void setFromRuntimeData(AnimationGraphWindow *window, const AnimationGraphNode &runtimeNode) noexcept override
	{
		assert(runtimeNode.m_nodeType == AnimationGraphNodeType::ANIM_CLIP);
		m_animClip = convertIdxToPtr(runtimeNode.m_nodeData.m_clipNodeData.m_animClip, window->m_animClips);
		m_loopParam = convertIdxToPtr(runtimeNode.m_nodeData.m_clipNodeData.m_loop, window->m_params);
	}

	AnimationGraphNode createRuntimeData(AnimationGraphWindow *window) const noexcept
	{
		AnimationGraphNode result{};
		result.m_nodeType = AnimationGraphNodeType::ANIM_CLIP;
		result.m_nodeData.m_clipNodeData.m_animClip = convertPtrToIdx(m_animClip, window->m_animClips);
		result.m_nodeData.m_clipNodeData.m_loop = convertPtrToIdx(m_loopParam, window->m_params);
		return result;
	}

	void draw(AnimationGraphWindow *window) noexcept override
	{
		ImNodes::BeginNode(m_nodeID);
		{
			// title bar
			ImNodes::BeginNodeTitleBar();
			ImGui::TextUnformatted("Animation Clip");
			ImNodes::EndNodeTitleBar();

			// pins
			ImNodes::BeginOutputAttribute(m_outputPinID);
			ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
			ImGui::TextUnformatted("Out");
			ImNodes::EndOutputAttribute();

			auto *curClip = m_animClip;
			auto *newClip = curClip;

			const char *curClipName = curClip ? (curClip->get() ? curClip->get()->getAssetID().m_string : "<No Asset>") : "<Empty>";

			ImGui::PushItemWidth(200.0f);
			if (ImGui::BeginCombo("Clip", curClipName))
			{
				for (size_t i = 0; i < window->m_animClips.size(); ++i)
				{
					bool selected = window->m_animClips[i] == curClip;
					if (ImGui::Selectable(window->m_animClips[i]->get() ? (*window->m_animClips[i])->getAssetID().m_string : "<No Asset>", selected))
					{
						newClip = window->m_animClips[i];
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			m_animClip = newClip;

			window->drawParamComboBox("Loop", m_loopParam, AnimationGraphParameter::Type::BOOL);
		}
		ImNodes::EndNode();
	}

	void computeNodePosition(int treeDepth, int siblingIndex) const noexcept override
	{
		ImVec2 origin = ImNodes::GetNodeGridSpacePos(AnimationGraphWindow::k_rootNodeID);
		ImNodes::SetNodeGridSpacePos(m_nodeID, ImVec2(-treeDepth * 450.0f + origin.x, siblingIndex * 150.0f + origin.y));
	}

	size_t getParameterReferenceCount(const AnimationGraphEditorParam *param) const noexcept override
	{
		return (param == m_loopParam) ? 1 : 0;
	}

	void deleteParameter(const AnimationGraphEditorParam *param) noexcept override
	{
		if (param == m_loopParam)
		{
			m_loopParam = nullptr;
		}
	}

	size_t getAnimationClipReferenceCount(const Asset<AnimationClipAsset> *animClip) const noexcept override
	{
		return (animClip == m_animClip) ? 1 : 0;
	}

	void deleteAnimationClip(const Asset<AnimationClipAsset> *animClip) noexcept override
	{
		if (animClip == m_animClip)
		{
			m_animClip = nullptr;
		}
	}

	void clearInputPin(size_t pinIndex) noexcept override
	{
		// this node has no inputs
		assert(false);
	}

	void setInputPin(size_t pinIndex, AnimationGraphEditorNode *inputNode) noexcept override
	{
		// this node has no inputs
		assert(false);
	}

	bool findLoop(const AnimationGraphEditorNode *searchNode) const noexcept override
	{
		return false;
	}
};

struct AnimationGraphLerpEditorNode : AnimationGraphEditorNode
{
	AnimationGraphEditorNode *m_inputA;
	AnimationGraphEditorNode *m_inputB;
	AnimationGraphEditorParam *m_alphaParam;

	void setFromRuntimeData(AnimationGraphWindow *window, const AnimationGraphNode &runtimeNode) noexcept override
	{
		assert(runtimeNode.m_nodeType == AnimationGraphNodeType::LERP);
		m_inputA = convertIdxToPtr(runtimeNode.m_nodeData.m_lerpNodeData.m_inputA, window->m_nodes);
		m_inputB = convertIdxToPtr(runtimeNode.m_nodeData.m_lerpNodeData.m_inputB, window->m_nodes);
		m_alphaParam = convertIdxToPtr(runtimeNode.m_nodeData.m_lerpNodeData.m_alpha, window->m_params);

		window->createVisualLink(m_inputA, m_inputPinIDs[0], m_nodeID);
		window->createVisualLink(m_inputB, m_inputPinIDs[1], m_nodeID);
	}

	AnimationGraphNode createRuntimeData(AnimationGraphWindow *window) const noexcept
	{
		AnimationGraphNode result{};
		result.m_nodeType = AnimationGraphNodeType::LERP;
		result.m_nodeData.m_lerpNodeData.m_inputA = convertPtrToIdx(m_inputA, window->m_nodes);
		result.m_nodeData.m_lerpNodeData.m_inputB = convertPtrToIdx(m_inputB, window->m_nodes);
		result.m_nodeData.m_lerpNodeData.m_alpha = convertPtrToIdx(m_alphaParam, window->m_params);
		return result;
	}

	void draw(AnimationGraphWindow *window) noexcept override
	{
		ImNodes::BeginNode(m_nodeID);
		{
			// title bar
			ImNodes::BeginNodeTitleBar();
			ImGui::TextUnformatted("Lerp");
			ImNodes::EndNodeTitleBar();

			// pins
			ImNodes::BeginOutputAttribute(m_outputPinID);
			ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
			ImGui::TextUnformatted("Out");
			ImNodes::EndOutputAttribute();

			ImNodes::BeginInputAttribute(m_inputPinIDs[0]);
			ImGui::TextUnformatted("X");
			ImNodes::EndInputAttribute();
			ImNodes::BeginInputAttribute(m_inputPinIDs[1]);
			ImGui::TextUnformatted("Y");
			ImNodes::EndInputAttribute();

			window->drawParamComboBox("Alpha", m_alphaParam, AnimationGraphParameter::Type::FLOAT);
		}
		ImNodes::EndNode();
	}

	void computeNodePosition(int treeDepth, int siblingIndex) const noexcept override
	{
		ImVec2 origin = ImNodes::GetNodeGridSpacePos(AnimationGraphWindow::k_rootNodeID);
		ImNodes::SetNodeGridSpacePos(m_nodeID, ImVec2(-treeDepth * 450.0f + origin.x, siblingIndex * 150.0f + origin.y));

		assert(m_inputA && m_inputB);
		m_inputA->computeNodePosition(treeDepth + 1, siblingIndex);
		m_inputB->computeNodePosition(treeDepth + 1, siblingIndex + 1);
	}

	size_t getParameterReferenceCount(const AnimationGraphEditorParam *param) const noexcept override
	{
		return (param == m_alphaParam) ? 1 : 0;
	}

	void deleteParameter(const AnimationGraphEditorParam *param) noexcept override
	{
		if (param == m_alphaParam)
		{
			m_alphaParam = nullptr;
		}
	}

	void clearInputPin(size_t pinIndex) noexcept override
	{
		assert(pinIndex < 2);
		if (pinIndex == 0)
		{
			m_inputA = nullptr;
		}
		else
		{
			m_inputB = nullptr;
		}
	}

	void setInputPin(size_t pinIndex, AnimationGraphEditorNode *inputNode) noexcept override
	{
		assert(pinIndex < 2);
		if (pinIndex == 0)
		{
			m_inputA = inputNode;
		}
		else
		{
			m_inputB = inputNode;
		}
	}

	bool findLoop(const AnimationGraphEditorNode *searchNode) const noexcept override
	{
		assert(searchNode);
		if (searchNode == m_inputA || searchNode == m_inputB)
		{
			return true;
		}

		return (m_inputA && m_inputA->findLoop(searchNode)) || (m_inputB && m_inputB->findLoop(searchNode));
	}
};

struct AnimationGraphLerp1DArrayEditorNode : AnimationGraphEditorNode
{
	static constexpr size_t k_maxInputs = AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs;
	AnimationGraphEditorNode *m_inputs[k_maxInputs];
	float m_inputKeys[k_maxInputs];
	size_t m_inputCount;
	AnimationGraphEditorParam *m_alphaParam;

	void setFromRuntimeData(AnimationGraphWindow *window, const AnimationGraphNode &runtimeNode) noexcept override
	{
		assert(runtimeNode.m_nodeType == AnimationGraphNodeType::LERP_1D_ARRAY);
		m_inputCount = runtimeNode.m_nodeData.m_lerp1DArrayNodeData.m_inputCount;

		for (size_t i = 0; i < k_maxInputs; ++i)
		{
			if (i < m_inputCount)
			{
				m_inputs[i] = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp1DArrayNodeData.m_inputs[i], window->m_nodes);
				m_inputKeys[i] = runtimeNode.m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i];
				window->createVisualLink(m_inputs[i], m_inputPinIDs[i], m_nodeID);
			}
		}
		m_alphaParam = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp1DArrayNodeData.m_alpha, window->m_params);
	}

	AnimationGraphNode createRuntimeData(AnimationGraphWindow *window) const noexcept
	{
		AnimationGraphNode result{};
		result.m_nodeType = AnimationGraphNodeType::LERP_1D_ARRAY;

		for (size_t i = 0; i < m_inputCount; ++i)
		{
			result.m_nodeData.m_lerp1DArrayNodeData.m_inputs[i] = convertPtrToIdx(m_inputs[i], window->m_nodes);
			result.m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i] = m_inputKeys[i];
		}

		result.m_nodeData.m_lerp1DArrayNodeData.m_inputCount = m_inputCount;
		result.m_nodeData.m_lerp1DArrayNodeData.m_alpha = convertPtrToIdx(m_alphaParam, window->m_params);

		return result;
	}

	void draw(AnimationGraphWindow *window) noexcept override
	{
		ImNodes::BeginNode(m_nodeID);
		{
			// title bar
			ImNodes::BeginNodeTitleBar();
			ImGui::TextUnformatted("Lerp 1D Array");
			ImNodes::EndNodeTitleBar();

			// pins
			ImNodes::BeginOutputAttribute(m_outputPinID);
			ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
			ImGui::TextUnformatted("Out");
			ImNodes::EndOutputAttribute();

			const auto inputCount = m_inputCount;

			float prevMaxKey = -FLT_MAX;

			for (size_t i = 0; i < AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs; ++i)
			{
				if (i < inputCount)
				{
					ImNodes::BeginInputAttribute(m_inputPinIDs[i]);
					ImGui::Text("%i", (int)i);
					ImNodes::EndInputAttribute();
				}
				else
				{
					ImGui::Text("%i", (int)i);
				}

				ImGui::SameLine();

				ImGui::PushID(&m_inputKeys[i]);
				ImGui::PushItemWidth(200.0f);
				if (ImGui::InputFloat("", &m_inputKeys[i]))
				{
					m_inputKeys[i] = fmaxf(m_inputKeys[i], prevMaxKey);
				}
				ImGui::PopItemWidth();
				ImGui::PopID();

				prevMaxKey = m_inputKeys[i];
			}

			window->drawParamComboBox("Alpha", m_alphaParam, AnimationGraphParameter::Type::FLOAT);

			int newInputCount = (int)inputCount;
			ImGui::PushItemWidth(200.0f);
			ImGui::InputInt("Input Count", &newInputCount);
			ImGui::PopItemWidth();
			newInputCount = newInputCount < 0 ? 0 : newInputCount;
			newInputCount = newInputCount > k_maxInputs ? k_maxInputs : newInputCount;

			if (newInputCount > inputCount)
			{
				const float newKeyVal = inputCount == 0 ? 0.0f : m_inputKeys[inputCount - 1];
				for (size_t i = inputCount; i < newInputCount; ++i)
				{
					m_inputKeys[i] = newKeyVal;
				}
			}
			else if (newInputCount < inputCount)
			{
				for (size_t i = newInputCount; i < inputCount; ++i)
				{
					const int sourceOutPinID = m_inputs[i]->m_outputPinID;
					window->m_links.erase(eastl::remove_if(window->m_links.begin(), window->m_links.end(), [&](const auto &link)
						{
							return link.m_fromPinID == sourceOutPinID && link.m_toPinID == m_inputPinIDs[i];
						}), window->m_links.end());
					m_inputs[i] = nullptr;
				}
			}

			m_inputCount = (size_t)newInputCount;
		}
		ImNodes::EndNode();
	}

	void computeNodePosition(int treeDepth, int siblingIndex) const noexcept override
	{
		ImVec2 origin = ImNodes::GetNodeGridSpacePos(AnimationGraphWindow::k_rootNodeID);
		ImNodes::SetNodeGridSpacePos(m_nodeID, ImVec2(-treeDepth * 450.0f + origin.x, siblingIndex * 150.0f + origin.y));

		for (size_t i = 0; i < m_inputCount; ++i)
		{
			assert(m_inputs[i]);
			m_inputs[i]->computeNodePosition(treeDepth + 1, siblingIndex + (int)i);
		}
	}

	size_t getParameterReferenceCount(const AnimationGraphEditorParam *param) const noexcept override
	{
		return (param == m_alphaParam) ? 1 : 0;
	}

	void deleteParameter(const AnimationGraphEditorParam *param) noexcept override
	{
		if (param == m_alphaParam)
		{
			m_alphaParam = nullptr;
		}
	}

	void clearInputPin(size_t pinIndex) noexcept override
	{
		assert(pinIndex < m_inputCount);
		m_inputs[pinIndex] = nullptr;
	}

	void setInputPin(size_t pinIndex, AnimationGraphEditorNode *inputNode) noexcept override
	{
		assert(pinIndex < m_inputCount);
		m_inputs[pinIndex] = inputNode;
	}

	bool findLoop(const AnimationGraphEditorNode *searchNode) const noexcept override
	{
		assert(searchNode);

		for (size_t i = 0; i < m_inputCount; ++i)
		{
			if (searchNode == m_inputs[i])
			{
				return true;
			}
		}

		for (size_t i = 0; i < m_inputCount; ++i)
		{
			if (m_inputs[i] && m_inputs[i]->findLoop(searchNode))
			{
				return true;
			}
		}

		return false;
	}
};

struct AnimationGraphLerp2DEditorNode : AnimationGraphEditorNode
{
	AnimationGraphEditorNode *m_inputTL;
	AnimationGraphEditorNode *m_inputTR;
	AnimationGraphEditorNode *m_inputBL;
	AnimationGraphEditorNode *m_inputBR;
	AnimationGraphEditorParam *m_alphaXParam;
	AnimationGraphEditorParam *m_alphaYParam;

	void setFromRuntimeData(AnimationGraphWindow *window, const AnimationGraphNode &runtimeNode) noexcept override
	{
		assert(runtimeNode.m_nodeType == AnimationGraphNodeType::LERP_2D);
		m_inputTL = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp2DNodeData.m_inputTL, window->m_nodes);
		m_inputTR = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp2DNodeData.m_inputTR, window->m_nodes);
		m_inputBL = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp2DNodeData.m_inputBL, window->m_nodes);
		m_inputBR = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp2DNodeData.m_inputBR, window->m_nodes);
		m_alphaXParam = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp2DNodeData.m_alphaX, window->m_params);
		m_alphaYParam = convertIdxToPtr(runtimeNode.m_nodeData.m_lerp2DNodeData.m_alphaY, window->m_params);

		window->createVisualLink(m_inputTL, m_inputPinIDs[0], m_nodeID);
		window->createVisualLink(m_inputTR, m_inputPinIDs[1], m_nodeID);
		window->createVisualLink(m_inputBL, m_inputPinIDs[2], m_nodeID);
		window->createVisualLink(m_inputBR, m_inputPinIDs[3], m_nodeID);
	}

	AnimationGraphNode createRuntimeData(AnimationGraphWindow *window) const noexcept
	{
		AnimationGraphNode result{};
		result.m_nodeType = AnimationGraphNodeType::LERP_2D;
		result.m_nodeData.m_lerp2DNodeData.m_inputTL = convertPtrToIdx(m_inputTL, window->m_nodes);
		result.m_nodeData.m_lerp2DNodeData.m_inputTR = convertPtrToIdx(m_inputTR, window->m_nodes);
		result.m_nodeData.m_lerp2DNodeData.m_inputBL = convertPtrToIdx(m_inputBL, window->m_nodes);
		result.m_nodeData.m_lerp2DNodeData.m_inputBR = convertPtrToIdx(m_inputBR, window->m_nodes);
		result.m_nodeData.m_lerp2DNodeData.m_alphaX = convertPtrToIdx(m_alphaXParam, window->m_params);
		result.m_nodeData.m_lerp2DNodeData.m_alphaY = convertPtrToIdx(m_alphaYParam, window->m_params);
		return result;
	}

	void draw(AnimationGraphWindow *window) noexcept override
	{
		ImNodes::BeginNode(m_nodeID);
		{
			// title bar
			ImNodes::BeginNodeTitleBar();
			ImGui::TextUnformatted("Lerp 2D");
			ImNodes::EndNodeTitleBar();

			// pins
			ImNodes::BeginOutputAttribute(m_outputPinID);
			ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
			ImGui::TextUnformatted("Out");
			ImNodes::EndOutputAttribute();

			ImNodes::BeginInputAttribute(m_inputPinIDs[0]);
			ImGui::TextUnformatted("TL");
			ImNodes::EndInputAttribute();
			ImNodes::BeginInputAttribute(m_inputPinIDs[1]);
			ImGui::TextUnformatted("TR");
			ImNodes::EndInputAttribute();
			ImNodes::BeginInputAttribute(m_inputPinIDs[2]);
			ImGui::TextUnformatted("BL");
			ImNodes::EndInputAttribute();
			ImNodes::BeginInputAttribute(m_inputPinIDs[3]);
			ImGui::TextUnformatted("BR");
			ImNodes::EndInputAttribute();

			window->drawParamComboBox("Alpha X", m_alphaXParam, AnimationGraphParameter::Type::FLOAT);
			window->drawParamComboBox("Alpha Y", m_alphaYParam, AnimationGraphParameter::Type::FLOAT);
		}
		ImNodes::EndNode();
	}

	void computeNodePosition(int treeDepth, int siblingIndex) const noexcept override
	{
		ImVec2 origin = ImNodes::GetNodeGridSpacePos(AnimationGraphWindow::k_rootNodeID);
		ImNodes::SetNodeGridSpacePos(m_nodeID, ImVec2(-treeDepth * 450.0f + origin.x, siblingIndex * 150.0f + origin.y));

		m_inputTL->computeNodePosition(treeDepth + 1, siblingIndex);
		m_inputTR->computeNodePosition(treeDepth + 1, siblingIndex + 1);
		m_inputBL->computeNodePosition(treeDepth + 1, siblingIndex + 2);
		m_inputBR->computeNodePosition(treeDepth + 1, siblingIndex + 3);
	}

	size_t getParameterReferenceCount(const AnimationGraphEditorParam *param) const noexcept override
	{
		size_t count = 0;
		count = (param == m_alphaXParam) ? (count + 1) : count;
		count = (param == m_alphaYParam) ? (count + 1) : count;
		return count;
	}

	void deleteParameter(const AnimationGraphEditorParam *param) noexcept override
	{
		if (param == m_alphaXParam)
		{
			m_alphaXParam = nullptr;
		}
		if (param == m_alphaYParam)
		{
			m_alphaYParam = nullptr;
		}
	}

	void clearInputPin(size_t pinIndex) noexcept override
	{
		switch (pinIndex)
		{
		case 0: m_inputTL = nullptr; break;
		case 1: m_inputTR = nullptr; break;
		case 2: m_inputBL = nullptr; break;
		case 3: m_inputBR = nullptr; break;
		default:
			assert(false);
		}
	}

	void setInputPin(size_t pinIndex, AnimationGraphEditorNode *inputNode) noexcept override
	{
		switch (pinIndex)
		{
		case 0: m_inputTL = inputNode; break;
		case 1: m_inputTR = inputNode; break;
		case 2: m_inputBL = inputNode; break;
		case 3: m_inputBR = inputNode; break;
		default:
			assert(false);
		}
	}

	bool findLoop(const AnimationGraphEditorNode *searchNode) const noexcept override
	{
		assert(searchNode);
		if (searchNode == m_inputTL
			|| searchNode == m_inputTR
			|| searchNode == m_inputBL
			|| searchNode == m_inputBR
			)
		{
			return true;
		}

		return (m_inputTL && m_inputTL->findLoop(searchNode))
			|| (m_inputTR && m_inputTR->findLoop(searchNode))
			|| (m_inputBL && m_inputBL->findLoop(searchNode))
			|| (m_inputBR && m_inputBR->findLoop(searchNode));
	}
};

struct AnimationGraphEditorParam
{
	char m_nameScratchMem[256];
	AnimationGraphParameter m_param;
};

AnimationGraphWindow::AnimationGraphWindow(Engine *engine) noexcept
	:m_engine(engine)
{
}

AnimationGraphWindow::~AnimationGraphWindow() noexcept
{
	for (auto *p : m_params)
	{
		delete p;
	}
	m_params.clear();

	for (auto *c : m_animClips)
	{
		delete c;
	}
	m_animClips.clear();

	for (auto *n : m_nodes)
	{
		delete n;
	}
	m_nodes.clear();
	m_links.clear();
}

void AnimationGraphWindow::draw(AnimationGraph *graph) noexcept
{
	if (!m_visible)
	{
		return;
	}

	bool newGraph = graph != m_graph;
	m_graph = graph;
	if (newGraph)
	{
		importGraph(graph);
	}

	if (ImGui::Begin("Animation Graph"))
	{
		if (ImGui::Button("Save") && m_graph)
		{
			exportGraph(m_graph);
		}

		if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("##detail_view", ImGuiTableColumnFlags_WidthFixed, 125.0f);
			ImGui::TableSetupColumn("##node_editor", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			// detail view
			{
				ImGui::TableSetColumnIndex(0);
				ImGui::BeginChild("##Params");
				{
					// controller script
					{
						AssetID resultAssetID;
						if (ImGuiHelpers::AssetPicker("Script Asset", ScriptAsset::k_assetType, m_controllerScriptAsset.get(), &resultAssetID))
						{
							m_controllerScriptAsset = AssetManager::get()->getAsset<ScriptAsset>(resultAssetID);
						}
					}

					if (ImGui::BeginTabBar("ParamsClipsTabBar", ImGuiTabBarFlags_None))
					{
						if (ImGui::BeginTabItem("Parameters"))
						{
							drawParams();
							ImGui::EndTabItem();
						}
						if (ImGui::BeginTabItem("Animation Clips"))
						{
							drawAnimationClips();
							ImGui::EndTabItem();
						}
						ImGui::EndTabBar();
					}
				}
				ImGui::EndChild();
			}

			// node editor
			{
				ImGui::TableSetColumnIndex(1);
				ImGui::BeginChild("##editor");
				drawNodeEditor();
				ImGui::EndChild();
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void AnimationGraphWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool AnimationGraphWindow::isVisible() const noexcept
{
	return m_visible;
}

void AnimationGraphWindow::importGraph(AnimationGraph *graph) noexcept
{
	for (auto *p : m_params)
	{
		delete p;
	}
	m_params.clear();

	for (auto *c : m_animClips)
	{
		delete c;
	}
	m_animClips.clear();

	for (auto *n : m_nodes)
	{
		delete n;
	}
	m_nodes.clear();
	m_links.clear();
	m_controllerScriptAsset.release();
	m_nextID = k_firstID;

	if (graph)
	{
		// copy params
		const auto paramCount = graph->getParameterCount();
		const auto *params = graph->getParameters();
		for (size_t i = 0; i < paramCount; ++i)
		{
			AnimationGraphEditorParam param;
			strcpy_s(param.m_nameScratchMem, params[i].m_name.m_string);
			param.m_param = params[i];
			m_params.push_back(new AnimationGraphEditorParam(param));
		}

		// copy anim clip assets
		const auto clipCount = graph->getAnimationClipAssetCount();
		const auto *clips = graph->getAnimationClipAssets();
		for (size_t i = 0; i < clipCount; ++i)
		{
			m_animClips.push_back(new Asset<AnimationClipAsset>(clips[i]));
		}

		m_controllerScriptAsset = graph->getControllerScript();

		// create editor nodes
		const auto nodeCount = graph->getNodeCount();
		const auto *nodes = graph->getNodes();
		for (size_t i = 0; i < nodeCount; ++i)
		{
			AnimationGraphEditorNode *editorNode = nullptr;

			switch (nodes[i].m_nodeType)
			{
			case AnimationGraphNodeType::ANIM_CLIP:
				editorNode = new AnimationGraphAnimClipEditorNode(); break;
			case AnimationGraphNodeType::LERP:
				editorNode = new AnimationGraphLerpEditorNode(); break;
			case AnimationGraphNodeType::LERP_1D_ARRAY:
				editorNode = new AnimationGraphLerp1DArrayEditorNode(); break;
			case AnimationGraphNodeType::LERP_2D:
				editorNode = new AnimationGraphLerp2DEditorNode(); break;
			default:
				assert(false);
				break;
			}

			editorNode->m_nodeID = m_nextID++;
			editorNode->m_outputPinID = m_nextID++;
			for (auto &pid : editorNode->m_inputPinIDs)
			{
				pid = m_nextID++;
			}

			m_nodes.push_back(editorNode);
		}

		if (graph->getRootNodeIndex() != -1)
		{
			createVisualLink(m_nodes[graph->getRootNodeIndex()], k_rootNodeInputPinID, k_rootNodeID);
		}

		// translate indices in nodes to pointers and set up links
		for (size_t i = 0; i < nodeCount; ++i)
		{
			m_nodes[i]->setFromRuntimeData(this, nodes[i]);
		}

		ImNodes::SetNodeEditorSpacePos(k_rootNodeID, ImVec2(ImGui::GetContentRegionAvail().x * 0.75f, ImGui::GetContentRegionAvail().y * 0.5f));
		ImNodes::SetNodeDraggable(k_rootNodeID, false);
		if (!m_nodes.empty())
		{
			m_nodes[graph->getRootNodeIndex()]->computeNodePosition(1, 0);
		}
	}
}

void AnimationGraphWindow::exportGraph(AnimationGraph *graph) noexcept
{
	if (graph)
	{
		eastl::vector<AnimationGraphParameter> params;
		params.reserve(m_params.size());
		for (size_t i = 0; i < m_params.size(); ++i)
		{
			params.push_back(m_params[i]->m_param);
		}

		eastl::vector<Asset<AnimationClipAsset>> clips;
		clips.reserve(m_animClips.size());
		for (size_t i = 0; i < m_animClips.size(); ++i)
		{
			clips.push_back(*m_animClips[i]);
		}

		eastl::vector<AnimationGraphNode> nodes;
		nodes.reserve(m_nodes.size());
		for (size_t i = 0; i < m_nodes.size(); ++i)
		{
			nodes.push_back(m_nodes[i]->createRuntimeData(this));
		}

		size_t rootNodeIdx = -1;

		// find root node
		if (!m_links.empty())
		{
			auto it = eastl::find_if(m_nodes.begin(), m_nodes.end(), [this](auto *n) {return m_links[0].m_fromPinID == n->m_outputPinID; });
			if (it != m_nodes.end())
			{
				rootNodeIdx = it - m_nodes.begin();
			}
		}

		*graph = AnimationGraph(rootNodeIdx, nodes.size(), nodes.data(), params.size(), params.data(), clips.size(), clips.data(), m_controllerScriptAsset);
	}
}

void AnimationGraphWindow::drawNodeEditor() noexcept
{
	ImNodes::BeginNodeEditor();
	{
		// root node
		ImNodes::BeginNode(k_rootNodeID);
		{
			// title bar
			ImNodes::BeginNodeTitleBar();
			ImGui::TextUnformatted("Output");
			ImNodes::EndNodeTitleBar();

			// pins
			ImNodes::BeginInputAttribute(k_rootNodeInputPinID);
			ImGui::Text("Final Pose");
			ImNodes::EndInputAttribute();

			ImGui::Dummy(ImVec2(100.0f, 1.0f));
		}
		ImNodes::EndNode();

		for (auto *n : m_nodes)
		{
			n->draw(this);
		}

		for (const auto &l : m_links)
		{
			ImNodes::Link(l.m_linkID, l.m_fromPinID, l.m_toPinID);
		}

		ImNodes::MiniMap();
	}
	ImNodes::EndNodeEditor();

	// destroy links
	{
		int destroyedLinkID = 0;
		if (ImNodes::IsLinkDestroyed(&destroyedLinkID))
		{
			destroyLink(destroyedLinkID);
		}

		const auto selectedLinkCount = ImNodes::NumSelectedLinks();
		if (ImGui::IsKeyDown((int)InputKey::DELETE) && selectedLinkCount > 0)
		{
			int *selectedLinkIDs = ALLOC_A_T(int, selectedLinkCount);
			ImNodes::GetSelectedLinks(selectedLinkIDs);

			for (size_t i = 0; i < selectedLinkCount; ++i)
			{
				destroyLink(selectedLinkIDs[i]);
			}
		}
	}

	// create links
	{
		int srcNodeID = 0;
		int srcPinID = 0;
		int dstNodeID = 0;
		int dstPinID = 0;
		if (ImNodes::IsLinkCreated(&srcNodeID, &srcPinID, &dstNodeID, &dstPinID))
		{
			createLink(srcNodeID, srcPinID, dstNodeID, dstPinID);
		}
	}

	// destroy nodes
	{
		const auto selectedNodeCount = ImNodes::NumSelectedNodes();
		if (ImGui::IsKeyDown((int)InputKey::DELETE) && selectedNodeCount > 0)
		{
			int *selectedNodeIDs = ALLOC_A_T(int, selectedNodeCount);
			ImNodes::GetSelectedNodes(selectedNodeIDs);

			for (size_t i = 0; i < selectedNodeCount; ++i)
			{
				destroyNode(selectedNodeIDs[i]);
			}
		}
	}

	// create nodes
	{
		int startedAtPinID = 0;
		if (ImNodes::IsLinkDropped(&startedAtPinID))
		{
			bool startedAtInPin = false;
			int startedAtNodeID = -1;
			// find node where the link was started
			for (auto *n : m_nodes)
			{
				if (n->m_outputPinID == startedAtPinID)
				{
					startedAtNodeID = n->m_nodeID;
					break;
				}
				else
				{
					bool foundPin = false;
					for (auto pid : n->m_inputPinIDs)
					{
						if (pid == startedAtPinID)
						{
							startedAtNodeID = n->m_nodeID;
							startedAtInPin = true;
							foundPin = true;
						}
					}
					if (foundPin)
					{
						break;
					}
				}
			}

			if (startedAtPinID == k_rootNodeInputPinID)
			{
				startedAtInPin = true;
				startedAtNodeID = k_rootNodeID;
			}

			// we can only create a default link if the other end is an output pin (because each node only has one)
			m_linkNewNode = false;
			if (startedAtInPin)
			{
				m_linkNewNode = true;
				m_startedLinkAtNodeID = startedAtNodeID;
				m_startedLinkAtPinID = startedAtPinID;
			}
			auto mousePos = ImGui::GetMousePos();
			m_newNodePosX = mousePos.x;
			m_newNodePosY = mousePos.y;

			ImGui::OpenPopup("add_node_popup");
		}

		if (ImGui::BeginPopup("add_node_popup"))
		{

			ImGui::Text("Node to add:");
			ImGui::Separator();

			for (size_t i = 0; i < eastl::size(k_nodeTypeNames); ++i)
			{
				if (ImGui::Selectable(k_nodeTypeNames[i]))
				{
					AnimationGraphEditorNode *editorNode = nullptr;

					switch (static_cast<AnimationGraphNodeType>(i))
					{
					case AnimationGraphNodeType::ANIM_CLIP:
						editorNode = new AnimationGraphAnimClipEditorNode(); break;
					case AnimationGraphNodeType::LERP:
						editorNode = new AnimationGraphLerpEditorNode(); break;
					case AnimationGraphNodeType::LERP_1D_ARRAY:
						editorNode = new AnimationGraphLerp1DArrayEditorNode(); break;
					case AnimationGraphNodeType::LERP_2D:
						editorNode = new AnimationGraphLerp2DEditorNode(); break;
					default:
						assert(false);
						break;
					}

					editorNode->m_nodeID = m_nextID++;
					editorNode->m_outputPinID = m_nextID++;
					for (auto &pid : editorNode->m_inputPinIDs)
					{
						pid = m_nextID++;
					}

					ImNodes::SetNodeScreenSpacePos(editorNode->m_nodeID, ImVec2(m_newNodePosX, m_newNodePosY));

					m_nodes.push_back(editorNode);

					if (m_linkNewNode)
					{
						createLink(editorNode->m_nodeID, editorNode->m_outputPinID, m_startedLinkAtNodeID, m_startedLinkAtPinID);
						m_linkNewNode = false;
					}
				}
			}
			ImGui::EndPopup();
		}
	}

}

void AnimationGraphWindow::drawParams() noexcept
{
	AnimationGraphEditorParam *paramToDelete = nullptr;

	ImGui::BeginChild("##Parameters");
	{
		if (ImGui::Button("Add Parameter"))
		{
			AnimationGraphEditorParam param;
			param.m_param.m_type = AnimationGraphParameter::Type::FLOAT;
			param.m_param.m_data.f = 0.0f;
			param.m_param.m_name = SID("New Parameter");
			strcpy_s(param.m_nameScratchMem, param.m_param.m_name.m_string);

			m_params.push_back(new AnimationGraphEditorParam(param));
		}

		ImGui::Separator();
		ImGui::NewLine();

		for (auto &param : m_params)
		{
			bool open = true;
			ImGui::PushID(param);
			bool displayParam = ImGui::CollapsingHeader(param->m_param.m_name.m_string, &open, ImGuiTreeNodeFlags_DefaultOpen);

			if (!open)
			{
				ImGui::OpenPopup("Delete Parameter?");
			}

			if (ImGui::BeginPopupModal("Delete Parameter?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				size_t refCount = getParameterReferenceCount(param);
				if (refCount == 0)
				{
					ImGui::Text("Delete Parameter \"%s\".\n", param->m_param.m_name.m_string);
				}
				else
				{
					ImGui::Text("Delete Parameter \"%s\". It still has %u references!\n", param->m_param.m_name.m_string, (unsigned)refCount);
				}
				ImGui::Separator();

				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					assert(!paramToDelete);
					paramToDelete = param;
					displayParam = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			if (displayParam)
			{
				// display name
				if (ImGui::InputText("Name", param->m_nameScratchMem, sizeof(param->m_nameScratchMem)))
				{
					param->m_param.m_name = StringID(param->m_nameScratchMem);
				}

				const char *typeNames[]{ "Float", "Int", "Bool" };
				assert((size_t)param->m_param.m_type < eastl::size(typeNames));

				const AnimationGraphParameter::Type oldType = param->m_param.m_type;
				AnimationGraphParameter::Type newType = param->m_param.m_type;

				// combo box for changing the type
				if (ImGui::BeginCombo("Type", typeNames[(size_t)param->m_param.m_type]))
				{
					for (size_t i = 0; i < eastl::size(typeNames); ++i)
					{
						bool selected = i == (size_t)param->m_param.m_type;
						if (ImGui::Selectable(typeNames[i], selected))
						{
							newType = static_cast<AnimationGraphParameter::Type>(i);
						}
					}
					ImGui::EndCombo();
				}

				// type was changed -> cast the old value to the new type
				if (newType != oldType)
				{
					AnimationGraphParameter::Data newVal;
					switch (newType)
					{
					case AnimationGraphParameter::Type::FLOAT:
						newVal.f = oldType == AnimationGraphParameter::Type::INT ? (float)param->m_param.m_data.i : (float)param->m_param.m_data.b;
						break;
					case AnimationGraphParameter::Type::INT:
						newVal.i = oldType == AnimationGraphParameter::Type::BOOL ? (int32_t)param->m_param.m_data.b : (int32_t)param->m_param.m_data.f;
						break;
					case AnimationGraphParameter::Type::BOOL:
						newVal.b = oldType == AnimationGraphParameter::Type::FLOAT ? (param->m_param.m_data.f != 0.0f) : (bool)param->m_param.m_data.i;
						break;
					default:
						assert(false);
						break;
					}

					param->m_param.m_type = newType;
					param->m_param.m_data = newVal;
				}

				// display type
				switch (param->m_param.m_type)
				{
				case AnimationGraphParameter::Type::FLOAT:
					ImGui::InputFloat("Value", &param->m_param.m_data.f);
					break;
				case AnimationGraphParameter::Type::INT:
					ImGui::InputInt("Value", &param->m_param.m_data.i);
					break;
				case AnimationGraphParameter::Type::BOOL:
					ImGui::Checkbox("Value", &param->m_param.m_data.b);
					break;
				default:
					assert(false);
					break;
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (paramToDelete)
	{
		deleteParameter(paramToDelete);
	}
}

void AnimationGraphWindow::drawAnimationClips() noexcept
{
	Asset<AnimationClipAsset> *clipToDelete = nullptr;

	ImGui::BeginChild("##AnimClips");
	{
		if (ImGui::Button("Add Animation Clip"))
		{
			m_animClips.push_back(new Asset<AnimationClipAsset>());
		}

		ImGui::Separator();
		ImGui::NewLine();

		for (auto *clip : m_animClips)
		{
			const char *clipName = clip->get() ? clip->get()->getAssetID().m_string : "<No Asset>";
			bool open = true;
			ImGui::PushID(clip);
			bool displayClip = ImGui::CollapsingHeader(clipName, &open, ImGuiTreeNodeFlags_DefaultOpen);
			if (!open)
			{
				ImGui::OpenPopup("Delete Animation Clip Reference?");
			}

			if (ImGui::BeginPopupModal("Delete Animation Clip Reference?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				size_t refCount = getAnimationClipReferenceCount(clip);
				if (refCount == 0)
				{
					ImGui::Text("Delete Animation Clip Reference \"%s\".\n", clipName);
				}
				else
				{
					ImGui::Text("Delete Animation Clip Reference \"%s\". It still has %u references!\n", clipName, (unsigned)refCount);
				}
				ImGui::Separator();

				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					assert(!clipToDelete);
					clipToDelete = clip;
					displayClip = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			if (displayClip)
			{
				AssetID resultAssetID;

				if (ImGuiHelpers::AssetPicker("Animation Clip Asset", AnimationClipAsset::k_assetType, clip->get(), &resultAssetID))
				{
					*clip = AssetManager::get()->getAsset<AnimationClipAsset>(resultAssetID);
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (clipToDelete)
	{
		deleteAnimationClip(clipToDelete);
	}
}

void AnimationGraphWindow::drawParamComboBox(const char *name, AnimationGraphEditorParam *&paramPtr, AnimationGraphParameter::Type type) noexcept
{
	AnimationGraphEditorParam *curParam = paramPtr;
	AnimationGraphEditorParam *newParam = curParam;
	ImGui::PushItemWidth(200.0f);
	if (ImGui::BeginCombo(name, curParam ? curParam->m_param.m_name.m_string : "<Empty>"))
	{
		for (size_t i = 0; i < m_params.size(); ++i)
		{
			if (m_params[i]->m_param.m_type == type)
			{
				bool selected = m_params[i] == curParam;
				if (ImGui::Selectable(((AnimationGraphEditorParam *)m_params[i])->m_param.m_name.m_string, selected))
				{
					newParam = m_params[i];
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	paramPtr = newParam;
}

size_t AnimationGraphWindow::getParameterReferenceCount(const AnimationGraphEditorParam *param) noexcept
{
	size_t refCount = 0;
	for (const auto *n : m_nodes)
	{
		refCount += n->getParameterReferenceCount(param);
	}
	return refCount;
}

void AnimationGraphWindow::deleteParameter(const AnimationGraphEditorParam *param) noexcept
{
	for (auto *n : m_nodes)
	{
		n->deleteParameter(param);
	}
	m_params.erase(eastl::remove(m_params.begin(), m_params.end(), param), m_params.end());
}

size_t AnimationGraphWindow::getAnimationClipReferenceCount(const Asset<AnimationClipAsset> *animClip) noexcept
{
	size_t refCount = 0;
	for (const auto *n : m_nodes)
	{
		refCount += n->getAnimationClipReferenceCount(animClip);
	}
	return refCount;
}

void AnimationGraphWindow::deleteAnimationClip(const Asset<AnimationClipAsset> *animClip) noexcept
{
	for (auto *n : m_nodes)
	{
		n->deleteAnimationClip(animClip);
	}
	m_animClips.erase(eastl::remove(m_animClips.begin(), m_animClips.end(), animClip), m_animClips.end());
}

void AnimationGraphWindow::destroyLink(int linkID, bool removeFromLinkList) noexcept
{
	auto linkIt = eastl::find_if(m_links.begin(), m_links.end(), [&](const auto &link)
		{
			return link.m_linkID == linkID;
		});

	if (linkIt != m_links.end())
	{
		auto &link = *linkIt;

		// set destination pin of link to null
		for (auto *n : m_nodes)
		{
			for (size_t i = 0; i < eastl::size(n->m_inputPinIDs); ++i)
			{
				if (n->m_inputPinIDs[i] == link.m_toPinID)
				{
					n->clearInputPin(i);
					break; // early out of search loop
				}
			}
		}

		if (removeFromLinkList)
		{
			m_links.erase(linkIt);
		}
	}
}

void AnimationGraphWindow::createLink(int srcNodeID, int srcPinID, int dstNodeID, int dstPinID) noexcept
{
	// find src node
	AnimationGraphEditorNode *srcNode = nullptr;
	for (auto *n : m_nodes)
	{
		if (n->m_nodeID == srcNodeID)
		{
			srcNode = n;
			break;
		}
	}
	assert(srcNode);

	// find dst node
	if (dstNodeID == k_rootNodeID)
	{
		assert(dstPinID == k_rootNodeInputPinID);
	}
	else
	{
		AnimationGraphEditorNode *dstNode = nullptr;
		for (auto *n : m_nodes)
		{
			if (n->m_nodeID == dstNodeID)
			{
				dstNode = n;
				break;
			}
		}
		assert(dstNode);

		// disallow creating loops
		if (dstNode->findLoop(dstNode))
		{
			return;
		}

		// assign to pin
		bool foundPin = false;
		for (size_t i = 0; i < eastl::size(dstNode->m_inputPinIDs); ++i)
		{
			if (dstNode->m_inputPinIDs[i] == dstPinID)
			{
				dstNode->setInputPin(i, srcNode);
				foundPin = true;
				break;
			}
		}
		assert(foundPin);
	}

	// drop any other links pointing to the same destination
	m_links.erase(eastl::remove_if(m_links.begin(), m_links.end(), [&](const auto &link)
		{
			return link.m_toPinID == dstPinID;
		}), m_links.end());

	// create link
	Link link{};
	link.m_linkID = m_nextID++;
	link.m_fromNodeID = srcNodeID;
	link.m_fromPinID = srcPinID;
	link.m_toNodeID = dstNodeID;
	link.m_toPinID = dstPinID;
	m_links.push_back(link);
}

void AnimationGraphWindow::createVisualLink(AnimationGraphEditorNode *fromNode, int toPinID, int toNodeID) noexcept
{
	if (fromNode)
	{
		Link link;
		link.m_linkID = m_nextID++;
		link.m_fromNodeID = fromNode->m_nodeID;
		link.m_fromPinID = fromNode->m_outputPinID;
		link.m_toNodeID = toNodeID;
		link.m_toPinID = toPinID;
		m_links.push_back(link);
	}
}

void AnimationGraphWindow::destroyNode(int nodeID) noexcept
{
	if (nodeID == k_rootNodeID)
	{
		return;
	}

	// find node
	AnimationGraphEditorNode *node = nullptr;
	for (auto *n : m_nodes)
	{
		if (n->m_nodeID == nodeID)
		{
			node = n;
			break;
		}
	}
	assert(node);

	// destroy links
	for (auto &l : m_links)
	{
		if (l.m_fromNodeID == node->m_nodeID || l.m_toNodeID == node->m_nodeID)
		{
			// dont remove the link from the vector yet because we are still iterating
			destroyLink(l.m_linkID, false);
		}
	}

	// now we can remove them from the vector
	m_links.erase(eastl::remove_if(m_links.begin(), m_links.end(), [&](const auto &l)
		{
			return l.m_fromNodeID == node->m_nodeID || l.m_toNodeID == node->m_nodeID;
		}), m_links.end());

	// delete the node
	m_nodes.erase(eastl::remove(m_nodes.begin(), m_nodes.end(), node), m_nodes.end());
	delete node;

	if (ImNodes::IsNodeSelected(nodeID))
	{
		ImNodes::ClearNodeSelection(nodeID);
	}
}