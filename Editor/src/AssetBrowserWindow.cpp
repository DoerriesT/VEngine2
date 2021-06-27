#include "AssetBrowserWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/imgui_internal.h>
#include <filesystem>
#include <ShObjIdl_core.h>
#include <windows.h>
#include <thumbcache.h>
#include <unordered_map>
#include <graphics/Renderer.h>
#include <Engine.h>
#include <UUID.h>
#include <utility/Utility.h>

static TextureHandle getThumbNail(const std::filesystem::path &path, Engine *engine)
{
	static std::unordered_map<std::string, TextureHandle> iconMap;

	std::string pathU8 = path.u8string();

	auto it = iconMap.find(pathU8);
	if (it != iconMap.end())
	{
		return it->second;
	}

#if 0
	static std::unordered_map<int, TextureHandle> iconIndexToTexMap;

	std::error_code ec;
	iconMap[pathU8] = {};

	DWORD attrs = 0;
	UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
	if (!std::filesystem::exists(path, ec)) {
		flags |= SHGFI_USEFILEATTRIBUTES;
		attrs = FILE_ATTRIBUTE_DIRECTORY;
	}

	SHFILEINFOW fileInfo = { 0 };
	std::wstring pathW = path.wstring();
	for (int i = 0; i < pathW.size(); i++)
		if (pathW[i] == '/')
			pathW[i] = '\\';
	SHGetFileInfoW(pathW.c_str(), attrs, &fileInfo, sizeof(SHFILEINFOW), flags);

	if (fileInfo.hIcon == nullptr)
		return {};

	// check if icon is already loaded
	auto idxIt = iconIndexToTexMap.find(fileInfo.iIcon);
	if (idxIt != iconIndexToTexMap.end())
	{
		iconMap[pathU8] = idxIt->second;
		return idxIt->second;
	}

	ICONINFO iconInfo = { 0 };
	GetIconInfo(fileInfo.hIcon, &iconInfo);

	if (iconInfo.hbmColor == nullptr)
		return {};

	DIBSECTION ds;
	GetObject(iconInfo.hbmColor, sizeof(ds), &ds);
	int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

	if (byteSize == 0)
		return {};

	uint8_t *data = (uint8_t *)malloc(byteSize);
	GetBitmapBits(iconInfo.hbmColor, byteSize, data);

	auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, path.generic_string().c_str(), ds.dsBm.bmWidth, ds.dsBm.bmHeight);
	iconMap[pathU8] = textureHandle;
	iconIndexToTexMap[fileInfo.iIcon] = textureHandle;

	free(data);

	return textureHandle;

	#elseif 0
		static std::unordered_map<TUUID, TextureHandle, UUIDHash> iconIDToTexMap;

	TextureHandle resultTex = {};

	// create IShellItem from path
	IShellItem *shellItem = nullptr;
	auto hr = SHCreateItemFromParsingName(path.native().c_str(), nullptr, __uuidof(IShellItem), (void **)&shellItem);
	if (SUCCEEDED(hr))
	{
		// get thumbnail cache
		IThumbnailCache *pThumbCache;
		hr = CoCreateInstance(CLSID_LocalThumbnailCache, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IThumbnailCache), (void **)&pThumbCache);
		if (SUCCEEDED(hr))
		{
			// get thumbnail
			WTS_THUMBNAILID thumbID{};
			ISharedBitmap *pBitmap;
			hr = pThumbCache->GetThumbnail(shellItem, 128, WTS_EXTRACT, &pBitmap, nullptr, &thumbID);
			if (SUCCEEDED(hr))
			{
				// check if we already loaded this thumbnail
				TUUID thumbUUID = TUUID(thumbID.rgbKey);
				auto idIt = iconIDToTexMap.find(thumbUUID);
				if (idIt != iconIDToTexMap.end())
				{
					iconMap[pathU8] = idIt->second;
					resultTex = idIt->second;
				}
				// we haven't loaded this thumbnail yet
				else
				{
					// get bitmap from thumbnail
					HBITMAP hBitmap;
					hr = pBitmap->GetSharedBitmap(&hBitmap);
					if (SUCCEEDED(hr))
					{
						// query info about bitmap
						DIBSECTION ds;
						GetObject(hBitmap, sizeof(ds), &ds);
						int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

						if (byteSize != 0)
						{
							// allocate memory and copy bitmap data to it
							uint8_t *data = (uint8_t *)malloc(byteSize);
							GetBitmapBits(hBitmap, byteSize, data);

							// convert from BGRA to RGBA
							for (size_t i = 0; i < byteSize; i += 4)
							{
								auto tmp = data[i];
								data[i] = data[i + 2];
								data[i + 2] = tmp;
							}

							// create texture and store in maps
							auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, pathU8.c_str(), ds.dsBm.bmWidth, ds.dsBm.bmHeight);
							iconMap[pathU8] = textureHandle;
							iconIDToTexMap[thumbUUID] = textureHandle;

							// dont forget to free our allocation!
							free(data);

							resultTex = textureHandle;
						}
					}
				}
			}
			else
			{
				printf("");
			}

			if (pBitmap)
			{
				pBitmap->Release();
			}
		}

		if (pThumbCache)
		{
			pThumbCache->Release();
		}
	}

	return resultTex;
