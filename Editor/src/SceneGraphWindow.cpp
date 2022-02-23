#include "SceneGraphWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/ImGuizmo.h>
#include <Engine.h>
#include <input/InputTokens.h>
#include <component/EntityMetaComponent.h>
#include <component/OutlineComponent.h>
#include <component/TransformComponent.h>
#include <component/MeshComponent.h>
#include <component/LightComponent.h>
#include <component/ParticipatingMediumComponent.h>
#include <component/ReflectionProbeComponent.h>
#include <component/IrradianceVolumeComponent.h>
#include <ecs/ECS.h>
#include <TransformHierarchy.h>
#include <EASTL/sort.h>
#include <EASTL/fixed_vector.h>

#ifdef DELETE
#undef DELETE
#endif

namespace
{
	struct EntityIDNamePair
	{
		EntityID m_entity;
		const char *m_name;
	};

	void sortEntitiesByName(size_t count, EntityIDNamePair *entities) noexcept
	{
		eastl::sort(entities, entities + count, [&](auto lhs, auto rhs)
			{
				if (lhs.m_name && rhs.m_name)
				{
					return strcmp(lhs.m_name, rhs.m_name) < 0;
				}
				else if (lhs.m_name)
				{
					return true;
				}
				else
				{
					return false;
				}
			});
	}
}

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
		auto &ecs = *m_engine->getECS();

		if (ImGui::Button("Add Entity"))
		{
			ImGui::OpenPopup("add_entity_popup");
		}

		EntityID selected = k_nullEntity;

		if (ImGui::BeginPopup("add_entity_popup"))
		{
			if (ImGui::Selectable("Empty Entity"))
			{
				selected = ecs.createEntity<EntityMetaComponent>(EntityMetaComponent("New Entity"));
			}
			if (ImGui::Selectable("Mesh"))
			{
				TransformComponent tc{};
				MeshComponent mc{};
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, MeshComponent>(EntityMetaComponent("Mesh"), tc, mc);
			}
			if (ImGui::Selectable("Point Light"))
			{
				TransformComponent tc{};
				LightComponent lc{};
				lc.m_type = LightComponent::Type::Point;
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, LightComponent>(EntityMetaComponent("Point Light"), tc, lc);

			}
			if (ImGui::Selectable("Spot Light"))
			{
				TransformComponent tc{};
				LightComponent lc{};
				lc.m_type = LightComponent::Type::Spot;
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, LightComponent>(EntityMetaComponent("Spot Light"), tc, lc);
			}
			if (ImGui::Selectable("Directional Light"))
			{
				TransformComponent tc{};
				LightComponent lc{};
				lc.m_type = LightComponent::Type::Directional;
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, LightComponent>(EntityMetaComponent("Directional Light"), tc, lc);
			}
			if (ImGui::Selectable("Participating Medium"))
			{
				TransformComponent tc{};
				ParticipatingMediumComponent pmc{};
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, ParticipatingMediumComponent>(EntityMetaComponent("Participating Medium"), tc, pmc);
			}
			if (ImGui::Selectable("Reflection Probe"))
			{
				TransformComponent tc{};
				ReflectionProbeComponent pc{};
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe"), tc, pc);
			}
			if (ImGui::Selectable("Irradiance Volume"))
			{
				TransformComponent tc{};
				IrradianceVolumeComponent vc{};
				selected = ecs.createEntity<EntityMetaComponent, TransformComponent, IrradianceVolumeComponent>(EntityMetaComponent("Irradiance Volume"), tc, vc);
			}

			ImGui::EndPopup();
		}

		ImGui::Separator();

		{
			ImGui::BeginChild("##Child1");
			{
				eastl::fixed_vector<EntityIDNamePair, 32> rootNodes;
				IterateQuery query;
				ecs.setIterateQueryOptionalComponents<TransformComponent, EntityMetaComponent>(query);
				ecs.iterate<TransformComponent, EntityMetaComponent>(query, [&](size_t count, const EntityID *entities, TransformComponent *transC, EntityMetaComponent *metaC)
					{
						for (size_t i = 0; i < count; ++i)
						{
							EntityIDNamePair pair{};
							pair.m_entity = entities[i];
							pair.m_name = metaC ? metaC[i].m_name : nullptr;

							if (!transC || transC[i].m_parentEntity == k_nullEntity)
							{
								rootNodes.push_back(pair);
							}
						}
					});

				sortEntitiesByName(rootNodes.size(), rootNodes.data());

				for (auto n : rootNodes)
				{
					drawTreeNode(n.m_entity, &selected);
				}

				const bool leftClicked = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
				const bool hasFocusSelectionFocus = viewportHasFocus && !(ImGuizmo::IsOver() || ImGuizmo::IsUsing());
				if (selected == k_nullEntity && (leftClicked && hasFocusSelectionFocus))
				{
					selected = m_engine->getPickedEntity();
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
					TransformHierarchy::detach(&ecs, m_toDeleteEntity, true);
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

	if (m_reparentDrop)
	{
		assert(m_reparentChild != k_nullEntity);
		TransformHierarchy::attach(m_engine->getECS(), m_reparentChild, m_reparentParent, false);
		m_reparentDrop = false;
	}
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

void SceneGraphWindow::drawTreeNode(EntityID entity, EntityID *selected) noexcept
{
	ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
	if (entity == m_selectedEntity)
	{
		nodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	auto *ecs = m_engine->getECS();
	auto *tc = ecs->getComponent<TransformComponent>(entity);
	auto *emc = ecs->getComponent<EntityMetaComponent>(entity);

	if (!tc || tc->m_childEntity == k_nullEntity)
	{
		nodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}

	bool nodeOpen = ImGui::TreeNodeEx((void *)entity, nodeFlags, emc ? emc->m_name : "*Nameless Entity*");

	// click selection
	if (ImGui::IsItemClicked())
	{
		*selected = entity;
	}

	// context menu
	ImGui::PushID("delete_entity_context_menu");
	if (ImGui::BeginPopupContextItem("delete_entity_context_menu"))
	{
		if (ImGui::Selectable("Delete"))
		{
			m_toDeleteEntity = entity;
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (ImGui::BeginDragDropTarget())
	{
		if (ImGui::AcceptDragDropPayload("SCENE_GRAPH_NODE"))
		{
			// reparent
			m_reparentParent = entity;
			m_reparentDrop = true;
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_GRAPH_NODE", nullptr, 0);
		m_reparentChild = entity;
		m_reparentParent = k_nullEntity;

		ImGui::Text(emc ? emc->m_name : "*Nameless Entity*");

		ImGui::EndDragDropSource();
	}

	// draw children
	if (nodeOpen)
	{
		if (tc)
		{
			eastl::fixed_vector<EntityIDNamePair, 16> children;

			for (auto child = tc->m_childEntity; child != k_nullEntity; child = TransformHierarchy::getNextSibling(ecs, child))
			{
				auto *cemc = ecs->getComponent<EntityMetaComponent>(child);

				EntityIDNamePair pair{};
				pair.m_entity = child;
				pair.m_name = cemc ? cemc->m_name : nullptr;

				children.push_back(pair);
			}

			sortEntitiesByName(children.size(), children.data());

			for (auto child : children)
			{
				drawTreeNode(child.m_entity, selected);
			}
		}

		ImGui::TreePop();
	}
}