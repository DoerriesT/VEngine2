#pragma once
#include <stdint.h>

struct lua_State;
struct TransformComponent;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

enum class CrouchState : uint32_t
{
	STANDING = 0,
	ENTERING_CROUCH = 1,
	CROUCHING = 2,
	EXITING_CROUCH = 3
};

struct CharacterMovementComponent
{
	// internal
	CrouchState m_crouchState;
	float m_crouchPercentage;
	float m_velocityX;
	float m_velocityY;
	float m_velocityZ;
	bool m_active;
	bool m_jumping;

	// input
	float m_movementXInputAxis;
	float m_movementZInputAxis;
	float m_turnRightInputAxis;
	bool m_jumpInputAction;
	bool m_enterCrouchInputAction;
	bool m_exitCrouchInputAction;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "CharacterMovementComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Character Movement Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};