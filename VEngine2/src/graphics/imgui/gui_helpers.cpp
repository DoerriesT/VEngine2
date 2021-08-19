#include "gui_helpers.h"
#include "imgui.h"

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