#else
	TextureHandle resultTex = {};

	static std::unordered_map<size_t, TextureHandle> hasedIconMap;

	// create IShellItemImageFactory  from path
	IShellItemImageFactory *imageFactory = nullptr;
	auto hr = SHCreateItemFromParsingName(path.native().c_str(), nullptr, __uuidof(IShellItemImageFactory), (void **)&imageFactory);

	if (SUCCEEDED(hr))
	{
		SIZE size = { 128, 128 };

		//sz - Size of the image, SIIGBF_BIGGERSIZEOK - GetImage will stretch down the bitmap (preserving aspect ratio)
		HBITMAP hbmp;
		hr = imageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hbmp);
		if (SUCCEEDED(hr))
		{
			// query info about bitmap
			DIBSECTION ds;
			GetObject(hbmp, sizeof(ds), &ds);
			int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

			if (byteSize != 0)
			{
				// allocate memory and copy bitmap data to it
				uint8_t *data = (uint8_t *)malloc(byteSize);
				GetBitmapBits(hbmp, byteSize, data);

				size_t hash = 0;

				// convert from BGRA to RGBA
				for (size_t i = 0; i < byteSize; i += 4)
				{
					auto tmp = data[i];
					data[i] = data[i + 2];
					data[i + 2] = tmp;

					util::hashCombine(hash, *(uint32_t *)(data + i));
				}

				// found an identical image in our cache
				auto hashIt = hasedIconMap.find(hash);
				if (hashIt != hasedIconMap.end())
				{
					iconMap[pathU8] = hashIt->second;
					resultTex = hashIt->second;
				}
				else
				{
					// create texture and store in maps
					auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, pathU8.c_str(), ds.dsBm.bmWidth, ds.dsBm.bmHeight);
					iconMap[pathU8] = textureHandle;
					hasedIconMap[hash] = textureHandle;

					resultTex = textureHandle;
				}

				// dont forget to free our allocation!
				free(data);
			}


			DeleteObject(hbmp);
		}

		imageFactory->Release();
	}

	return resultTex;
#endif
}

AssetBrowserWindow::AssetBrowserWindow(Engine *engine) noexcept
	:m_engine(engine)
{
	updatePathSegments();
	m_history.push_back(m_currentPath);
}

