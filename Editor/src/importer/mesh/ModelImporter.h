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
	};
	bool importModel(const ImportOptions &importOptions, Physics *physics, const char *srcPath, const char *dstPath);
}
