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
#include <component/TransformComponent.h>
#include <component/CameraComponent.h>
#include <component/LightComponent.h>
#include <Editor.h>
#include <graphics/Renderer.h>
#include <input/UserInput.h>
#include <input/FPSCameraController.h>
#include <graphics/Camera.h>
#include <Level.h>

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

class DummyGameLogic : public IGameLogic
{
public:
	void init(Engine *engine) noexcept override
	{
		m_engine = engine;
		m_fpsCameraController = new FPSCameraController(m_engine->getUserInput());
		m_cameraEntity = m_engine->getECS()->createEntity<TransformComponent, CameraComponent>();
		m_engine->getRenderer()->setCameraEntity(m_cameraEntity);
		m_engine->getLevel()->addEntity(m_cameraEntity, "First Person Camera");
	}

	void update(float deltaTime) noexcept override
	{
		m_engine->getRenderer()->setCameraEntity(m_cameraEntity);

		TransformComponent *tc = m_engine->getECS()->getComponent<TransformComponent>(m_cameraEntity);
		CameraComponent *cc = m_engine->getECS()->getComponent<CameraComponent>(m_cameraEntity);
		assert(tc && cc);

		uint32_t swWidth, swHeight, w, h;
		m_engine->getResolution(&swWidth, &swHeight, &w, &h);

		// make sure aspect ratio of editor camera is correct
		cc->m_aspectRatio = (w > 0 && h > 0) ? w / (float)h : 1.0f;

		Camera camera(*tc, *cc);

		m_fpsCameraController->update(deltaTime, camera);
	}

	void shutdown() noexcept override
	{

	}

private:
	Engine *m_engine = nullptr;
	FPSCameraController *m_fpsCameraController = nullptr;
	EntityID m_cameraEntity;
};



int main(int argc, char *argv[])
{
	DummyGameLogic gameLogic;
	Editor editor(&gameLogic);
	Engine engine;
	return engine.start(argc, argv, &editor);
}