#include "AssetHandlerRegistration.h"
#include "AnimationGraphAssetHandler.h"
#include "AnimationClipAssetHandler.h"
#include "MeshAssetHandler.h"
#include "SkeletonAssetHandler.h"
#include "TextureAssetHandler.h"
#include "MaterialAssetHandler.h"
#include "ScriptAssetHandler.h"
#include "asset/AssetManager.h"

void AssetHandlerRegistration::createAndRegisterHandlers(Renderer *renderer, Physics *physics) noexcept
{
	AnimationGraphAssetHandler::init(AssetManager::get());
	AnimationClipAssetHandler::init(AssetManager::get());
	MeshAssetHandler::init(AssetManager::get(), renderer, physics);
	SkeletonAssetHandler::init(AssetManager::get());
	TextureAssetHandler::init(AssetManager::get(), renderer);
	MaterialAssetHandler::init(AssetManager::get(), renderer);
	ScriptAssetHandler::init(AssetManager::get());
}

void AssetHandlerRegistration::shutdownHandlers() noexcept
{
	AnimationClipAssetHandler::shutdown();
	MeshAssetHandler::shutdown();
	SkeletonAssetHandler::shutdown();
	TextureAssetHandler::shutdown();
	MaterialAssetHandler::shutdown();
	ScriptAssetHandler::shutdown();
}
