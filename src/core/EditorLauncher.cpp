#include "EditorLauncher.h"

#include <SDL.h>

#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace EditorLauncher {
namespace {

std::filesystem::path editorPath() {
    std::filesystem::path dir;
    if (char* base = SDL_GetBasePath()) {
        dir = base;
        SDL_free(base);
    }
#ifdef _WIN32
    return dir / "mario_editor.exe";
#else
    return dir / "mario_editor";
#endif
}

#ifdef _WIN32
HANDLE gProcess = nullptr;
#endif

}  // namespace

bool available() {
    std::error_code ec;
    return std::filesystem::exists(editorPath(), ec);
}

bool launch(bool pickLevel) {
#ifdef _WIN32
    if (running()) return true;
    const std::filesystem::path exe = editorPath();
    std::wstring command = L"\"" + exe.wstring() + L"\"";
    if (pickLevel) command += L" --abrir";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    const std::wstring dir = exe.parent_path().wstring();
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &startup,
                        &info)) {
        return false;
    }
    CloseHandle(info.hThread);
    gProcess = info.hProcess;
    return true;
#else
    (void)pickLevel;
    return false;
#endif
}

bool running() {
#ifdef _WIN32
    if (!gProcess) return false;
    if (WaitForSingleObject(gProcess, 0) == WAIT_TIMEOUT) return true;
    CloseHandle(gProcess);
    gProcess = nullptr;
#endif
    return false;
}

}  // namespace EditorLauncher
