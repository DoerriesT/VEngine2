#include "CharacterMovementComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(CharacterMovementComponent &c, Stream &stream) noexcept
{
	return true;
}

void CharacterMovementComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	CharacterMovementComponent &c = *reinterpret_cast<CharacterMovementComponent *>(instance);

	int crouchStateInt = static_cast<int>(c.m_crouchState);
	const char *crouchStateStrings[] = { "STANDING", "ENTERING_CROUCH", "CROUCHING", "EXITING_CROUCH" };
	ImGui::Combo("Crouch State", &crouchStateInt, crouchStateStrings, IM_ARRAYSIZE(crouchStateStrings));
	c.m_crouchState = static_cast<CrouchState>(crouchStateInt);

	float crouchPercentage = c.m_crouchPercentage * 100.0f;
	ImGui::InputFloat("Crouch Percentage", &crouchPercentage);

	float velocity[] = { c.m_velocityX, c.m_velocityY, c.m_velocityZ };
	ImGui::InputFloat3("Velocity", velocity);

	ImGui::Checkbox("Active", &c.m_active);

	bool jumping = c.m_jumping;
	ImGui::Checkbox("Jumping", &jumping);

	ImGui::Separator();

	float movementInput[] = { c.m_movementXInputAxis, 0.0f, c.m_movementZInputAxis };
	ImGui::InputFloat3("Movement Input", movementInput);

	float turnRightInput = c.m_turnRightInputAxis;
	ImGui::InputFloat("Turn Input", &turnRightInput);

	bool jumpInput = c.m_jumpInputAction;
	ImGui::Checkbox("Jump Input", &jumpInput);

	bool enterCrouchInput = c.m_enterCrouchInputAction;
	ImGui::Checkbox("Enter Crouch Input", &enterCrouchInput);

	bool exitCrouchInput = c.m_exitCrouchInputAction;
	ImGui::Checkbox("Exit Crouch Input", &exitCrouchInput);
}

bool CharacterMovementComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<CharacterMovementComponent *>(instance), stream);
}

bool CharacterMovementComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<CharacterMovementComponent *>(instance), stream);
}

void CharacterMovementComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	CharacterMovementComponent &c = *reinterpret_cast<CharacterMovementComponent *>(instance);

	LuaUtil::setTableIntegerField(L, "m_crouchState", (lua_Integer)c.m_crouchState);
	LuaUtil::setTableNumberField(L, "m_crouchPercentage", (lua_Number)c.m_crouchPercentage);
	LuaUtil::setTableNumberField(L, "m_velocityX", (lua_Number)c.m_velocityX);
	LuaUtil::setTableNumberField(L, "m_velocityY", (lua_Number)c.m_velocityY);
	LuaUtil::setTableNumberField(L, "m_velocityZ", (lua_Number)c.m_velocityZ);
	LuaUtil::setTableBoolField(L, "m_active", c.m_active);
	LuaUtil::setTableBoolField(L, "m_jumping", c.m_jumping);
	LuaUtil::setTableNumberField(L, "m_movementXInputAxis", (lua_Number)c.m_movementXInputAxis);
	LuaUtil::setTableNumberField(L, "m_movementZInputAxis", (lua_Number)c.m_movementZInputAxis);
	LuaUtil::setTableNumberField(L, "m_turnRightInputAxis", (lua_Number)c.m_turnRightInputAxis);
	LuaUtil::setTableBoolField(L, "m_jumpInputAction", c.m_jumpInputAction);
	LuaUtil::setTableBoolField(L, "m_enterCrouchInputAction", c.m_enterCrouchInputAction);
	LuaUtil::setTableBoolField(L, "m_exitCrouchInputAction", c.m_exitCrouchInputAction);
}

void CharacterMovementComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	CharacterMovementComponent &c = *reinterpret_cast<CharacterMovementComponent *>(instance);

	c.m_crouchState = (CrouchState)LuaUtil::getTableIntegerField(L, "m_crouchState");
	c.m_crouchPercentage = (float)LuaUtil::getTableNumberField(L, "m_crouchState");
	c.m_velocityX = (float)LuaUtil::getTableNumberField(L, "m_velocityX");
	c.m_velocityY = (float)LuaUtil::getTableNumberField(L, "m_velocityY");
	c.m_velocityZ = (float)LuaUtil::getTableNumberField(L, "m_velocityZ");
	c.m_active = LuaUtil::getTableBoolField(L, "m_active");
	c.m_jumping = LuaUtil::getTableBoolField(L, "m_jumping");
	c.m_movementXInputAxis = (float)LuaUtil::getTableNumberField(L, "m_movementXInputAxis");
	c.m_movementZInputAxis = (float)LuaUtil::getTableNumberField(L, "m_movementZInputAxis");
	c.m_turnRightInputAxis = (float)LuaUtil::getTableNumberField(L, "m_turnRightInputAxis");
	c.m_jumpInputAction = LuaUtil::getTableBoolField(L, "m_jumpInputAction");
	c.m_enterCrouchInputAction = LuaUtil::getTableBoolField(L, "m_enterCrouchInputAction");
	c.m_exitCrouchInputAction = LuaUtil::getTableBoolField(L, "m_exitCrouchInputAction");
}
