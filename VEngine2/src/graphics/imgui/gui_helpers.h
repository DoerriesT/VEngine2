#pragma once
#include <stdint.h>

typedef int ImGuiColorEditFlags;
struct TUUID;
typedef TUUID AssetType;
class AssetData;

namespace ImGuiHelpers
{
	void Tooltip(const char *text) noexcept;
	void ColorEdit3(const char *label, uint32_t *color, ImGuiColorEditFlags flags = 0) noexcept;
	void ColorEdit4(const char *label, uint32_t *color, ImGuiColorEditFlags flags = 0) noexcept;
	bool AssetPicker(const char *label, const AssetType &assetType, const AssetData *inAssetData, AssetData **resultAssetData) noexcept;
}