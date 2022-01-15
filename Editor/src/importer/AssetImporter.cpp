#include "AssetImporter.h"
#include <Log.h>
#include "loader/WavefrontOBJLoader.h"
#include "loader/GLTFLoader.h"
#include "SkeletonImporter.h"
#include "AnimationClipImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
#include <asset/Asset.h>

struct LoadedModel;

namespace
{
	typedef bool (*LoaderFuncPtr)(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes, bool importSkeletons, bool importAnimations, LoadedModel &model);
}

bool AssetImporter::importAsset(const ImportOptions &importOptions, Physics *physics, const char *nativeSrcPath, const char *dstPath) noexcept
{
	// select correct loader function pointer
	LoaderFuncPtr loader = WavefrontOBJLoader::loadModel;
	switch (importOptions.m_fileType)
	{
	case FileType::WAVEFRONT_OBJ:
		loader = WavefrontOBJLoader::loadModel;
		break;
	case FileType::GLTF:
		loader = GLTFLoader::loadModel;
		break;
	default:
		assert(false);
		break;
	}

	// load model
	LoadedModel model;
	if (!loader(nativeSrcPath, importOptions.m_mergeByMaterial, importOptions.m_invertTexCoordY, importOptions.m_importMeshes, importOptions.m_importSkeletons, importOptions.m_importAnimations, model))
	{
		Log::err("Import of asset \"%s\" failed!", nativeSrcPath);
		return false;
	}

	if (importOptions.m_importSkeletons && !model.m_skeletons.empty())
	{
		bool res = SkeletonImporter::importSkeletons(model.m_skeletons.size(), model.m_skeletons.data(), dstPath, nativeSrcPath);
	}

	if (importOptions.m_importAnimations && !model.m_animationClips.empty())
	{
		bool res = AnimationClipImporter::importAnimationClips(model.m_animationClips.size(), model.m_animationClips.data(), dstPath, nativeSrcPath);
	}

	if (importOptions.m_importMeshes && !model.m_meshes.empty())
	{
		eastl::vector<AssetID> materialAssetIDs(model.m_materials.size());
		bool matRes = MaterialImporter::importMaterials(model.m_materials.size(), model.m_materials.data(), dstPath, nativeSrcPath, materialAssetIDs.data());
		bool res = MeshImporter::importMeshes(1, &model, dstPath, nativeSrcPath, physics, materialAssetIDs.data(), importOptions.m_cookConvexPhysicsMesh, importOptions.m_cookTrianglePhysicsMesh);
	}

	return true;
}
