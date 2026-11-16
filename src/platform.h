#pragma once

#include <string>
#include <vector>

namespace efs {

std::wstring utf8ToWide(const std::string& text);
std::string wideToUtf8(const std::wstring& text);
std::string pathToUtf8(const std::wstring& path);

std::string appDir();
std::string configDir();
std::string tempDir();
std::string appVersion();
std::string appExecutablePath();

bool fileExists(const std::string& utf8Path);
bool directoryExists(const std::string& utf8Path);
bool ensureDirectory(const std::string& utf8Path);
bool readTextFile(const std::string& utf8Path, std::string& out, std::string& error);
bool writeTextFile(const std::string& utf8Path, const std::string& text, std::string& error);
bool writeTextFileAtomic(const std::string& utf8Path, const std::string& text, std::string& error);

std::string fileName(const std::string& path);
std::string fileBaseName(const std::string& path);
std::string fileExtensionLower(const std::string& path);
std::string joinPath(const std::string& a, const std::string& b);
std::string normalizePathForCompare(const std::string& path);
std::string fileNameFromUtf8(const std::string& path);

std::string ffmpegExecutable();
void revealInExplorer(const std::string& path);
void openFolder(const std::string& path);

std::vector<std::string> openMediaDialog(void* owner);
std::string openProjectDialog(void* owner);
std::string saveProjectDialog(void* owner, const std::string& suggestedName);
std::string saveExportDialog(void* owner, const std::string& suggestedName);

std::string lowerAscii(std::string text);
std::string trimText(const std::string& text);

} // namespace efs
