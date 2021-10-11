#pragma once

class Renderer;
class Physics;
class AnimationSystem;

namespace AssetHandlerRegistration
{
	void createAndRegisterHandlers(Renderer *renderer, Physics *physics, AnimationSystem *animation) noexcept;
	void shutdownHandlers() noexcept;
}