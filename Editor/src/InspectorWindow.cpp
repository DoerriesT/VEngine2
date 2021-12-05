#include "InspectorWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/gui_helpers.h>
#include <graphics/imgui/ImGuizmo.h>
#include <Engine.h>
#include <graphics/Renderer.h>
#include <Level.h>
#include <ecs/ECSComponentInfoTable.h>
#include <component/OutlineComponent.h>
#include <component/TransformComponent.h>
#include <component/LightComponent.h>
#include <glm/gtx/transform.hpp>

static void drawSphereGeometry(Renderer &renderer, const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	const size_t segmentCount = 32;

	glm::mat4 transform = glm::translate(position) * glm::mat4_cast(rotation) * glm::scale(scale);

	for (size_t i = 0; i < segmentCount; ++i)
	{
		float angle0 = i / (float)segmentCount * (2.0f * glm::pi<float>());
		float angle1 = (i + 1) / (float)segmentCount * (2.0f * glm::pi<float>());
		float x0 = cosf(angle0);
		float x1 = cosf(angle1);
		float y0 = sinf(angle0);
		float y1 = sinf(angle1);

		glm::vec3 p0, p1;

		// horizontal
		p0 = transform * glm::vec4(x0, 0.0f, y0, 1.0f);
		p1 = transform * glm::vec4(x1, 0.0f, y1, 1.0f);
		renderer.drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderer.drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}

		// vertical z
		p0 = transform * glm::vec4(0.0f, x0, y0, 1.0f);
		p1 = transform * glm::vec4(0.0f, x1, y1, 1.0f);
		renderer.drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderer.drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}

		// vertical x
		p0 = transform * glm::vec4(x0, y0, 0.0f, 1.0f);
		p1 = transform * glm::vec4(x1, y1, 0.0f, 1.0f);
		renderer.drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderer.drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	}
}

static void drawBoxGeometry(Renderer &renderer, const glm::mat4 &transform, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	glm::vec3 corners[2][2][2];

	for (int x = 0; x < 2; ++x)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int z = 0; z < 2; ++z)
			{
				corners[x][y][z] = transform * glm::vec4(glm::vec3(x, y, z) * 2.0f - 1.0f, 1.0f);
			}
		}
	}

	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][0], corners[1][0][0], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][1], corners[1][0][1], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][0][1], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[1][0][0], corners[1][0][1], visibleColor, visibleColor);

	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][1][0], corners[1][1][0], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][1][1], corners[1][1][1], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][1][0], corners[0][1][1], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[1][1][0], corners[1][1][1], visibleColor, visibleColor);

	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][1][0], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][1], corners[0][1][1], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[1][0][0], corners[1][1][0], visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, corners[1][0][1], corners[1][1][1], visibleColor, visibleColor);

	if (drawOccluded)
	{
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][0], corners[1][0][0], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][1], corners[1][0][1], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][0][1], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[1][0][0], corners[1][0][1], occludedColor, occludedColor);

		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][1][0], corners[1][1][0], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][1][1], corners[1][1][1], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][1][0], corners[0][1][1], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[1][1][0], corners[1][1][1], occludedColor, occludedColor);

		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][1][0], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][1], corners[0][1][1], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[1][0][0], corners[1][1][0], occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, corners[1][0][1], corners[1][1][1], occludedColor, occludedColor);
	}
}

