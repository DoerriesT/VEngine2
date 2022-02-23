#include "InspectorWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/gui_helpers.h>
#include <Engine.h>
#include <graphics/Renderer.h>
#include <ecs/ECSComponentInfoTable.h>
#include <component/EntityMetaComponent.h>
#include <component/OutlineComponent.h>
#include <component/TransformComponent.h>

InspectorWindow::InspectorWindow(Engine *engine) noexcept
	:m_engine(engine)
{
}

void InspectorWindow::draw(EntityID entity) noexcept
{
	if (!m_visible)
	{
		return;
	}

	ImGui::Begin("Inspector");
	{
		ECS &ecs = *m_engine->getECS();

		// get meta component of currently selected entity
		EntityMetaComponent *metaComp = nullptr;
		if (entity != k_nullEntity)
		{
			metaComp = ecs.getComponent<EntityMetaComponent>(entity);
		}

		// copy entity name to scratch memory
		if (entity != m_lastDisplayedEntity)
		{
			m_lastDisplayedEntity = entity;
			memset(m_nameStringTemp, 0, sizeof(m_nameStringTemp));

			if (metaComp)
			{
				static_assert(sizeof(m_nameStringTemp) == EntityMetaComponent::k_maxNameLength);
				strcpy_s(m_nameStringTemp, metaComp->m_name);
			}
			else
			{
				strcpy_s(m_nameStringTemp, "*Nameless Entity*");
			}
		}

		if (entity != k_nullEntity)
		{
			if (metaComp)
			{
				if (ImGui::InputText("Entity Name", m_nameStringTemp, IM_ARRAYSIZE(m_nameStringTemp), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					strcpy_s(metaComp->m_name, m_nameStringTemp);
				}

				char uuidStr[TUUID::k_uuidStringSize];
				metaComp->m_uuid.toString(uuidStr);
				ImGui::InputText("ID", uuidStr, sizeof(uuidStr), ImGuiInputTextFlags_ReadOnly);
			}

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
						// skip components already present on the entity and hidden editor components
						if (compMask[componentID]
							|| componentID == ComponentIDGenerator::getID<EditorOutlineComponent>()
							|| componentID == ComponentIDGenerator::getID<EntityMetaComponent>())
						{
							return;
						}

						if (ImGui::Selectable(ECSComponentInfoTable::getComponentInfo(componentID).m_displayName))
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
					// skip hidden editor components
					if (componentID == ComponentIDGenerator::getID<EditorOutlineComponent>()
						|| componentID == ComponentIDGenerator::getID<EntityMetaComponent>())
					{
						return;
					}

					const auto &compInfo = ECSComponentInfoTable::getComponentInfo(componentID);

					void *component = ecs.getComponentTypeless(entity, componentID);

					bool open = true;
					ImGui::PushID((const void *)compInfo.m_name);
					bool displayComponent = ImGui::CollapsingHeader(compInfo.m_displayName, &open, ImGuiTreeNodeFlags_DefaultOpen);
					ImGuiHelpers::Tooltip(compInfo.m_tooltip);

					if (!open)
					{
						ImGui::OpenPopup("Delete Component?");
					}

					if (ImGui::BeginPopupModal("Delete Component?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
					{
						ImGui::Text("Delete %s Component.\n", compInfo.m_displayName);
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
						auto *tc = ecs.getComponent<TransformComponent>(entity);
						compInfo.m_onGUI(&ecs, entity, component, m_engine->getRenderer(), tc);
					}

				});
		}
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
