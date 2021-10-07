#include "AnimationGraphWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/imnodes.h>
#include <animation/AnimationGraph.h>
#include <utility/memory.h>
#include <input/InputTokens.h>
#include <Log.h>
#include <graphics/imgui/gui_helpers.h>

struct AnimationGraphEditorNode
{
	int m_nodeID;
	int m_outputPinID;
	int m_inputPinIDs[8];
	AnimationGraphNodeType m_nodeType;
	AnimationGraphNodeData m_nodeData;
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
			m_animClips.push_back(new Asset<AnimationClipAssetData>(clips[i]));
		}

		// copy nodes
		const auto nodeCount = graph->getNodeCount();
		const auto *nodes = graph->getNodes();
		for (size_t i = 0; i < nodeCount; ++i)
		{
			AnimationGraphEditorNode editorNode{};
			editorNode.m_nodeID = m_nextID++;
			editorNode.m_outputPinID = m_nextID++;
			for (auto &pid : editorNode.m_inputPinIDs)
			{
				pid = m_nextID++;
			}
			editorNode.m_nodeType = nodes[i].m_nodeType;
			editorNode.m_nodeData = nodes[i].m_nodeData;

			m_nodes.push_back(new AnimationGraphEditorNode(editorNode));
		}

		auto createLink = [this](size_t aliasedPtr, int toPinID, int toNodeID)
		{
			Link link;
			link.m_linkID = m_nextID++;
			link.m_fromNodeID = ((AnimationGraphEditorNode *)aliasedPtr)->m_nodeID;
			link.m_fromPinID = ((AnimationGraphEditorNode *)aliasedPtr)->m_outputPinID;
			link.m_toNodeID = toNodeID;
			link.m_toPinID = toPinID;
			m_links.push_back(link);
		};

		if (graph->getRootNodeIndex() != -1)
		{
			createLink((size_t)m_nodes[graph->getRootNodeIndex()], k_rootNodeInputPinID, k_rootNodeID);
		}

		// translate indices in nodes to pointers and set up links
		for (auto *n : m_nodes)
		{
			auto convertIdx = [](size_t &aliasedIdx, const auto &v)
			{
				if (aliasedIdx == -1 || aliasedIdx >= v.size())
				{
					aliasedIdx = 0;
				}
				else
				{
					aliasedIdx = (size_t)v[aliasedIdx];
				}
			};

			switch (n->m_nodeType)
			{
			case AnimationGraphNodeType::ANIM_CLIP:
			{
				convertIdx(n->m_nodeData.m_clipNodeData.m_animClip, m_animClips);
				convertIdx(n->m_nodeData.m_clipNodeData.m_loop, m_params);
				break;
			}
			case AnimationGraphNodeType::LERP:
			{
				convertIdx(n->m_nodeData.m_lerpNodeData.m_inputA, m_nodes);
				convertIdx(n->m_nodeData.m_lerpNodeData.m_inputB, m_nodes);
				convertIdx(n->m_nodeData.m_lerpNodeData.m_alpha, m_params);

				createLink(n->m_nodeData.m_lerpNodeData.m_inputA, n->m_inputPinIDs[0], n->m_nodeID);
				createLink(n->m_nodeData.m_lerpNodeData.m_inputB, n->m_inputPinIDs[1], n->m_nodeID);

				break;
			}
			case AnimationGraphNodeType::LERP_1D_ARRAY:
			{
				for (size_t j = 0; j < n->m_nodeData.m_lerp1DArrayNodeData.m_inputCount; ++j)
				{
					convertIdx(n->m_nodeData.m_lerp1DArrayNodeData.m_inputs[j], m_nodes);

					createLink(n->m_nodeData.m_lerp1DArrayNodeData.m_inputs[j], n->m_inputPinIDs[j], n->m_nodeID);
				}
				convertIdx(n->m_nodeData.m_lerp1DArrayNodeData.m_alpha, m_params);
				break;
			}
			case AnimationGraphNodeType::LERP_2D:
			{
				convertIdx(n->m_nodeData.m_lerp2DNodeData.m_inputTL, m_nodes);
				convertIdx(n->m_nodeData.m_lerp2DNodeData.m_inputTR, m_nodes);
				convertIdx(n->m_nodeData.m_lerp2DNodeData.m_inputBL, m_nodes);
				convertIdx(n->m_nodeData.m_lerp2DNodeData.m_inputBR, m_nodes);
				convertIdx(n->m_nodeData.m_lerp2DNodeData.m_alphaX, m_params);
				convertIdx(n->m_nodeData.m_lerp2DNodeData.m_alphaY, m_params);

				createLink(n->m_nodeData.m_lerp2DNodeData.m_inputTL, n->m_inputPinIDs[0], n->m_nodeID);
				createLink(n->m_nodeData.m_lerp2DNodeData.m_inputTR, n->m_inputPinIDs[1], n->m_nodeID);
				createLink(n->m_nodeData.m_lerp2DNodeData.m_inputBL, n->m_inputPinIDs[2], n->m_nodeID);
				createLink(n->m_nodeData.m_lerp2DNodeData.m_inputBR, n->m_inputPinIDs[3], n->m_nodeID);
				break;
			}
			default:
				assert(false);
				break;
			}
		}

		ImNodes::SetNodeEditorSpacePos(k_rootNodeID, ImVec2(ImGui::GetContentRegionAvail().x * 0.75f, ImGui::GetContentRegionAvail().y * 0.5f));
		ImNodes::SetNodeDraggable(k_rootNodeID, false);
		if (!m_nodes.empty())
		{
			computeNodePosition(m_nodes[graph->getRootNodeIndex()], 1, 0);
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

		eastl::vector<Asset<AnimationClipAssetData>> clips;
		clips.reserve(m_animClips.size());
		for (size_t i = 0; i < m_animClips.size(); ++i)
		{
			clips.push_back(*m_animClips[i]);
		}

		eastl::vector<AnimationGraphNode> nodes;
		nodes.reserve(m_nodes.size());
		for (size_t i = 0; i < m_nodes.size(); ++i)
		{
			AnimationGraphNode node;
			node.m_nodeType = m_nodes[i]->m_nodeType;
			node.m_nodeData = m_nodes[i]->m_nodeData;

			auto convertPtr = [](size_t &aliasedPtr, const auto &v)
			{
				if (aliasedPtr == 0)
				{
					aliasedPtr = -1;
				}
				else
				{
					auto it = eastl::find(v.begin(), v.end(), (decltype(v[0]))aliasedPtr);
					aliasedPtr = it != v.end() ? (it - v.begin()) : -1;
				}
			};

			// convert pointers to indices
			switch (node.m_nodeType)
			{
			case AnimationGraphNodeType::ANIM_CLIP:
			{
				convertPtr(node.m_nodeData.m_clipNodeData.m_animClip, m_animClips);
				convertPtr(node.m_nodeData.m_clipNodeData.m_loop, m_params);
				break;
			}
			case AnimationGraphNodeType::LERP:
			{
				convertPtr(node.m_nodeData.m_lerpNodeData.m_inputA, m_nodes);
				convertPtr(node.m_nodeData.m_lerpNodeData.m_inputB, m_nodes);
				convertPtr(node.m_nodeData.m_lerpNodeData.m_alpha, m_params);
				break;
			}
			case AnimationGraphNodeType::LERP_1D_ARRAY:
			{
				for (size_t j = 0; j < node.m_nodeData.m_lerp1DArrayNodeData.m_inputCount; ++j)
				{
					convertPtr(node.m_nodeData.m_lerp1DArrayNodeData.m_inputs[j], m_nodes);
				}
				convertPtr(node.m_nodeData.m_lerp1DArrayNodeData.m_alpha, m_params);
				break;
			}
			case AnimationGraphNodeType::LERP_2D:
			{
				convertPtr(node.m_nodeData.m_lerp2DNodeData.m_inputTL, m_nodes);
				convertPtr(node.m_nodeData.m_lerp2DNodeData.m_inputTR, m_nodes);
				convertPtr(node.m_nodeData.m_lerp2DNodeData.m_inputBL, m_nodes);
				convertPtr(node.m_nodeData.m_lerp2DNodeData.m_inputBR, m_nodes);
				convertPtr(node.m_nodeData.m_lerp2DNodeData.m_alphaX, m_params);
				convertPtr(node.m_nodeData.m_lerp2DNodeData.m_alphaY, m_params);
				break;
			}
			default:
				assert(false);
				break;
			}

			nodes.push_back(node);
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


		*graph = AnimationGraph(rootNodeIdx, nodes.size(), nodes.data(), params.size(), params.data(), clips.size(), clips.data(), graph->getControllerScript());
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
			switch (n->m_nodeType)
			{
			case AnimationGraphNodeType::ANIM_CLIP:
				drawAnimClipNode(n); break;
			case AnimationGraphNodeType::LERP:
				drawLerpNode(n); break;
			case AnimationGraphNodeType::LERP_1D_ARRAY:
				drawLerp1DArrayNode(n); break;
			case AnimationGraphNodeType::LERP_2D:
				drawLerp2DNode(n); break;
			default:
				assert(false);
				break;
			}
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

			const char *nodeNames[]{ "Animation Clip", "Lerp", "Lerp 1D Array", "Lerp 2D" };

			for (size_t i = 0; i < eastl::size(nodeNames); ++i)
			{
				if (ImGui::Selectable(nodeNames[i]))
				{
					AnimationGraphEditorNode editorNode;
					editorNode.m_nodeID = m_nextID++;
					editorNode.m_outputPinID = m_nextID++;
					for (auto &pid : editorNode.m_inputPinIDs)
					{
						pid = m_nextID++;
					}
					editorNode.m_nodeType = static_cast<AnimationGraphNodeType>(i);

					switch (editorNode.m_nodeType)
					{
					case AnimationGraphNodeType::ANIM_CLIP:
						editorNode.m_nodeData.m_clipNodeData = {};
						editorNode.m_nodeData.m_clipNodeData.m_animClip = (size_t)m_animClips[0]; // TODO
						break;
					case AnimationGraphNodeType::LERP:
						editorNode.m_nodeData.m_lerpNodeData = {};
						break;
					case AnimationGraphNodeType::LERP_1D_ARRAY:
						editorNode.m_nodeData.m_lerp1DArrayNodeData = {};
						break;
					case AnimationGraphNodeType::LERP_2D:
						editorNode.m_nodeData.m_lerp2DNodeData = {};
						break;
					default:
						assert(false);
						break;
					}

					ImNodes::SetNodeScreenSpacePos(editorNode.m_nodeID, ImVec2(m_newNodePosX, m_newNodePosY));

					m_nodes.push_back(new AnimationGraphEditorNode(editorNode));

					if (m_linkNewNode)
					{
						createLink(editorNode.m_nodeID, editorNode.m_outputPinID, m_startedLinkAtNodeID, m_startedLinkAtPinID);
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
	Asset<AnimationClipAssetData> *clipToDelete = nullptr;

	ImGui::BeginChild("##AnimClips");
	{
		if (ImGui::Button("Add Animation Clip"))
		{
			m_animClips.push_back(new Asset<AnimationClipAssetData>());
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
				AssetData *resultAssetData = nullptr;
			
				if (ImGuiHelpers::AssetPicker("Animation Clip Asset", AnimationClipAssetData::k_assetType, clip->get(), &resultAssetData))
				{
					*clip = resultAssetData;
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

void AnimationGraphWindow::drawAnimClipNode(AnimationGraphEditorNode *node) noexcept
{
	ImNodes::BeginNode(node->m_nodeID);
	{
		// title bar
		ImNodes::BeginNodeTitleBar();
		ImGui::TextUnformatted("Animation Clip");
		ImNodes::EndNodeTitleBar();

		// pins
		ImNodes::BeginOutputAttribute(node->m_outputPinID);
		ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
		ImGui::TextUnformatted("Out");
		ImNodes::EndOutputAttribute();

		auto *curClip = (Asset<AnimationClipAssetData> *)node->m_nodeData.m_clipNodeData.m_animClip;
		auto *newClip = curClip;

		const char *curClipName = curClip ? (curClip->get() ? curClip->get()->getAssetID().m_string : "<No Asset>") : "<Empty>";

		ImGui::PushItemWidth(200.0f);
		if (ImGui::BeginCombo("Clip", curClipName))
		{
			for (size_t i = 0; i < m_animClips.size(); ++i)
			{
				bool selected = m_animClips[i] == curClip;
				if (ImGui::Selectable(m_animClips[i]->get() ? (*m_animClips[i])->getAssetID().m_string : "<No Asset>", selected))
				{
					newClip = m_animClips[i];
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		node->m_nodeData.m_clipNodeData.m_animClip = (size_t)newClip;

		drawParamComboBox("Loop", node->m_nodeData.m_clipNodeData.m_loop, AnimationGraphParameter::Type::BOOL);
	}
	ImNodes::EndNode();
}

void AnimationGraphWindow::drawLerpNode(AnimationGraphEditorNode *node) noexcept
{
	ImNodes::BeginNode(node->m_nodeID);
	{
		// title bar
		ImNodes::BeginNodeTitleBar();
		ImGui::TextUnformatted("Lerp");
		ImNodes::EndNodeTitleBar();

		// pins
		ImNodes::BeginOutputAttribute(node->m_outputPinID);
		ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
		ImGui::TextUnformatted("Out");
		ImNodes::EndOutputAttribute();

		ImNodes::BeginInputAttribute(node->m_inputPinIDs[0]);
		ImGui::TextUnformatted("X");
		ImNodes::EndInputAttribute();
		ImNodes::BeginInputAttribute(node->m_inputPinIDs[1]);
		ImGui::TextUnformatted("Y");
		ImNodes::EndInputAttribute();

		drawParamComboBox("Alpha", node->m_nodeData.m_lerpNodeData.m_alpha, AnimationGraphParameter::Type::FLOAT);
	}
	ImNodes::EndNode();
}

void AnimationGraphWindow::drawLerp1DArrayNode(AnimationGraphEditorNode *node) noexcept
{
	ImNodes::BeginNode(node->m_nodeID);
	{
		// title bar
		ImNodes::BeginNodeTitleBar();
		ImGui::TextUnformatted("Lerp 1D Array");
		ImNodes::EndNodeTitleBar();

		// pins
		ImNodes::BeginOutputAttribute(node->m_outputPinID);
		ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
		ImGui::TextUnformatted("Out");
		ImNodes::EndOutputAttribute();

		const auto inputCount = node->m_nodeData.m_lerp1DArrayNodeData.m_inputCount;

		float prevMaxKey = -FLT_MAX;

		for (size_t i = 0; i < AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs; ++i)
		{
			if (i < inputCount)
			{
				ImNodes::BeginInputAttribute(node->m_inputPinIDs[i]);
				ImGui::Text("%i", (int)i);
				ImNodes::EndInputAttribute();
			}
			else
			{
				ImGui::Text("%i", (int)i);
			}

			ImGui::SameLine();

			ImGui::PushID(&node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i]);
			ImGui::PushItemWidth(200.0f);
			if (ImGui::InputFloat("", &node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i]))
			{
				node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i] = fmaxf(node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i], prevMaxKey);
			}
			ImGui::PopItemWidth();
			ImGui::PopID();

			prevMaxKey = node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i];
		}

		drawParamComboBox("Alpha", node->m_nodeData.m_lerp1DArrayNodeData.m_alpha, AnimationGraphParameter::Type::FLOAT);

		int newInputCount = (int)inputCount;
		ImGui::PushItemWidth(200.0f);
		ImGui::InputInt("Input Count", &newInputCount);
		ImGui::PopItemWidth();
		newInputCount = newInputCount < 0 ? 0 : newInputCount;
		newInputCount = newInputCount > AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs ? AnimationGraphNodeData::Lerp1DArrayNodeData::k_maxInputs : newInputCount;

		if (newInputCount > inputCount)
		{
			const float newKeyVal = inputCount == 0 ? 0.0f : node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[inputCount - 1];
			for (size_t i = inputCount; i < newInputCount; ++i)
			{
				node->m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[i] = newKeyVal;
			}
		}
		else if (newInputCount < inputCount)
		{
			for (size_t i = newInputCount; i < inputCount; ++i)
			{
				int sourceOutPinID = ((AnimationGraphEditorNode *)node->m_nodeData.m_lerp1DArrayNodeData.m_inputs[i])->m_outputPinID;
				m_links.erase(eastl::remove_if(m_links.begin(), m_links.end(), [&](const auto &link)
					{
						return link.m_fromPinID == sourceOutPinID && link.m_toPinID == node->m_inputPinIDs[i];
					}), m_links.end());
				node->m_nodeData.m_lerp1DArrayNodeData.m_inputs[i] = 0;
			}
		}

		node->m_nodeData.m_lerp1DArrayNodeData.m_inputCount = (size_t)newInputCount;
	}
	ImNodes::EndNode();
}

void AnimationGraphWindow::drawLerp2DNode(AnimationGraphEditorNode *node) noexcept
{
	ImNodes::BeginNode(node->m_nodeID);
	{
		// title bar
		ImNodes::BeginNodeTitleBar();
		ImGui::TextUnformatted("Lerp 2D");
		ImNodes::EndNodeTitleBar();

		// pins
		ImNodes::BeginOutputAttribute(node->m_outputPinID);
		ImGui::Indent(100.f - ImGui::CalcTextSize("Out").x);
		ImGui::TextUnformatted("Out");
		ImNodes::EndOutputAttribute();

		ImNodes::BeginInputAttribute(node->m_inputPinIDs[0]);
		ImGui::TextUnformatted("TL");
		ImNodes::EndInputAttribute();
		ImNodes::BeginInputAttribute(node->m_inputPinIDs[1]);
		ImGui::TextUnformatted("TR");
		ImNodes::EndInputAttribute();
		ImNodes::BeginInputAttribute(node->m_inputPinIDs[2]);
		ImGui::TextUnformatted("BL");
		ImNodes::EndInputAttribute();
		ImNodes::BeginInputAttribute(node->m_inputPinIDs[3]);
		ImGui::TextUnformatted("BR");
		ImNodes::EndInputAttribute();

		drawParamComboBox("Alpha X", node->m_nodeData.m_lerp2DNodeData.m_alphaX, AnimationGraphParameter::Type::FLOAT);
		drawParamComboBox("Alpha Y", node->m_nodeData.m_lerp2DNodeData.m_alphaY, AnimationGraphParameter::Type::FLOAT);
	}
	ImNodes::EndNode();
}

void AnimationGraphWindow::drawParamComboBox(const char *name, size_t &aliasedCurParamPtr, AnimationGraphParameter::Type type) noexcept
{
	AnimationGraphEditorParam *curParam = (AnimationGraphEditorParam *)aliasedCurParamPtr;
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
	aliasedCurParamPtr = (size_t)newParam;
}

void AnimationGraphWindow::computeNodePosition(AnimationGraphEditorNode *node, int treeDepth, int siblingIndex) noexcept
{
	ImVec2 origin = ImNodes::GetNodeGridSpacePos(k_rootNodeID);
	ImNodes::SetNodeGridSpacePos(node->m_nodeID, ImVec2(-treeDepth * 450.0f + origin.x, siblingIndex * 150.0f + origin.y));

	switch (node->m_nodeType)
	{
	case AnimationGraphNodeType::ANIM_CLIP:
		return;
	case AnimationGraphNodeType::LERP:
		computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerpNodeData.m_inputA, treeDepth + 1, siblingIndex);
		computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerpNodeData.m_inputB, treeDepth + 1, siblingIndex + 1);
		return;

	case AnimationGraphNodeType::LERP_1D_ARRAY:
		for (size_t i = 0; i < node->m_nodeData.m_lerp1DArrayNodeData.m_inputCount; ++i)
		{
			computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerp1DArrayNodeData.m_inputs[i], treeDepth + 1, siblingIndex + (int)i);
		}
		return;
	case AnimationGraphNodeType::LERP_2D:
		computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputTL, treeDepth + 1, siblingIndex);
		computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputTR, treeDepth + 1, siblingIndex + 1);
		computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputBL, treeDepth + 1, siblingIndex + 2);
		computeNodePosition((AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputBR, treeDepth + 1, siblingIndex + 3);
		return;
	default:
		assert(false);
		break;
	}
}

size_t AnimationGraphWindow::getParameterReferenceCount(const AnimationGraphEditorParam *param) noexcept
{
	size_t refCount = 0;

	for (const auto *n : m_nodes)
	{
		switch (n->m_nodeType)
		{
		case AnimationGraphNodeType::ANIM_CLIP:
			refCount += ((AnimationGraphEditorParam *)n->m_nodeData.m_clipNodeData.m_loop == param) ? 1 : 0;
			break;
		case AnimationGraphNodeType::LERP:
			refCount += ((AnimationGraphEditorParam *)n->m_nodeData.m_lerpNodeData.m_alpha == param) ? 1 : 0;
			break;

		case AnimationGraphNodeType::LERP_1D_ARRAY:
			refCount += ((AnimationGraphEditorParam *)n->m_nodeData.m_lerp1DArrayNodeData.m_alpha == param) ? 1 : 0;
			break;
		case AnimationGraphNodeType::LERP_2D:
			refCount += ((AnimationGraphEditorParam *)n->m_nodeData.m_lerp2DNodeData.m_alphaX == param) ? 1 : 0;
			refCount += ((AnimationGraphEditorParam *)n->m_nodeData.m_lerp2DNodeData.m_alphaY == param) ? 1 : 0;
			break;
		default:
			assert(false);
			break;
		}
	}

	return refCount;
}

void AnimationGraphWindow::deleteParameter(const AnimationGraphEditorParam *param) noexcept
{
	for (auto *n : m_nodes)
	{
		switch (n->m_nodeType)
		{
		case AnimationGraphNodeType::ANIM_CLIP:
			if ((AnimationGraphEditorParam *)n->m_nodeData.m_clipNodeData.m_loop == param)
			{
				n->m_nodeData.m_clipNodeData.m_loop = 0; // nullptr
			}
			break;
		case AnimationGraphNodeType::LERP:
			if ((AnimationGraphEditorParam *)n->m_nodeData.m_lerpNodeData.m_alpha == param)
			{
				n->m_nodeData.m_lerpNodeData.m_alpha = 0; // nullptr
			}
			break;
		case AnimationGraphNodeType::LERP_1D_ARRAY:
			if ((AnimationGraphEditorParam *)n->m_nodeData.m_lerp1DArrayNodeData.m_alpha == param)
			{
				n->m_nodeData.m_lerp1DArrayNodeData.m_alpha = 0; // nullptr
			}
			break;
		case AnimationGraphNodeType::LERP_2D:
			if ((AnimationGraphEditorParam *)n->m_nodeData.m_lerp2DNodeData.m_alphaX == param)
			{
				n->m_nodeData.m_lerp2DNodeData.m_alphaX = 0; // nullptr
			}
			if ((AnimationGraphEditorParam *)n->m_nodeData.m_lerp2DNodeData.m_alphaY == param)
			{
				n->m_nodeData.m_lerp2DNodeData.m_alphaY = 0; // nullptr
			}
			break;
		default:
			assert(false);
			break;
		}
	}

	m_params.erase(eastl::remove(m_params.begin(), m_params.end(), param), m_params.end());
}

size_t AnimationGraphWindow::getAnimationClipReferenceCount(const Asset<AnimationClipAssetData> *animClip) noexcept
{
	size_t refCount = 0;

	for (const auto *n : m_nodes)
	{
		if (n->m_nodeType == AnimationGraphNodeType::ANIM_CLIP && ((Asset<AnimationClipAssetData> *)n->m_nodeData.m_clipNodeData.m_animClip == animClip))
		{
			++refCount;
		}
	}

	return refCount;
}

void AnimationGraphWindow::deleteAnimationClip(const Asset<AnimationClipAssetData> *animClip) noexcept
{
	for (auto *n : m_nodes)
	{
		if (n->m_nodeType == AnimationGraphNodeType::ANIM_CLIP && ((Asset<AnimationClipAssetData> *)n->m_nodeData.m_clipNodeData.m_animClip == animClip))
		{
			n->m_nodeData.m_clipNodeData.m_animClip = 0; // nullptr
		}
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
					switch (n->m_nodeType)
					{
					case AnimationGraphNodeType::ANIM_CLIP:
					{
						assert(false);
						break;
					}
					case AnimationGraphNodeType::LERP:
					{
						assert(i < 2);
						if (i == 0)
						{
							n->m_nodeData.m_lerpNodeData.m_inputA = 0;
						}
						else
						{
							n->m_nodeData.m_lerpNodeData.m_inputB = 0;
						}
						break;
					}
					case AnimationGraphNodeType::LERP_1D_ARRAY:
					{
						n->m_nodeData.m_lerp1DArrayNodeData.m_inputs[i] = 0;
						break;
					}
					case AnimationGraphNodeType::LERP_2D:
					{
						switch (i)
						{
						case 0: n->m_nodeData.m_lerp2DNodeData.m_inputTL = 0; break;
						case 1: n->m_nodeData.m_lerp2DNodeData.m_inputTR = 0; break;
						case 2: n->m_nodeData.m_lerp2DNodeData.m_inputBL = 0; break;
						case 3: n->m_nodeData.m_lerp2DNodeData.m_inputBR = 0; break;
						default:
							assert(false);
							break;
						}
						break;
					}
					default:
						assert(false);
						break;
					}

					// early out of search loop
					break;
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
		if (findLoop(dstNode, dstNode))
		{
			return;
		}

		// assign to pin
		bool foundPin = false;
		for (size_t i = 0; i < eastl::size(dstNode->m_inputPinIDs); ++i)
		{
			if (dstNode->m_inputPinIDs[i] == dstPinID)
			{
				switch (dstNode->m_nodeType)
				{
				case AnimationGraphNodeType::ANIM_CLIP:
				{
					assert(false);
					break;
				}
				case AnimationGraphNodeType::LERP:
				{
					assert(i < 2);
					if (i == 0)
					{
						dstNode->m_nodeData.m_lerpNodeData.m_inputA = (size_t)srcNode;
					}
					else
					{
						dstNode->m_nodeData.m_lerpNodeData.m_inputB = (size_t)srcNode;
					}
					break;
				}
				case AnimationGraphNodeType::LERP_1D_ARRAY:
				{
					dstNode->m_nodeData.m_lerp1DArrayNodeData.m_inputs[i] = (size_t)srcNode;
					break;
				}
				case AnimationGraphNodeType::LERP_2D:
				{
					switch (i)
					{
					case 0: dstNode->m_nodeData.m_lerp2DNodeData.m_inputTL = (size_t)srcNode; break;
					case 1: dstNode->m_nodeData.m_lerp2DNodeData.m_inputTR = (size_t)srcNode; break;
					case 2: dstNode->m_nodeData.m_lerp2DNodeData.m_inputBL = (size_t)srcNode; break;
					case 3: dstNode->m_nodeData.m_lerp2DNodeData.m_inputBR = (size_t)srcNode; break;
					default:
						assert(false);
						break;
					}
					break;
				}
				default:
					assert(false);
					break;
				}
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

bool AnimationGraphWindow::findLoop(const AnimationGraphEditorNode *searchNode, const AnimationGraphEditorNode *node, bool isFirstIteration) noexcept
{
	if (!isFirstIteration)
	{
		// missing inputs can result in nullptr nodes
		if (!node)
		{
			return false;
		}

		if (searchNode == node)
		{
			return true;
		}
	}

	switch (node->m_nodeType)
	{
	case AnimationGraphNodeType::ANIM_CLIP:
	{
		return false;
	}
	case AnimationGraphNodeType::LERP:
	{
		return findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerpNodeData.m_inputA, false)
			|| findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerpNodeData.m_inputB, false);
	}
	case AnimationGraphNodeType::LERP_1D_ARRAY:
	{
		for (size_t i = 0; i < node->m_nodeData.m_lerp1DArrayNodeData.m_inputCount; ++i)
		{
			if (findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerp1DArrayNodeData.m_inputs[i], false))
			{
				return true;
			}
		}
		return false;
	}
	case AnimationGraphNodeType::LERP_2D:
	{
		return findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputTL, false)
			|| findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputTR, false)
			|| findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputBL, false)
			|| findLoop(searchNode, (const AnimationGraphEditorNode *)node->m_nodeData.m_lerp2DNodeData.m_inputBR, false);
	}
	default:
		assert(false);
		break;
	}

	return false;
}
