#include "AssetHandlerRegistration.h"
#include "AnimationClipAssetHandler.h"
#include "MeshAssetHandler.h"
#include "SkeletonAssetHandler.h"
#include "TextureAssetHandler.h"
#include "ScriptAssetHandler.h"
#include "asset/AssetManager.h"

void AssetHandlerRegistration::registerHandlers(Renderer *renderer, Physics *physics, AnimationSystem *animation) noexcept
{
	AnimationClipAssetHandler::init(AssetManager::get(), animation);
	MeshAssetHandler::init(AssetManager::get(), renderer, physics);
	SkeletonAssetHandler::init(AssetManager::get(), animation);
	TextureAssetHandler::init(AssetManager::get(), renderer);
	ScriptAssetHandler::init(AssetManager::get());
}

void AssetHandlerRegistration::unregisterHandlers() noexcept
{
	AnimationClipAssetHandler::shutdown();
	MeshAssetHandler::shutdown();
	SkeletonAssetHandler::shutdown();
	TextureAssetHandler::shutdown();
	ScriptAssetHandler::shutdown();
}
