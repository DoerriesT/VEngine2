#include "gui_helpers.h"
#include "imgui.h"
#include "asset/Asset.h"
#include "asset/AssetMetaDataRegistry.h"
#include "asset/AssetManager.h"
#include <ctype.h>

void ImGuiHelpers::Tooltip(const char *text) noexcept
{
	if (text && ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text(text);
		ImGui::EndTooltip();
	}
}

void ImGuiHelpers::ColorEdit3(const char *label, uint32_t *color, ImGuiColorEditFlags flags) noexcept
{
	float col[3];
	col[0] = ((*color >> 0) & 0xFF) / 255.0f;
	col[1] = ((*color >> 8) & 0xFF) / 255.0f;
	col[2] = ((*color >> 16) & 0xFF) / 255.0f;

	ImGui::ColorEdit3(label, col, flags);

	uint32_t result = 0;
	result |= (static_cast<uint32_t>(col[0] * 255.0f) & 0xFF) << 0;
	result |= (static_cast<uint32_t>(col[1] * 255.0f) & 0xFF) << 8;
	result |= (static_cast<uint32_t>(col[2] * 255.0f) & 0xFF) << 16;
	result |= *color & 0xFF000000;

	*color = result;
}

void ImGuiHelpers::ColorEdit4(const char *label, uint32_t *color, ImGuiColorEditFlags flags) noexcept
{
	float col[4];
	col[0] = ((*color >> 0) & 0xFF) / 255.0f;
	col[1] = ((*color >> 8) & 0xFF) / 255.0f;
	col[2] = ((*color >> 16) & 0xFF) / 255.0f;
	col[3] = ((*color >> 24) & 0xFF) / 255.0f;

	ImGui::ColorEdit4(label, col, flags);

	uint32_t result = 0;
	result |= (static_cast<uint32_t>(col[0] * 255.0f) & 0xFF) << 0;
	result |= (static_cast<uint32_t>(col[1] * 255.0f) & 0xFF) << 8;
	result |= (static_cast<uint32_t>(col[2] * 255.0f) & 0xFF) << 16;
	result |= (static_cast<uint32_t>(col[3] * 255.0f) & 0xFF) << 24;

	*color = result;
}

// TODO: find cleaner solution than this
static char s_searchText[255];
static char s_searchTextLower[255];

bool ImGuiHelpers::AssetPicker(const char *label, const AssetType &assetType, const AssetData *inAssetData, AssetID *resultAssetID) noexcept
{
	bool assignedNewAssetData = false;

	const char *popupNameBase = "select_asset_popup";
	static char s_popupName[sizeof(popupNameBase) + 32];
	s_popupName[0] = '\0';
	strcat_s(s_popupName, popupNameBase);
	strcat_s(s_popupName, label);

	ImGui::Text(label);
	ImGui::BeginGroup();
	{
		ImGui::Image(ImGui::GetIO().Fonts->TexID, ImVec2(64, 64));
		ImGui::SameLine();
		ImGui::Text(inAssetData ? inAssetData->getAssetID().m_string : "EMPTY");
		ImGui::SameLine();
		if (ImGui::ArrowButton(label, ImGuiDir_Down))
		{
			s_searchText[0] = '\0';
			s_searchTextLower[0] = '\0';
			ImGui::OpenPopup(s_popupName);
		}
	}
	ImGui::EndGroup();

	if (ImGui::BeginDragDropTarget())
	{
		// peek the payload
		const auto *payload = ImGui::GetDragDropPayload();
		// manually check if this is an asset
		if (payload->IsDataType("ASSET_ID"))
		{
			AssetID assetID = *reinterpret_cast<const AssetID *>(payload->Data);

			// check if the asset is of the proper type
			AssetType actualAssetType;
			if (AssetMetaDataRegistry::get()->getAssetType(assetID, &actualAssetType) && actualAssetType == assetType)
			{
				// formally accept the payload and load the asset
				if (ImGui::AcceptDragDropPayload("ASSET_ID"))
				{
					*resultAssetID = assetID;
					assignedNewAssetData = true;
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginPopup(s_popupName))
	{
		// search field
		{
			ImGui::InputTextWithHint("##Search", "Search Assets", s_searchText, sizeof(s_searchText));
			for (size_t i = 0; ; ++i)
			{
				if (s_searchText[i] == '\0')
				{
					s_searchTextLower[i] = '\0';
					break;
				}
				s_searchTextLower[i] = tolower(s_searchText[i]);
			}
		}
		eastl::string_view searchTextView = eastl::string_view(s_searchTextLower);

		ImGui::Separator();
		ImGui::NewLine();

		auto *reg = AssetMetaDataRegistry::get();
		size_t count = 0;
		if (reg->getAssetIDs(assetType, &count, nullptr))
		{
			for (size_t i = 0; i < count; ++i)
			{
				AssetID assetID;
				if (!reg->getAssetIDAt(assetType, i, &assetID))
				{
					break;
				}

				if (!searchTextView.empty() && eastl::string_view(assetID.m_string).find(s_searchTextLower, 0) == eastl::string::npos)
				{
					continue;
				}

				if (ImGui::Selectable(assetID.m_string))
				{
					*resultAssetID = assetID;
					assignedNewAssetData = true;
				}
			}
		}

		ImGui::EndPopup();
	}

	return assignedNewAssetData;
}
