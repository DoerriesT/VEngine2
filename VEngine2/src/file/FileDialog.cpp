#include "FileDialog.h"
#include "Log.h"
#include <ShObjIdl.h>
#include <atlbase.h>

static std::wstring cstringToWString(const char *str)
{
	constexpr size_t wStrLen = 1024;
	wchar_t wName[wStrLen];

	size_t ret;
	mbstowcs_s(&ret, wName, str, wStrLen - 1);

	return wName;
}

static std::string wstringToString(const wchar_t *str)
{
	constexpr size_t strLen = 1024;
	char name[strLen];

	size_t ret;
	wcstombs_s(&ret, name, str, strLen - 1);

	return name;
}

bool FileDialog::initializeCOM()
{
	HRESULT hr = S_OK;
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		hr = ::CoInitialize(NULL);

		if (SUCCEEDED(hr))
		{
			Log::info("Initialized COM");
		}
		else
		{
			Log::err("Failed to initialize COM!");
		}
	}

	return  SUCCEEDED(hr);
}

bool FileDialog::showFileDialog(const FileDialogParams &dialogParams, eastl::vector<std::filesystem::path> &selectedFiles, uint32_t &selectedFilterIndex)
{
	namespace fs = std::filesystem;

	bool success = false;

	CComPtr<IFileDialog> fileDialog;
	if (SUCCEEDED(::CoCreateInstance(dialogParams.m_save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, dialogParams.m_save ? IID_IFileSaveDialog : IID_IFileOpenDialog, reinterpret_cast<void **>(&fileDialog))))
	{
		if (dialogParams.m_fileSelection)
		{
			if (dialogParams.m_save)
			{
				if (dialogParams.m_defaultFile)
				{
					fs::path defaultFile(dialogParams.m_defaultFile);
					fileDialog->SetFileName(defaultFile.filename().native().c_str());
				}
			}
			else
			{
				if (dialogParams.m_multiSelection)
				{
					FILEOPENDIALOGOPTIONS options = 0;
					fileDialog->GetOptions(&options);
					fileDialog->SetOptions(options | FOS_ALLOWMULTISELECT);
				}
			}
		}
		// dialog for selecting a folder
		else
		{
			FILEOPENDIALOGOPTIONS options = 0;
			fileDialog->GetOptions(&options);
			fileDialog->SetOptions(options | FOS_PICKFOLDERS);
		}


		// set dialog title
		if (dialogParams.m_dialogTitle)
		{
			fileDialog->SetTitle(cstringToWString(dialogParams.m_dialogTitle).c_str());
		}
		if (dialogParams.m_defaultFolder)
		{
			fs::path defaultFolder(dialogParams.m_defaultFolder);
			CComPtr<IShellItem> defaultPathItem;
			if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.remove_filename().native().c_str(), nullptr, IID_PPV_ARGS(&defaultPathItem))))
			{
				fileDialog->SetFolder(defaultPathItem);
			}
		}

		// set file extension filters
		if (dialogParams.m_fileSelection)
		{
			std::vector<std::wstring> extensionStrings;
			std::vector<COMDLG_FILTERSPEC> filters;
			{
				extensionStrings.reserve(dialogParams.m_fileExtensionCount * 2);
				filters.reserve(dialogParams.m_fileExtensionCount);
				for (size_t i = 0; i < dialogParams.m_fileExtensionCount; ++i)
				{
					extensionStrings.push_back(cstringToWString(dialogParams.m_fileExtensions[i].m_name).c_str());
					extensionStrings.push_back(cstringToWString(dialogParams.m_fileExtensions[i].m_extension).c_str());
				}
				for (size_t i = 0; i < dialogParams.m_fileExtensionCount; ++i)
				{
					COMDLG_FILTERSPEC filterSpec{};
					filterSpec.pszName = extensionStrings[i * 2 + 0].c_str();
					filterSpec.pszSpec = extensionStrings[i * 2 + 1].c_str();
					filters.push_back(filterSpec);
				}
			}
			fileDialog->SetFileTypes((UINT)filters.size(), filters.data());
		}

		// show dialog
		if (SUCCEEDED(fileDialog->Show(nullptr)))
		{
			if (dialogParams.m_fileSelection)
			{
				UINT outFilterIndex = 0;
				if (SUCCEEDED(fileDialog->GetFileTypeIndex(&outFilterIndex)))
				{
					--outFilterIndex; // GetFileTypeIndex returns indices starting at 1
				}
				selectedFilterIndex = outFilterIndex;

				if (dialogParams.m_save)
				{
					CComPtr<IShellItem> result;
					if (SUCCEEDED(fileDialog->GetResult(&result)))
					{
						PWSTR filePath = nullptr;
						if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &filePath)))
						{
							success = true;
							selectedFiles.push_back(filePath);
							::CoTaskMemFree(filePath);
						}
					}
				}
				else
				{
					IFileOpenDialog *fileOpenDialog = static_cast<IFileOpenDialog *>(fileDialog.p);

					CComPtr<IShellItemArray> results;
					auto hres = fileOpenDialog->GetResults(&results);
					if (SUCCEEDED(hres))
					{
						DWORD resultCount = 0;
						results->GetCount(&resultCount);
						for (DWORD i = 0; i < resultCount; ++i)
						{
							CComPtr<IShellItem> result;
							if (SUCCEEDED(results->GetItemAt(i, &result)))
							{
								PWSTR filePath = nullptr;
								if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &filePath)))
								{
									success = true;
									selectedFiles.push_back(filePath);
									::CoTaskMemFree(filePath);
								}
							}
						}
					}
				}
			}
			// folder selection
			else
			{
				CComPtr<IShellItem> result;
				if (SUCCEEDED(fileDialog->GetResult(&result)))
				{
					PWSTR filePath = nullptr;
					if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &filePath)))
					{
						success = true;
						selectedFiles.push_back(filePath);
						::CoTaskMemFree(filePath);
					}
				}
			}
		}
	}
	return success;
}
