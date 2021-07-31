#pragma once

class ECS;

namespace ComponentRegistration
{
	void registerAllComponents(ECS *ecs) noexcept;
}