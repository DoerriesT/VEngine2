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

						AttributeValue minValueAtt;
						const bool hasMinValueAtt = f.findAttribute(AttributeType::MIN, &minValueAtt);

						AttributeValue maxValueAtt;
						const bool hasMaxValueAtt = f.findAttribute(AttributeType::MAX, &maxValueAtt);

						AttributeValue getterAtt;
						const bool hasGetterAtt = f.findAttribute(AttributeType::GETTER, &getterAtt);

						AttributeValue setterAtt;
						const bool hasSetterAtt = f.findAttribute(AttributeType::SETTER, &setterAtt);

						AttributeValue uiElementAtt;
						const bool hasUIElementAtt = f.findAttribute(AttributeType::UI_ELEMENT, &uiElementAtt);

						if (f.m_typeID == getTypeID<float>())
						{
							float v_min = hasMinValueAtt ? minValueAtt.m_float : 0.0f;
							float v_max = hasMaxValueAtt ? maxValueAtt.m_float : 0.0f;

							if (hasMinValueAtt && !hasMaxValueAtt)
							{
								v_max = FLT_MAX;
							}
							else if (!hasMinValueAtt && hasMaxValueAtt)
							{
								v_min = -FLT_MAX;
							}

							float value = 0.0f;

							if (hasGetterAtt)
							{
								getterAtt.m_getter(component, getTypeID<float>(), &value);
							}
							else
							{
								value = *f.getAs<float>(component);
							}

							AttributeUIElements uiElement = hasUIElementAtt ? uiElementAtt.m_uiElement : AttributeUIElements::DEFAULT;

							bool modified = false;

							switch (uiElement)
							{
							case AttributeUIElements::DEFAULT:
							case AttributeUIElements::DRAG:
								modified = ImGui::DragFloat(displayName, &value, 1.0f, v_min, v_max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
								break;
							case AttributeUIElements::SLIDER:
								modified = ImGui::SliderFloat(displayName, &value, v_min, v_max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
								break;
							default:
								assert(false); // unsupported or unknown ui element
								break;
							}

							if (modified)
							{
								if (hasSetterAtt)
								{
									setterAtt.m_setter(component, getTypeID<float>(), &value);
								}
								else
								{
									f.setAs<float>(component, value);
								}
							}
						}
						else if (f.m_typeID == getTypeID<glm::quat>())
						{
							float v_min = hasMinValueAtt ? minValueAtt.m_float : 0.0f;
							float v_max = hasMaxValueAtt ? maxValueAtt.m_float : 0.0f;

							if (hasMinValueAtt && !hasMaxValueAtt)
							{
								v_max = FLT_MAX;
							}
							else if (!hasMinValueAtt && hasMaxValueAtt)
							{
								v_min = -FLT_MAX;
							}

							glm::quat value = {};

							if (hasGetterAtt)
							{
								getterAtt.m_getter(component, getTypeID<glm::quat>(), &value);
							}
							else
							{
								value = *f.getAs<glm::quat>(component);
							}

							AttributeUIElements uiElement = hasUIElementAtt ? uiElementAtt.m_uiElement : AttributeUIElements::DEFAULT;
							
							bool modified = false;

							switch (uiElement)
							{
							case AttributeUIElements::DEFAULT:
							case AttributeUIElements::DRAG:
								modified = ImGui::DragFloat4(displayName, &value.x, 1.0f, v_min, v_max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
								break;
							case AttributeUIElements::SLIDER:
								modified = ImGui::SliderFloat4(displayName, &value.x, v_min, v_max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
								if (modified)
								{
									value = glm::normalize(value);
								}
								break;
							case AttributeUIElements::EULER_ANGLES:
								glm::vec3 eulerAnglesBefore = glm::degrees(glm::eulerAngles(value));
								glm::vec3 eulerAngles = eulerAnglesBefore;
								modified = ImGui::DragFloat3(displayName, &eulerAngles.x, 1.0f, fmaxf(v_min, -360.0f), fminf(v_max, 360.0f), "%.3f", ImGuiSliderFlags_AlwaysClamp);
								if (modified)
								{
									glm::vec3 diff = eulerAngles - eulerAnglesBefore;
									glm::quat diffQuat = glm::quat(glm::radians(diff));

									value = value * diffQuat;
									value = glm::normalize(value);
								}
								
								break;
							default:
								assert(false); // unsupported or unknown ui element
								break;
							}

							if (modified)
							{
								if (hasSetterAtt)
								{
									setterAtt.m_setter(component, getTypeID<glm::quat>(), &value);
								}
								else
								{
									f.setAs<glm::quat>(component, value);
								}
							}
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
