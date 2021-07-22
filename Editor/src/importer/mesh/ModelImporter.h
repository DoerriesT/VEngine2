#pragma once

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
	bool importModel(const ImportOptions &importOptions, const char *srcPath, const char *dstPath);
}
