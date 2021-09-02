#include "imgui_file_browser.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "filesystem/Path.h"
#include <cctype>

ImGuiFileBrowser::ImGuiFileBrowser(
	IFileSystem *fs, 
	const char *currentPath, 
	ThumbnailCallback thumbnailCallback, 
	FileFilterCallback filterCallback, 
	IsDragDropSourceCallback isDragDropSourceCallback, 
	DragDropSourcePayloadCallback dragDropSourcePayloadCallback, 
	void *callbackUserData) noexcept
	:m_fs(fs),
	m_thumbnailCallback(thumbnailCallback),
	m_fileFilterCallback(filterCallback),
	m_isDragDropCallback(isDragDropSourceCallback),
	m_dragDropPayloadCallback(dragDropSourcePayloadCallback),
	m_callbackUserData(callbackUserData),
	m_currentPath(currentPath)
{
	updatePathSegments();
	m_history.push_back(m_currentPath);
}

void ImGuiFileBrowser::draw() noexcept
{
	auto newPath = m_currentPath;
	bool newPathIsFromHistory = false;

	drawPathMenu(&newPath, &newPathIsFromHistory);
	drawMainArea(&newPath);

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

eastl::string ImGuiFileBrowser::getCurrentPath() const noexcept
{
	return m_currentPath;
}

void ImGuiFileBrowser::drawPathMenu(eastl::string *newPath, bool *newPathIsFromHistory) noexcept
{
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
				*newPath = m_history[m_historyPointer];
				*newPathIsFromHistory = true;
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
				*newPath = m_history[m_historyPointer];
				*newPathIsFromHistory = true;
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
			if (ImGui::Button(Path::getFileName(p.c_str())))
			{
				*newPath = p;
			}
			ImGui::SameLine();
			ImGui::PopID();

			bool hasSubDirs = false;

			m_fs->iterate(p.c_str(), [&](const FileFindData &ffd)
				{
					if (ffd.m_isDirectory)
					{
						hasSubDirs = true;
						return false;
					}
					return true;
				});

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
			m_fs->iterate(m_subDirPopupPath.c_str(), [&](const FileFindData &ffd)
				{
					if (ffd.m_isDirectory)
					{
						if (ImGui::Selectable(ffd.m_path))
						{
							*newPath = ffd.m_path;
						}
					}
					return true;
				});

			ImGui::EndPopup();
		}

		ImGui::NewLine();
	}
}

void ImGuiFileBrowser::drawMainArea(eastl::string *newPath) noexcept
{
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
				drawTreeNode(m_currentPathSegments[0].c_str(), newPath);
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

				eastl::string_view searchTextView = eastl::string_view(m_searchTextLower);

				float itemSize = 128.0f;
				auto contentRegionAvail = ImGui::GetContentRegionAvail();
				int columns = (int)(contentRegionAvail.x / 128.0f);
				columns = columns < 1 ? 1 : columns;
				ImGui::Columns(columns, nullptr, false);

				for (int isFileOnlyRun = 0; isFileOnlyRun < 2; ++isFileOnlyRun)
				{
					m_fs->iterate(m_currentPath.c_str(), [&](const FileFindData &ffd)
						{
							if (isFileOnlyRun == 0 && !ffd.m_isDirectory)
							{
								return true;
							}
							if (isFileOnlyRun == 1 && !ffd.m_isFile)
							{
								return true;
							}
							if (ffd.m_isFile && m_fileFilterCallback && !m_fileFilterCallback(ffd, m_callbackUserData))
							{
								return true;
							}

							eastl::string displayName = Path::getFileName(ffd.m_path);
							eastl::string displayNameLower = displayName;
							eastl::transform(displayNameLower.begin(), displayNameLower.end(), displayNameLower.begin(), [](unsigned char c) { return std::tolower(c); });

							if (!searchTextView.empty() && displayNameLower.find(searchTextView.data(), 0) == eastl::string::npos)
							{
								return true;
							}

							ImGui::PushID(displayName.c_str());

							ImTextureID thumbnail = 0;
							if (m_thumbnailCallback)
							{
								thumbnail = m_thumbnailCallback(ffd, m_callbackUserData);
							}

							auto drawElement = [](float itemSize, ImTextureID thumbnail, const char *displayName)
							{
								ImGui::BeginGroup();
								{
									if (thumbnail)
									{
										ImGui::Image(thumbnail, ImVec2(itemSize - 20.0f, itemSize - 16));
									}
									else
									{
										ImGui::Button("???", ImVec2(itemSize - 20.0f, itemSize - 16));
									}

									ImGui::TextWrapped(displayName);
								}
								ImGui::EndGroup();
							};

							drawElement(itemSize, thumbnail, displayName.c_str());
							if (m_isDragDropCallback(ffd, m_callbackUserData))
							{
								if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
								{
									m_dragDropPayloadCallback(ffd, m_callbackUserData);

									drawElement(itemSize, thumbnail, displayName.c_str());

									ImGui::EndDragDropSource();
								}
							}
							
							
							bool isHovered = ImGui::IsItemHovered();
							bool isClicked = ImGui::IsItemClicked();
							bool isDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);


							if (isHovered)
							{
								ImGui::SetTooltip(displayName.c_str());
							}
							if (isClicked)
							{
								if (isDoubleClicked)
								{
									if (ffd.m_isDirectory)
									{
										*newPath = ffd.m_path;
									}
								}
							}

							ImGui::PopID();
							ImGui::NextColumn();

							return true;
						});
				}
			}
			ImGui::EndChild();
		}

		ImGui::EndTable();
	}
}

void ImGuiFileBrowser::drawTreeNode(const char *path, eastl::string *newPath) noexcept
{
	if (!m_fs->isDirectory(path))
	{
		return;
	}

	bool isSelected = m_currentPath == path;
	bool isParentOfCurrentPath = m_currentPath.find(path) != eastl::string::npos; // TODO: bad hack, find something better
	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoAutoOpenOnLog;
	treeNodeFlags |= isSelected ? ImGuiTreeNodeFlags_Selected : 0;
	treeNodeFlags |= isParentOfCurrentPath ? ImGuiTreeNodeFlags_DefaultOpen : 0;

	bool nodeOpen = ImGui::TreeNodeEx(Path::getFileName(path), treeNodeFlags);
	if (ImGui::IsItemClicked())
	{
		*newPath = path;
	}

	if (nodeOpen)
	{
		m_fs->iterate(path, [&](const FileFindData &ffd)
			{
				if (ffd.m_isDirectory)
				{
					drawTreeNode(ffd.m_path, newPath);
				}
				return true;
			});

		ImGui::TreePop();
	}
}

void ImGuiFileBrowser::updatePathSegments() noexcept
{
	m_currentPathSegments.clear();
	const size_t pathSegmentCount = Path::getSegmentCount(m_currentPath.c_str());
	m_currentPathSegments.reserve(pathSegmentCount);

	for (size_t i = 0; i < pathSegmentCount; ++i)
	{
		size_t offset = Path::getFirstNSegments(m_currentPath.c_str(), i + 1);
		m_currentPathSegments.push_back(m_currentPath.substr(0, offset));
	}
}