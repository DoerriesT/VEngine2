#pragma once
#include <EASTL/vector.h>
#include <asset/AnimationClipAsset.h>
#include <animation/AnimationGraph.h>

class AnimationGraph;
struct AnimationGraphParameter;
class Engine;

struct AnimationGraphEditorNode;
struct AnimationGraphEditorParam;

class AnimationGraphWindow
{
public:
	explicit AnimationGraphWindow(Engine *engine) noexcept;
	~AnimationGraphWindow() noexcept;
	void draw(AnimationGraph *graph) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	struct Link
	{
		int m_linkID;
		int m_fromNodeID;
		int m_fromPinID;
		int m_toNodeID;
		int m_toPinID;
	};

	static constexpr int k_rootNodeID = 1;
	static constexpr int k_rootNodeInputPinID = 2;
	static constexpr int k_firstID = 3;
	Engine *m_engine = nullptr;
	AnimationGraph *m_graph = nullptr;
	eastl::vector<AnimationGraphEditorNode *> m_nodes;
	eastl::vector<AnimationGraphEditorParam *> m_params;
	eastl::vector<Asset<AnimationClipAssetData> *> m_animClips;
	eastl::vector<Link> m_links;
	int m_nextID = k_firstID;
	bool m_linkNewNode = false;
	int m_startedLinkAtNodeID = 0;
	int m_startedLinkAtPinID = 0;
	float m_newNodePosX = 0.0f;
	float m_newNodePosY = 0.0f;
	bool m_visible = true;

	void importGraph(AnimationGraph *graph) noexcept;
	void exportGraph(AnimationGraph *graph) noexcept;
	
	void drawNodeEditor() noexcept;
	void drawParams() noexcept;
	void drawAnimationClips() noexcept;
	void drawAnimClipNode(AnimationGraphEditorNode *node) noexcept;
	void drawLerpNode(AnimationGraphEditorNode *node) noexcept;
	void drawLerp1DArrayNode(AnimationGraphEditorNode *node) noexcept;
	void drawLerp2DNode(AnimationGraphEditorNode *node) noexcept;
	void drawParamComboBox(const char *name, size_t &aliasedCurParamPtr, AnimationGraphParameter::Type type) noexcept;

	void computeNodePosition(AnimationGraphEditorNode *node, int treeDepth, int siblingIndex) noexcept;

	size_t getParameterReferenceCount(const AnimationGraphEditorParam *param) noexcept;
	void deleteParameter(const AnimationGraphEditorParam *param) noexcept;
	size_t getAnimationClipReferenceCount(const Asset<AnimationClipAssetData> *animClip) noexcept;
	void deleteAnimationClip(const Asset<AnimationClipAssetData> *param) noexcept;

	void destroyLink(int linkID, bool removeFromLinkList = true) noexcept;
	void createLink(int srcNodeID, int srcPinID, int dstNodeID, int dstPinID = 0) noexcept;
	void destroyNode(int nodeID) noexcept;

	bool findLoop(const AnimationGraphEditorNode *searchNode, const AnimationGraphEditorNode *node, bool isFirstIteration = true) noexcept;
};