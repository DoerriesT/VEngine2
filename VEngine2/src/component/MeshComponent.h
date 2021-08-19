#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"

struct MeshComponent
{
	Asset<MeshAssetData> m_mesh;

	static void onGUI(void *instance) noexcept;
	static const char *getComponentName() noexcept { return "MeshComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Mesh Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};