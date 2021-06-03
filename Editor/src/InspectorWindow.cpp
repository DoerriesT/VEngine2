#include "InspectorWindow.h"
#include <graphics/imgui/imgui.h>
#include <Engine.h>
#include <ecs/ECSTypeIDTranslator.h>
#include <reflection/Reflection.h>

InspectorWindow::InspectorWindow(Engine *engine) noexcept
	:m_engine(engine)
{
}

void InspectorWindow::draw(EntityID entity) noexcept
{
	if (!m_visible || !ImGui::Begin("Inspector"))
	{
		return;
	}
	{
		ECS &ecs = *m_engine->getECS();

		auto compMask = ecs.getComponentMask(entity);

		if (ImGui::Button("Add Component"))
		{
			ImGui::OpenPopup("add_component_popup");
		}

		if (ImGui::BeginPopup("add_component_popup"))
		{

			ImGui::Text("Component to add:");
			ImGui::Separator();

			auto registeredComponentMask = ecs.getRegisteredComponentMask();

			forEachComponentType(registeredComponentMask, [&](size_t, ComponentID componentID)
				{
					if (compMask[componentID])
					{
						return;
					}

					auto typeID = ECSTypeIDTranslator::getFromComponentID(componentID);
					auto type = g_Reflection.getType(typeID);

					if (ImGui::Selectable(type->m_name))
					{
						ecs.addComponentsTypeless(entity, 1, &componentID);
					}
				});

			ImGui::EndPopup();
		}

		ImGui::Separator();
		ImGui::NewLine();

		forEachComponentType(compMask,
			[&](size_t, ComponentID componentID)
			{
				auto typeID = ECSTypeIDTranslator::getFromComponentID(componentID);
				auto type = g_Reflection.getType(typeID);

				const char *typeDisplayName = type->m_displayName ? type->m_displayName : type->m_name;

				void *component = ecs.getComponentTypeless(entity, componentID);

				bool open = true;
				ImGui::PushID((void *)type->m_name);
				bool displayComponent = ImGui::CollapsingHeader(typeDisplayName, &open, ImGuiTreeNodeFlags_DefaultOpen);
				if (type->m_description && ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(type->m_description);
					ImGui::EndTooltip();
				}

				if (!open)
				{
					ImGui::OpenPopup("Delete Component?");
				}

				if (ImGui::BeginPopupModal("Delete Component?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Delete %s Component.\n", typeDisplayName);
					ImGui::Separator();

					if (ImGui::Button("OK", ImVec2(120, 0)))
					{
						ecs.removeComponentsTypeless(entity, 1, &componentID);
						displayComponent = false;
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
				ImGui::PopID();

				if (displayComponent)
				{
					for (auto f : type->m_fields)
					{
						const char *displayName = f.m_displayName ? f.m_displayName : f.m_name;

						if (f.m_typeID == getTypeID<float>())
						{
							ImGui::DragFloat(displayName, f.getAs<float>(component));
						}
						else if (f.m_typeID == getTypeID<glm::vec3>())
						{
							glm::vec3 *v = f.getAs<glm::vec3>(component);
							ImGui::DragFloat3(displayName, &v->x);
						}
						else if (f.m_typeID == getTypeID<bool>())
						{
							ImGui::Checkbox(displayName, f.getAs<bool>(component));
						}

						if (f.m_description && ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Text(f.m_description);
							ImGui::EndTooltip();
						}
					}
				}

			});
	}
	ImGui::End();
}

void InspectorWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool InspectorWindow::isVisible() const noexcept
{
	return m_visible;
}
