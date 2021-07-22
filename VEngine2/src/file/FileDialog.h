#pragma once
#include <EASTL/vector.h>
#include <filesystem>

namespace FileDialog
{
	struct FileExtension
	{
		const char *m_name;
		const char *m_extension;
	};
	struct FileDialogParams
	{
		size_t m_fileExtensionCount;
		const FileExtension *m_fileExtensions;
		const char *m_dialogTitle;
		const char *m_defaultFile;
		const char *m_defaultFolder;
		bool m_multiSelection;
		bool m_fileSelection;
		bool m_save;
	};

	bool initializeCOM();
	bool showFileDialog(const FileDialogParams &dialogParams, eastl::vector<std::filesystem::path> &selectedFiles, uint32_t &selectedFilterIndex);
}