#include "Engine.h"
#include <EASTL/fixed_vector.h>
#include <task/Task.h>
#include <stdio.h>
#include <ecs/ECS.h>
#include <ecs/ECSTypeIDTranslator.h>
#include <reflection/Reflection.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <IGameLogic.h>
#include <graphics/imgui/imgui.h>

Reflection g_Reflection;

DEFINE_TYPE_INFO(glm::vec3, "24BC398F-AA63-4674-BA24-09F222B3C8FD")

template<>
void reflect<glm::vec3>(Reflection &refl) noexcept
{
	refl.addClass<glm::vec3>()
		.addField("x", &glm::vec3::x, "X", "The x-component")
		.addField("y", &glm::vec3::y, "Y", "The y-component")
		.addField("z", &glm::vec3::z, "Z", "The z-component");
}

struct TransformComponent
{
	glm::vec3 m_translation;
	glm::vec3 m_rotation;
	float m_scale;

	static void reflect(Reflection &refl) noexcept
	{
		refl.addClass<TransformComponent>("Transform Component")
			.addField("m_translation", &TransformComponent::m_translation, "Translation", "The translation of the entity.")
			.addField("m_rotation", &TransformComponent::m_rotation, "Rotation", "The rotation in euler angles of the entity.")
			.addField("m_scale", &TransformComponent::m_scale, "Scale", "The scale of the entity.");
	}
};
DEFINE_TYPE_INFO(TransformComponent, "ABA16580-3F7A-404A-BE66-77353FD6E550")

struct CameraComponent
{
	float m_aspectRatio = 1.0f;
	float m_fovy = 1.57f; // 90°
	float m_near = 0.1f;
	float m_far = 300.0f;
	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;

	static void reflect(Reflection &refl) noexcept
	{
		refl.addClass<CameraComponent>("Camera Component")
			.addField("m_fovy", &CameraComponent::m_fovy, "Field of View", "The vertical field of view.")
			.addField("m_near", &CameraComponent::m_near, "Near Plane", "The distance of the camera near plane.")
			.addField("m_far", &CameraComponent::m_far, "Far Plane", "The distance of the camera far plane.");
	}
};
DEFINE_TYPE_INFO(CameraComponent, "2F315E1B-3277-4A40-8BBC-A5BEEB290A7B")

struct LightComponent
{
	glm::vec3 m_color = glm::vec3(1.0f);
	float m_luminousPower = 1000.0f;
	float m_radius = 8.0f;
	float m_outerAngle = 0.785f;
	float m_innerAngle = 0.26f;
	bool m_shadows = false;
	bool m_volumetricShadows = false; // requires m_shadows to be true

	static void reflect(Reflection &refl) noexcept
	{
		refl.addClass<LightComponent>("Light Component", "Represents a light source.")
			.addField("m_color", &LightComponent::m_color, "Color")
			.addField("m_luminousPower", &LightComponent::m_luminousPower, "Intensity", "Intensity in lumens.")
			.addField("m_outerAngle", &LightComponent::m_outerAngle, "Outer Angle", "Outer angle of the spot light.")
			.addField("m_innerAngle", &LightComponent::m_innerAngle, "Inner Radius", "Inner angle of the spot light.")
			.addField("m_shadows", &LightComponent::m_shadows, "Shadows", "Enables shadow casting for this light.")
			.addField("m_volumetricShadows", &LightComponent::m_volumetricShadows, "Volumetric Radius", "Enables volumetric shadow casting for this light.");
	}
};
DEFINE_TYPE_INFO(LightComponent, "04955E50-AFC4-4595-A1A2-4E4A2B16797E")

void visitType(const Reflection &r, const TypeID &typeID, const char *fieldName = nullptr, unsigned int indent = 0)
{
	for (size_t i = 0; i < indent; ++i)
	{
		printf(" ");
	}

	auto type = r.getType(typeID);
	printf("%s %s\n", type->m_name, fieldName ? fieldName : "");

	if (type->m_typeCategory == TYPE_CATEGORY_POINTER || type->m_typeCategory == TYPE_CATEGORY_ARRAY)
	{
		visitType(r, type->m_pointedToTypeID, nullptr, indent + 4);
	}
	else
	{
		for (auto f : type->m_fields)
		{
			visitType(r, f.m_typeID, f.m_name, indent + 4);
		}
	}
	
}

ECS ecs;

class DummyGameLogic : public IGameLogic
{
public:
	void init(Engine *engine) noexcept override
	{
		m_engine = engine;
		m_currentEntity = ecs.createEntity<TransformComponent, CameraComponent>();
	}

	void update(float deltaTime) noexcept override
	{
		ImGui::Begin("Inspector");
		{
			auto forEachComponentType = [](const ComponentMask &mask, const eastl::function<void(size_t index, ComponentID componentID)> &f) noexcept
			{
				size_t componentIndex = 0;
				auto componentID = mask.DoFindFirst();
				while (componentID != mask.kSize)
				{
					f(componentIndex, componentID);

					componentID = mask.DoFindNext(componentID);
					++componentIndex;
				}
			};

			auto compMask = ecs.getComponentMask(m_currentEntity);

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
							ecs.addComponentsTypeless(m_currentEntity, 1, &componentID);
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

					void *component = ecs.getComponentTypeless(m_currentEntity, componentID);

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
							ecs.removeComponentsTypeless(m_currentEntity, 1, &componentID);
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

	void shutdown() noexcept override
	{

	}

private:
	Engine *m_engine = nullptr;
	EntityID m_currentEntity;
};

template<typename T>
void registerComponent()
{
	T::reflect(g_Reflection);
	ecs.registerComponent<T>();
	ECSTypeIDTranslator::registerType<T>();
}

int main(int argc, char *argv[])
{
	registerComponent<TransformComponent>();
	registerComponent<CameraComponent>();
	registerComponent<LightComponent>();

	DummyGameLogic gameLogic;
	Engine engine;
	return engine.start(argc, argv, &gameLogic);
}