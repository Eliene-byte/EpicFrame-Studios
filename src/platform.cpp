#include "platform.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace efs {

static std::string win32Error(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buffer, 0, nullptr);
    std::string result;
    if (length && buffer) {
        result = wideToUtf8(std::wstring(buffer, length));
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
            result.pop_back();
        }
    }
    if (buffer) LocalFree(buffer);
    if (result.empty()) result = "Erro do Windows " + std::to_string(code);
    return result;
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return std::wstring();
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0);
    if (length <= 0) {
        length = MultiByteToWideChar(CP_ACP, 0, text.data(), (int)text.size(), nullptr, 0);
        if (length <= 0) return std::wstring();
        std::wstring result((size_t)length, L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.data(), (int)text.size(), result.data(), length);
        return result;
    }
    std::wstring result((size_t)length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), result.data(), length);
    return result;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return std::string();
    std::string result((size_t)length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), length, nullptr, nullptr);
    return result;
}

std::string pathToUtf8(const std::wstring& path) {
    return wideToUtf8(path);
}

std::string appExecutablePath() {
    static std::string cached;
    if (!cached.empty()) return cached;
    std::wstring buffer(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());
    if (length == 0) return std::string();
    buffer.resize(length);
    cached = pathToUtf8(buffer);
    return cached;
}

std::string appDir() {
    static std::string cached;
    if (!cached.empty()) return cached;
    const std::string exe = appExecutablePath();
    if (exe.empty()) return std::string();
    fs::path path = fs::path(utf8ToWide(exe)).parent_path();
    cached = pathToUtf8(path.wstring());
    return cached;
}

std::string configDir() {
    static std::string cached;
    if (!cached.empty()) return cached;

    const std::string base = appDir();
    fs::path dir;
    if (!base.empty() && fileExists(joinPath(base, "portable.txt"))) {
        dir = fs::path(utf8ToWide(base)) / L"EpicFrameData";
    } else {
        wchar_t buffer[MAX_PATH * 2] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, buffer))) {
            dir = fs::path(buffer) / L"EpicFrame Studios";
        } else {
            dir = fs::path(utf8ToWide(tempDir())) / L"EpicFrame Studios";
        }
    }
    ensureDirectory(pathToUtf8(dir.wstring()));
    cached = pathToUtf8(dir.wstring());
    return cached;
}

std::string tempDir() {
    static std::string cached;
    if (!cached.empty()) return cached;
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetTempPathW((DWORD)buffer.size(), buffer.data());
    fs::path dir;
    if (length > 0 && length < buffer.size()) {
        dir = fs::path(std::wstring(buffer.data(), length)) / L"EpicFrame Studios";
    } else {
        dir = fs::path(L".") / L"EpicFrameTemp";
    }
    ensureDirectory(pathToUtf8(dir.wstring()));
    cached = pathToUtf8(dir.wstring());
    return cached;
}

std::string appVersion() {
#ifdef APP_VERSION
    return APP_VERSION;
#else
    return "0.1.0";
#endif
}

bool fileExists(const std::string& utf8Path) {
    if (utf8Path.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(fs::path(utf8ToWide(utf8Path)), ec);
}

bool directoryExists(const std::string& utf8Path) {
    if (utf8Path.empty()) return false;
    std::error_code ec;
    return fs::is_directory(fs::path(utf8ToWide(utf8Path)), ec);
}

bool ensureDirectory(const std::string& utf8Path) {
    if (utf8Path.empty()) return false;
    std::error_code ec;
    fs::create_directories(fs::path(utf8ToWide(utf8Path)), ec);
    return !ec;
}

bool readTextFile(const std::string& utf8Path, std::string& out, std::string& error) {
    out.clear();
    error.clear();
    HANDLE file = CreateFileW(fs::path(utf8ToWide(utf8Path)).c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = win32Error(GetLastError());
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > (LONGLONG)512 * 1024 * 1024) {
        CloseHandle(file);
        error = "Arquivo inválido ou grande demais.";
        return false;
    }
    out.resize((size_t)size.QuadPart);
    size_t offset = 0;
    while (offset < out.size()) {
        DWORD read = 0;
        const DWORD chunk = (DWORD)std::min<size_t>(out.size() - offset, 1u << 20);
        if (!ReadFile(file, out.data() + offset, chunk, &read, nullptr) || read == 0) {
            error = win32Error(GetLastError());
            CloseHandle(file);
            out.clear();
            return false;
        }
        offset += read;
    }
    CloseHandle(file);
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB &&
        (unsigned char)out[2] == 0xBF) {
        out.erase(0, 3);
    }
    return true;
}