static void drawCappedCone(Renderer &renderer, glm::vec3 position, glm::quat rotation, float coneLength, float coneAngle, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	constexpr size_t segmentCount = 32;
	constexpr size_t capSegmentCount = 16;

	glm::mat3 rotation3x3 = glm::mat3_cast(rotation);

	// cone lines
	for (size_t i = 0; i < segmentCount; ++i)
	{
		float angle = i / (float)segmentCount * glm::two_pi<float>();
		float x = cosf(angle);
		float z = sinf(angle);

		float alpha = coneAngle * 0.5f;
		float hypotenuse = coneLength;
		float adjacent = cosf(alpha) * hypotenuse; // cosine = adjacent/ hypotenuse
		float opposite = tanf(alpha) * adjacent; // tangent = opposite / adjacent
		x *= opposite;
		z *= opposite;

		glm::vec3 p0 = position;
		glm::vec3 p1 = rotation3x3 * -glm::vec3(x, adjacent, z) + position;

		renderer.drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderer.drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	}

	// cap
	for (size_t i = 0; i < capSegmentCount; ++i)
	{
		const float offset = 0.5f * (glm::pi<float>() - coneAngle);
		float angle0 = i / (float)capSegmentCount * coneAngle + offset;
		float angle1 = (i + 1) / (float)capSegmentCount * coneAngle + offset;
		float x0 = cosf(angle0) * coneLength;
		float x1 = cosf(angle1) * coneLength;
		float z0 = sinf(angle0) * coneLength;
		float z1 = sinf(angle1) * coneLength;
	
		glm::vec3 p0, p1;
	
		// horizontal
		p0 = rotation3x3 * -glm::vec3(x0, z0, 0.0f) + position;
		p1 = rotation3x3 * -glm::vec3(x1, z1, 0.0f) + position;
		renderer.drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderer.drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	
		// vertical
		p0 = rotation3x3 * -glm::vec3(0.0f, z0, x0) + position;
		p1 = rotation3x3 * -glm::vec3(0.0f, z1, x1) + position;
		renderer.drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderer.drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	}
}

static void drawArrow(Renderer &renderer, glm::vec3 position, glm::quat rotation, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	glm::mat3 rotation3x3 = glm::mat3_cast(rotation);

	glm::vec3 arrowHead = rotation3x3 * glm::vec3(0.0f, 0.0f, 0.0f) + position;
	glm::vec3 arrowTail = rotation3x3 * glm::vec3(0.0f, 3.0f, 0.0f) + position;
	glm::vec3 arrowHeadX0 = rotation3x3 * glm::vec3(0.25f, 0.25f, 0.0f) + position;
	glm::vec3 arrowHeadX1 = rotation3x3 * glm::vec3(-0.25f, 0.25f, 0.0f) + position;
	glm::vec3 arrowHeadZ0 = rotation3x3 * glm::vec3(0.0f, 0.25f, 0.25f) + position;
	glm::vec3 arrowHeadZ1 = rotation3x3 * glm::vec3(0.0f, 0.25f, -0.25f) + position;

	renderer.drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowTail, visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadX0, visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadX1, visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadZ0, visibleColor, visibleColor);
	renderer.drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadZ1, visibleColor, visibleColor);

	if (drawOccluded)
	{
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowTail, occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadX0, occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadX1, occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadZ0, occludedColor, occludedColor);
		renderer.drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadZ1, occludedColor, occludedColor);
	}
}

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

			// draw light debug geometry
			if (ecs.hasComponents<TransformComponent, LightComponent>(entity))
			{
				auto *tc = ecs.getComponent<TransformComponent>(entity);
				auto *lc = ecs.getComponent<LightComponent>(entity);

				const glm::vec4 k_visibleLightDebugColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
				const glm::vec4 k_occludedLightDebugColor = glm::vec4(0.5f, 0.25f, 0.0f, 1.0f);

				switch (lc->m_type)
				{
				case LightComponent::Type::Point:
					drawSphereGeometry(*m_engine->getRenderer(), tc->m_transform.m_translation, tc->m_transform.m_rotation, glm::vec3(lc->m_radius), k_visibleLightDebugColor, k_occludedLightDebugColor, true); break;
				case LightComponent::Type::Spot:
					drawCappedCone(*m_engine->getRenderer(), tc->m_transform.m_translation, tc->m_transform.m_rotation, lc->m_radius, lc->m_outerAngle, k_visibleLightDebugColor, k_occludedLightDebugColor, true); break;
				case LightComponent::Type::Directional:
					drawArrow(*m_engine->getRenderer(), tc->m_transform.m_translation, tc->m_transform.m_rotation, k_visibleLightDebugColor, k_occludedLightDebugColor, true); break;
				default:
					assert(false);
					break;
				}
			}

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
						// skip components already present on the entity and hidden editor components
						if (compMask[componentID] || componentID == ComponentIDGenerator::getID<EditorOutlineComponent>())
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
					if (componentID == ComponentIDGenerator::getID<EditorOutlineComponent>())
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
						compInfo.m_onGUI(component);
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