void AssetBrowserWindow::draw() noexcept
{
	if (!m_visible)
	{
		return;
	}

	ImGui::Begin("Asset Browser");
	{
		auto newPath = m_currentPath;
		bool newPathIsFromHistory = false;

		// history forward/backward buttons
		{
			bool disabledBackButton = !(m_historyPointer > 0);
			bool disabledForwardButton = !((m_historyPointer + 1) < m_history.size());

			if (disabledBackButton)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::ArrowButton("<##path_history_backward", ImGuiDir_Left))
			{
				if (!disabledBackButton)
				{
					--m_historyPointer;
					newPath = m_history[m_historyPointer];
					newPathIsFromHistory = true;
				}
			}
			if (disabledBackButton)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}

			ImGui::SameLine();

			if (disabledForwardButton)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::ArrowButton(">##path_history_forward", ImGuiDir_Right))
			{
				if (!disabledForwardButton)
				{
					++m_historyPointer;
					newPath = m_history[m_historyPointer];
					newPathIsFromHistory = true;
				}
			}
			if (disabledForwardButton)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
		}

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		// current path menu
		{
			bool showSubDirPopup = false;

			for (const auto &p : m_currentPathSegments)
			{
				ImGui::PushID(&p);
				if (ImGui::Button(p.filename().u8string().c_str()))
				{
					newPath = p;
				}
				ImGui::SameLine();
				ImGui::PopID();

				bool hasSubDirs = false;

				for (auto entry : std::filesystem::directory_iterator(p))
				{
					if (entry.is_directory())
					{
						hasSubDirs = true;
						break;
					}
				}

				if (hasSubDirs)
				{
					ImGui::PushID(p.c_str());
					bool buttonClicked = ImGui::ArrowButton("##>", ImGuiDir_Right);
					ImGui::PopID();
					if (buttonClicked)
					{
						showSubDirPopup = true;
						m_subDirPopupPath = p;
					}
					ImGui::SameLine();
				}
			}

			if (showSubDirPopup)
			{
				ImGui::OpenPopup("jump_to_sub_dir_popup");
			}

			if (ImGui::BeginPopup("jump_to_sub_dir_popup"))
			{
				for (auto entry : std::filesystem::directory_iterator(m_subDirPopupPath))
				{
					if (entry.is_directory())
					{
						if (ImGui::Selectable(entry.path().filename().u8string().c_str()))
						{
							newPath = entry.path();
						}
					}
				}

				ImGui::EndPopup();
			}

			ImGui::NewLine();
		}

		if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("##dir_tree", ImGuiTableColumnFlags_WidthFixed, 125.0f);
			ImGui::TableSetupColumn("##content", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			// directory tree
			{
				ImGui::TableSetColumnIndex(0);
				ImGui::BeginChild("##dir_tree_container");
				{
					renderTreeNode(m_currentPathSegments[0], &newPath);
				}
				ImGui::EndChild();
			}

			// content
			{
				ImGui::TableSetColumnIndex(1);
				ImGui::BeginChild("##contentContainer");
				{
					// search field
					{
						ImGui::Text("Search");
						ImGui::SameLine();
						ImGui::InputText("##Search", m_searchText, sizeof(m_searchText));
						for (size_t i = 0; ; ++i)
						{
							if (m_searchText[i] == '\0')
							{
								m_searchTextLower[i] = '\0';
								break;
							}
							m_searchTextLower[i] = std::tolower(m_searchText[i]);
						}
					}

					ImGui::Separator();
					ImGui::NewLine();
					
					std::string_view searchTextView = std::string_view(m_searchTextLower);
					
					float itemSize = 128.0f;
					auto contentRegionAvail = ImGui::GetContentRegionAvail();
					int columns = (int)(contentRegionAvail.x / 128.0f);
					ImGui::Columns(columns, nullptr, false);
					
					for (int isFileOnlyRun = 0; isFileOnlyRun < 2; ++isFileOnlyRun)
					{
						for (auto entry : std::filesystem::directory_iterator(m_currentPath))
						{
							if (isFileOnlyRun == 0 && !entry.is_directory())
							{
								continue;
							}
							if (isFileOnlyRun == 1 && !entry.is_regular_file())
							{
								continue;
							}

							std::string displayName = entry.path().filename().u8string();
							std::string displayNameLower = displayName;
							std::transform(displayNameLower.begin(), displayNameLower.end(), displayNameLower.begin(), [](unsigned char c) { return std::tolower(c); });

							if (!searchTextView.empty() && displayNameLower.find(searchTextView) == std::string::npos)
							{
								continue;
							}

							ImGui::PushID(displayName.c_str());

							ImGui::BeginGroup();
							{
								auto thumb = getThumbNail(entry.path(), m_engine);
								if (!thumb)
								{
									ImGui::Button("???", ImVec2(itemSize - 20.0f, itemSize - 16));
								}
								else
								{
									ImGui::Image(m_engine->getRenderer()->getImGuiTextureID(thumb), ImVec2(itemSize - 20.0f, itemSize - 16));
								}

								ImGui::TextWrapped(displayName.c_str());
							}
							ImGui::EndGroup();
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip(displayName.c_str());
							}
							if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							{
								if (entry.is_directory())
								{
									newPath = entry.path();
								}
							}

							ImGui::PopID();
							ImGui::NextColumn();
						}
					}
				}
				ImGui::EndChild();
			}

			ImGui::EndTable();
		}

		// update path
		if (m_currentPath != newPath)
		{
			if (!newPathIsFromHistory)
			{
				if ((m_historyPointer + 1) != m_history.size())
				{
					m_history.resize(m_historyPointer + 1);
				}
		
				m_history.push_back(newPath);
				++m_historyPointer;
			}
			m_currentPath = newPath;
			updatePathSegments();
		}

	}
	ImGui::End();
}

void AssetBrowserWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool AssetBrowserWindow::isVisible() const noexcept
{
	return m_visible;
}

void AssetBrowserWindow::updatePathSegments() noexcept
{
	m_currentPathSegments.clear();
	size_t pathSegmentCount = 0;
	auto curPath = m_currentPath;

	// compute number of segments
	do
	{
		++pathSegmentCount;
		if (curPath.parent_path() == curPath)
		{
			break;
		}
		curPath = curPath.parent_path();
	} while (curPath.has_parent_path());

	m_currentPathSegments.resize(pathSegmentCount);

	curPath = m_currentPath;

	// write in reverse bottom-up order (so its top-down)
	do
	{
		--pathSegmentCount;
		m_currentPathSegments[pathSegmentCount] = curPath;
		if (curPath.parent_path() == curPath)
		{
			break;
		}
		curPath = curPath.parent_path();
	} while (curPath.has_parent_path());
}

void AssetBrowserWindow::renderTreeNode(const std::filesystem::path &path, std::filesystem::path *newPath) noexcept
{
	if (!std::filesystem::is_directory(path))
	{
		return;
	}

	bool isSelected = m_currentPath == path;
	bool isParentOfCurrentPath = m_currentPath.u8string().find(path.u8string()) != std::string::npos; // TODO: bad hack, find something better
	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoAutoOpenOnLog;
	treeNodeFlags |= isSelected ? ImGuiTreeNodeFlags_Selected : 0;
	treeNodeFlags |= isParentOfCurrentPath ? ImGuiTreeNodeFlags_DefaultOpen : 0;

	bool nodeOpen = ImGui::TreeNodeEx(path.filename().u8string().c_str(), treeNodeFlags);
	if (ImGui::IsItemClicked())
	{
		*newPath = path;
	}

	if (nodeOpen)
	{
		try
		{
			for (auto entry : std::filesystem::directory_iterator(path))
			{
				if (entry.is_directory())
				{
					renderTreeNode(entry.path(), newPath);
				}
			}
		}
		catch (...)
		{

		}
		ImGui::TreePop();
	}
}