static bool writeAll(HANDLE file, const std::string& text, std::string& error) {
    size_t offset = 0;
    while (offset < text.size()) {
        DWORD written = 0;
        const DWORD chunk = (DWORD)std::min<size_t>(text.size() - offset, 1u << 20);
        if (!WriteFile(file, text.data() + offset, chunk, &written, nullptr) || written == 0) {
            error = win32Error(GetLastError());
            return false;
        }
        offset += written;
    }
    return true;
}

bool writeTextFile(const std::string& utf8Path, const std::string& text, std::string& error) {
    error.clear();
    HANDLE file = CreateFileW(fs::path(utf8ToWide(utf8Path)).c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = win32Error(GetLastError());
        return false;
    }
    const bool ok = writeAll(file, text, error);
    CloseHandle(file);
    return ok;
}

bool writeTextFileAtomic(const std::string& utf8Path, const std::string& text, std::string& error) {
    error.clear();
    const std::string temp = utf8Path + ".tmp";
    if (!writeTextFile(temp, text, error)) return false;
    if (!MoveFileExW(fs::path(utf8ToWide(temp)).c_str(), fs::path(utf8ToWide(utf8Path)).c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = win32Error(GetLastError());
        DeleteFileW(fs::path(utf8ToWide(temp)).c_str());
        return false;
    }
    return true;
}

std::string fileName(const std::string& path) {
    if (path.empty()) return std::string();
    fs::path p(utf8ToWide(path));
    return pathToUtf8(p.filename().wstring());
}

std::string fileNameFromUtf8(const std::string& path) {
    return fileName(path);
}

std::string fileBaseName(const std::string& path) {
    if (path.empty()) return std::string();
    fs::path p(utf8ToWide(path));
    return pathToUtf8(p.stem().wstring());
}

std::string fileExtensionLower(const std::string& path) {
    if (path.empty()) return std::string();
    fs::path p(utf8ToWide(path));
    std::string ext = pathToUtf8(p.extension().wstring());
    if (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
    return lowerAscii(ext);
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    fs::path p(utf8ToWide(a));
    p /= fs::path(utf8ToWide(b));
    return pathToUtf8(p.wstring());
}

std::string normalizePathForCompare(const std::string& path) {
    if (path.empty()) return std::string();
    std::error_code ec;
    fs::path p = fs::absolute(fs::path(utf8ToWide(path)), ec);
    if (ec) p = fs::path(utf8ToWide(path));
    p = p.lexically_normal();
    std::string result = lowerAscii(pathToUtf8(p.wstring()));
    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
}

std::string ffmpegExecutable() {
    static std::string cached;
    if (!cached.empty()) return cached;
    const std::string dir = appDir();
    const std::string candidates[] = {joinPath(dir, "ffmpeg.exe"), joinPath(dir, "ffmpeg/ffmpeg.exe")};
    for (const auto& candidate : candidates) {
        if (fileExists(candidate)) {
            cached = candidate;
            return cached;
        }
    }
    wchar_t buffer[32768] = {};
    const DWORD length = SearchPathW(nullptr, L"ffmpeg.exe", nullptr, (DWORD)std::size(buffer), buffer, nullptr);
    if (length > 0 && length < std::size(buffer)) {
        cached = pathToUtf8(std::wstring(buffer, length));
    }
    return cached;
}

void revealInExplorer(const std::string& path) {
    if (path.empty()) return;
    const std::wstring wide = fs::path(utf8ToWide(path)).wstring();
    const std::wstring args = L"/select,\"" + wide + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

void openFolder(const std::string& path) {
    if (path.empty()) return;
    ShellExecuteW(nullptr, L"open", fs::path(utf8ToWide(path)).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static std::vector<std::string> parseMultiSelect(wchar_t* buffer) {
    std::vector<std::string> result;
    const std::wstring first(buffer);
    if (first.empty()) return result;
    const wchar_t* next = buffer + first.size() + 1;
    if (*next == L'\0') {
        result.push_back(pathToUtf8(first));
        return result;
    }
    std::wstring directory = first;
    if (!directory.empty() && directory.back() != L'\\' && directory.back() != L'/') {
        directory.push_back(L'\\');
    }
    while (*next != L'\0') {
        const std::wstring name(next);
        result.push_back(pathToUtf8(directory + name));
        next += name.size() + 1;
    }
    return result;
}

static std::wstring openDialogInternal(HWND owner, const wchar_t* filter, const wchar_t* defaultExt,
                                       bool multi, const wchar_t* title) {
    std::vector<wchar_t> buffer(65536, L'\0');
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = (DWORD)buffer.size();
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                OFN_HIDEREADONLY;
    if (multi) ofn.Flags |= OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameW(&ofn)) return std::wstring();
    return std::wstring(buffer.data());
}

static std::string saveDialogInternal(HWND owner, const wchar_t* filter, const wchar_t* defaultExt,
                                      const std::wstring& suggested, const wchar_t* title) {
    std::vector<wchar_t> buffer(32768, L'\0');
    if (!suggested.empty()) {
        wcsncpy(buffer.data(), suggested.c_str(), buffer.size() - 1);
    }
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = (DWORD)buffer.size();
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return std::string();
    return pathToUtf8(std::wstring(buffer.data()));
}

std::vector<std::string> openMediaDialog(void* owner) {
    const wchar_t filter[] =
        L"Vídeos, áudio e imagens\0*.mp4;*.m4v;*.mov;*.mkv;*.webm;*.avi;*.wmv;*.flv;*.ts;*.mts;*.m2ts;*.mpg;*.mpeg;*.3gp;*.ogv;*.mp3;*.wav;*.m4a;*.aac;*.flac;*.ogg;*.opus;*.wma;*.aiff;*.aif;*.png;*.jpg;*.jpeg;*.bmp;*.webp\0"
        L"Vídeos\0*.mp4;*.m4v;*.mov;*.mkv;*.webm;*.avi;*.wmv;*.flv;*.ts;*.mts;*.m2ts;*.mpg;*.mpeg;*.3gp;*.ogv\0"
        L"Áudio\0*.mp3;*.wav;*.m4a;*.aac;*.flac;*.ogg;*.opus;*.wma;*.aiff;*.aif\0"
        L"Imagens\0*.png;*.jpg;*.jpeg;*.bmp;*.webp\0"
        L"Todos os arquivos\0*.*\0\0";
    std::vector<wchar_t> buffer(65536, L'\0');
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = (DWORD)buffer.size();
    ofn.lpstrTitle = L"Importar mídia";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameW(&ofn)) return {};
    return parseMultiSelect(buffer.data());
}

std::string openProjectDialog(void* owner) {
    const wchar_t filter[] = L"Projeto EpicFrame (*.efsproj)\0*.efsproj\0Todos os arquivos\0*.*\0\0";
    const std::wstring raw = openDialogInternal((HWND)owner, filter, L"efsproj", false, L"Abrir projeto");
    return raw.empty() ? std::string() : pathToUtf8(raw);
}

std::string saveProjectDialog(void* owner, const std::string& suggestedName) {
    const wchar_t filter[] = L"Projeto EpicFrame (*.efsproj)\0*.efsproj\0\0";
    std::wstring suggested = utf8ToWide(suggestedName);
    if (suggested.empty()) suggested = L"Sem título.efsproj";
    if (fileExtensionLower(suggestedName) != "efsproj") suggested += L".efsproj";
    return saveDialogInternal((HWND)owner, filter, L"efsproj", suggested, L"Salvar projeto");
}

std::string saveExportDialog(void* owner, const std::string& suggestedName) {
    const wchar_t filter[] = L"Vídeo MP4 (*.mp4)\0*.mp4\0Todos os arquivos\0*.*\0\0";
    std::wstring suggested = utf8ToWide(suggestedName);
    if (suggested.empty()) suggested = L"EpicFrame.mp4";
    if (fileExtensionLower(suggestedName) != "mp4") suggested += L".mp4";
    return saveDialogInternal((HWND)owner, filter, L"mp4", suggested, L"Exportar vídeo");
}

std::string lowerAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return text;
}

std::string trimText(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace((unsigned char)text[begin])) ++begin;
    while (end > begin && std::isspace((unsigned char)text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

} // namespace efs
