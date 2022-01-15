#pragma once
#include "Handles.h"
#include "asset/TextureAsset.h"

struct lua_State;
struct TransformComponent;
class Renderer;

struct ParticipatingMediumComponent
{
	enum class Type
	{
		Global, Local
	};

	Type m_type;
	uint32_t m_albedo = 0xFFFFFFFF;
	float m_extinction = 0.01f;
	uint32_t m_emissiveColor = 0xFFFFFFFF;
	float m_emissiveIntensity = 0.0f;
	float m_phaseAnisotropy = 0.0f;
	bool m_heightFogEnabled = true;
	float m_heightFogStart = 0.0f;
	float m_heightFogFalloff = 0.1f;
	float m_maxHeight = 100.0f;
	Asset<TextureAssetData> m_densityTexture = {};
	float m_textureScale = 1.0f;
	float m_textureBias[3] = {};
	bool m_spherical = false;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "ParticipatingMediumComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Participating Medium Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};