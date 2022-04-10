#pragma once

class Renderer;
class Physics;

namespace AssetHandlerRegistration
{
	void createAndRegisterHandlers(Renderer *renderer, Physics *physics) noexcept;
	void shutdownHandlers() noexcept;
}