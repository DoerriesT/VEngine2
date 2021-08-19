#include "InspectorWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/gui_helpers.h>
#include <Engine.h>
#include <Level.h>
#include <ecs/ECSTypeIDTranslator.h>

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

	if (entity != m_lastDisplayedEntity)
	{
		m_lastDisplayedEntity = entity;
		memset(m_nameStringTemp, 0, sizeof(m_nameStringTemp));

		// look up name of selected entity
		if (entity != k_nullEntity)
		{
			Level *level = m_engine->getLevel();

			const auto &sceneGraphNodes = level->getSceneGraphNodes();

			for (auto *n : sceneGraphNodes)
			{
				if (n->m_entity == entity)
				{
					m_currentSceneGraphNode = n;
					break;
				}
			}

			static_assert(sizeof(m_nameStringTemp) == SceneGraphNode::k_maxNameLength);
			strcpy_s(m_nameStringTemp, m_currentSceneGraphNode->m_name);
		}
	}

	if (entity != k_nullEntity)
	{
		ECS &ecs = *m_engine->getECS();
		
		if (ImGui::InputText("Entity Name", m_nameStringTemp, IM_ARRAYSIZE(m_nameStringTemp), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			strcpy_s(m_currentSceneGraphNode->m_name, m_nameStringTemp);
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
					if (compMask[componentID])
					{
						return;
					}

					if (ImGui::Selectable(ECSTypeIDTranslator::getBasicComponentReflectionData(componentID).m_displayName))
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
				const auto &basicCompInfo = ECSTypeIDTranslator::getBasicComponentReflectionData(componentID);

				void *component = ecs.getComponentTypeless(entity, componentID);

				bool open = true;
				ImGui::PushID((const void *)basicCompInfo.m_name);
				bool displayComponent = ImGui::CollapsingHeader(basicCompInfo.m_displayName, &open, ImGuiTreeNodeFlags_DefaultOpen);
				ImGuiHelpers::Tooltip(basicCompInfo.m_tooltip);

				if (!open)
				{
					ImGui::OpenPopup("Delete Component?");
				}

				if (ImGui::BeginPopupModal("Delete Component?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Delete %s Component.\n", basicCompInfo.m_displayName);
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
					basicCompInfo.m_onGUI(component);
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
