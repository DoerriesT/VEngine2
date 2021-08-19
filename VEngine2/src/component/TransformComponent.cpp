#include "TransformComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void TransformComponent::onGUI(void *instance) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	int mobilityInt = static_cast<int>(c.m_mobility);
	const char *mobilityStrings[] = { "STATIC", "DYNAMIC" };
	ImGui::Combo("Mobility", &mobilityInt, mobilityStrings, IM_ARRAYSIZE(mobilityStrings));
	c.m_mobility = static_cast<Mobility>(mobilityInt);

	ImGui::DragFloat3("Translation", &c.m_translation.x, 0.1f);

	{
		glm::vec3 eulerAnglesBefore = glm::degrees(glm::eulerAngles(c.m_rotation));
		glm::vec3 eulerAngles = eulerAnglesBefore;
		if (ImGui::DragFloat3("Rotation", &eulerAngles.x, 1.0f, -360.0f, 360.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			glm::vec3 diff = eulerAngles - eulerAnglesBefore;
			glm::quat diffQuat = glm::quat(glm::radians(diff));

			c.m_rotation = glm::normalize(c.m_rotation * diffQuat);
		}
	}

	ImGui::DragFloat3("Scale", &c.m_scale.x, 0.1f);
	
}
