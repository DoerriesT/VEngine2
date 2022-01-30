#pragma once
#include <stdint.h>

struct lua_State;
struct TransformComponent;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct IrradianceVolumeComponent
{
	uint32_t m_resolutionX = 4;
	uint32_t m_resolutionY = 4;
	uint32_t m_resolutionZ = 4;
	float m_selfShadowBias = 0.3f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 50.0f;
	uint32_t m_internalHandle = 0;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "IrradianceVolumeComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Irradiance Volume Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};