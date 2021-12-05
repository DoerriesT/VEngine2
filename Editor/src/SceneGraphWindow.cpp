#include "SceneGraphWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/ImGuizmo.h>
#include <Engine.h>
#include <Level.h>
#include <input/InputTokens.h>
#include <component/OutlineComponent.h>
#include <component/TransformComponent.h>
#include <component/LightComponent.h>

SceneGraphWindow::SceneGraphWindow(Engine *engine) noexcept
	:m_engine(engine)
{
}

void SceneGraphWindow::draw(bool viewportHasFocus) noexcept
{
	if (!m_visible)
	{
		return;
	}

	ImGui::Begin("Scene");
	{
		auto *level = m_engine->getLevel();
		const auto &sceneGraphNodes = level->getSceneGraphNodes();
		ECS &ecs = *m_engine->getECS();

		if (ImGui::Button("Add Entity"))
		{
			ImGui::OpenPopup("add_entity_popup");
		}

		if (ImGui::BeginPopup("add_entity_popup"))
		{
			if (ImGui::Selectable("Empty Entity"))
			{
				auto entity = ecs.createEntity();
				level->addEntity(entity, "New Entity");
				m_selectedEntity = entity;
			}
			if (ImGui::Selectable("Point Light"))
			{
				TransformComponent tc{};
				LightComponent lc{};
				lc.m_type = LightComponent::Type::Point;
				auto entity = ecs.createEntity<TransformComponent, LightComponent>(tc, lc);
				level->addEntity(entity, "Point Light");
				m_selectedEntity = entity;
			}
			if (ImGui::Selectable("Spot Light"))
			{
				TransformComponent tc{};
				LightComponent lc{};
				lc.m_type = LightComponent::Type::Spot;
				auto entity = ecs.createEntity<TransformComponent, LightComponent>(tc, lc);
				level->addEntity(entity, "Spot Light");
				m_selectedEntity = entity;
			}
			if (ImGui::Selectable("Directional Light"))
			{
				TransformComponent tc{};
				LightComponent lc{};
				lc.m_type = LightComponent::Type::Directional;
				auto entity = ecs.createEntity<TransformComponent, LightComponent>(tc, lc);
				level->addEntity(entity, "Directional Light");
				m_selectedEntity = entity;
			}

			ImGui::EndPopup();
		}

		ImGui::Separator();

		{
			EntityID selected = k_nullEntity;

			ImGui::BeginChild("##Child1");
			{
				ImGuiTreeNodeFlags treeBaseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

				EntityID renderPickingSelectedEntity = m_engine->getPickedEntity();
				const bool leftClicked = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
				const bool hasFocusSelectionFocus = viewportHasFocus && !(ImGuizmo::IsOver() || ImGuizmo::IsUsing());

				uint32_t entityCount = 0;
				for (auto &node : sceneGraphNodes)
				{
					ImGuiTreeNodeFlags nodeFlags = treeBaseFlags;

					if (node->m_entity == m_selectedEntity)
					{
						nodeFlags |= ImGuiTreeNodeFlags_Selected;
					}

					nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // ImGuiTreeNodeFlags_Bullet
					ImGui::TreeNodeEx((void *)(intptr_t)entityCount, nodeFlags, node->m_name);
					if (ImGui::IsItemClicked() || (leftClicked && hasFocusSelectionFocus && (node->m_entity == renderPickingSelectedEntity)))
					{
						selected = node->m_entity;
					}
					ImGui::PushID(&node->m_entity);
					if (ImGui::BeginPopupContextItem("delete_entity_context_menu"))
					{
						if (ImGui::Selectable("Delete"))
						{
							m_toDeleteEntity = node->m_entity;
						}
						ImGui::EndPopup();
					}
					ImGui::PopID();
					++entityCount;
				}
			}
			ImGui::EndChild();

			if (selected != k_nullEntity)
			{
				// disable outline on old entity
				if (m_selectedEntity != k_nullEntity)
				{
					auto *editorOutlineComp = m_engine->getECS()->getComponent<EditorOutlineComponent>(m_selectedEntity);
					if (editorOutlineComp)
					{
						editorOutlineComp->m_outlined = false;
					}
				}

				// unselect entity
				if (selected == m_selectedEntity)
				{
					m_selectedEntity = k_nullEntity;
				}
				// select different entity
				else
				{
					m_selectedEntity = selected;

					EditorOutlineComponent editorOutlineComp{};
					editorOutlineComp.m_outlined = true;
					m_engine->getECS()->addComponent<EditorOutlineComponent>(selected, editorOutlineComp);
				}
			}

			// delete entity by pressing delete key
			if (m_selectedEntity != k_nullEntity && m_toDeleteEntity == k_nullEntity)
			{
				if (ImGui::IsKeyPressed((int)InputKey::DELETE, false))
					m_toDeleteEntity = m_selectedEntity;
			}

			if (m_toDeleteEntity != k_nullEntity)
			{
				ImGui::OpenPopup("Delete Entity?");
			}

			if (ImGui::BeginPopupModal("Delete Entity?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Delete Entity.\nThis operation cannot be undone!\n\n");
				ImGui::Separator();

				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					level->removeEntity(m_toDeleteEntity);
					ecs.destroyEntity(m_toDeleteEntity);

					m_selectedEntity = m_selectedEntity == m_toDeleteEntity ? k_nullEntity : m_selectedEntity;
					m_toDeleteEntity = k_nullEntity;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					m_toDeleteEntity = k_nullEntity;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
	}
	ImGui::End();
}

void SceneGraphWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool SceneGraphWindow::isVisible() const noexcept
{
	return m_visible;
}

EntityID SceneGraphWindow::getSelectedEntity() const
{
	return m_selectedEntity;
}
