#pragma once

class Renderer;
class Physics;
class AnimationSystem;

namespace AssetHandlerRegistration
{
	void registerHandlers(Renderer *renderer, Physics *physics, AnimationSystem *animation) noexcept;
	void unregisterHandlers() noexcept;
}