/*
Copyright 2017-2021 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string>

bool InPerfLogUsersGroup()
{
    // PERFLOG_USERS = S-1-5-32-559
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID sidPerfLogUsers = {};
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_LOGGING_USERS,
                                 0, 0, 0, 0, 0, 0, &sidPerfLogUsers) == 0) {
        return false;
    }

    BOOL isMember = FALSE;
    if (!CheckTokenMembership(nullptr, sidPerfLogUsers, &isMember)) {
        isMember = FALSE;
    }

    FreeSid(sidPerfLogUsers);
    return isMember != FALSE;
}

bool EnableDebugPrivilege()
{
    auto hmodule = LoadLibraryA("advapi32.dll");
    auto pOpenProcessToken      = (decltype(&OpenProcessToken))      GetProcAddress(hmodule, "OpenProcessToken");
    auto pGetTokenInformation   = (decltype(&GetTokenInformation))   GetProcAddress(hmodule, "GetTokenInformation");
    auto pLookupPrivilegeValue  = (decltype(&LookupPrivilegeValueA)) GetProcAddress(hmodule, "LookupPrivilegeValueA");
    auto pAdjustTokenPrivileges = (decltype(&AdjustTokenPrivileges)) GetProcAddress(hmodule, "AdjustTokenPrivileges");
    if (pOpenProcessToken      == nullptr ||
        pGetTokenInformation   == nullptr ||
        pLookupPrivilegeValue  == nullptr ||
        pAdjustTokenPrivileges == nullptr) {
        FreeLibrary(hmodule);
        return false;
    }

    HANDLE hToken = NULL;
    if (pOpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken) == 0) {
        FreeLibrary(hmodule);
        return false;
    }

    // Try to enable required privilege
    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (pLookupPrivilegeValue(NULL, "SeDebugPrivilege", &tp.Privileges[0].Luid) == 0) {
        CloseHandle(hToken);
        FreeLibrary(hmodule);
        return false;
    }

    auto adjustResult = pAdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
    auto adjustError = GetLastError();

    CloseHandle(hToken);
    FreeLibrary(hmodule);

    return
        adjustResult != 0 &&
        adjustError != ERROR_NOT_ALL_ASSIGNED;
}

int RestartAsAdministrator(
    int argc,
    char** argv)
{
    // Get the exe path
    char exe_path[MAX_PATH] = {};
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));

    // Combine arguments into single string and remove -restart_as_admin to
    // prevent an endless loop if the escalation fails.
    std::string args;
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "-restart_as_admin") == 0) continue;

        auto addQuotes = argv[i][0] != '\"' && strchr(argv[i], ' ') != nullptr;
        if (addQuotes) {
            args += '\"';
        }

        args += argv[i];

        if (addQuotes) {
            args += '\"';
        }

        args += ' ';
    }

    // Re-run the process with the runas verb
    DWORD code = 2;

    SHELLEXECUTEINFOA info = {};
    info.cbSize       = sizeof(info);
    info.fMask        = SEE_MASK_NOCLOSEPROCESS; // return info.hProcess for explicit wait
    info.lpVerb       = "runas";
    info.lpFile       = exe_path;
    info.lpParameters = args.c_str();
    info.nShow        = SW_SHOWDEFAULT;
    auto ok = ShellExecuteExA(&info);
    if (ok) {
        WaitForSingleObject(info.hProcess, INFINITE);
        GetExitCodeProcess(info.hProcess, &code);
        CloseHandle(info.hProcess);
    } else {
        fprintf(stderr, "error: failed to elevate privilege ");
        int e = GetLastError();
        switch (e) {
        case ERROR_FILE_NOT_FOUND:    fprintf(stderr, "(file not found).\n"); break;
        case ERROR_PATH_NOT_FOUND:    fprintf(stderr, "(path not found).\n"); break;
        case ERROR_DLL_NOT_FOUND:     fprintf(stderr, "(dll not found).\n"); break;
        case ERROR_ACCESS_DENIED:     fprintf(stderr, "(access denied).\n"); break;
        case ERROR_CANCELLED:         fprintf(stderr, "(cancelled).\n"); break;
        case ERROR_NOT_ENOUGH_MEMORY: fprintf(stderr, "(out of memory).\n"); break;
        case ERROR_SHARING_VIOLATION: fprintf(stderr, "(sharing violation).\n"); break;
        default:                      fprintf(stderr, "(%u).\n", e); break;
        }
    }

    return code;
}

