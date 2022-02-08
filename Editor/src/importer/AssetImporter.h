#pragma once

class Physics;

namespace AssetImporter
{
	enum class FileType
	{
		WAVEFRONT_OBJ,
		GLTF,
	};

	struct ImportOptions
	{
		FileType m_fileType;
		bool m_mergeByMaterial;
		bool m_invertTexCoordY;
		bool m_importMeshes;
		bool m_importSkeletons;
		bool m_importAnimations;
		bool m_cookConvexPhysicsMesh;
		bool m_cookTrianglePhysicsMesh;
		float m_meshScale = 1.0f;
	};

	bool importAsset(const ImportOptions &importOptions, Physics *physics, const char *nativeSrcPath, const char *dstPath) noexcept;
}