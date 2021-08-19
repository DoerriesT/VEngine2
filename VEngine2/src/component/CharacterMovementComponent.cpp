#include "CharacterMovementComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void CharacterMovementComponent::onGUI(void *instance) noexcept
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
