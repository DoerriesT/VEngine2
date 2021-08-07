#pragma once

class Physics;

namespace ModelImporter
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
	};
	bool importModel(const ImportOptions &importOptions, Physics *physics, const char *srcPath, const char *dstPath);
}
