/*
Copyright 2020 Intel Corporation

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
#define NOMINMAX
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <windows.h>

struct PresentMonCsv
{
    static constexpr char const* const REQUIRED_HEADER[] = {
        "Application",
        "ProcessID",
        "SwapChainAddress",
        "Runtime",
        "SyncInterval",
        "PresentFlags",
        "Dropped",
        "TimeInSeconds",
        "msBetweenPresents",
        "msInPresentAPI",
    };

    static constexpr char const* const TRACK_DISPLAY_HEADER[] = {
        "AllowsTearing",
        "PresentMode",
        "msBetweenDisplayChange",
        "msUntilRenderComplete",
        "msUntilDisplayed",
    };

    static constexpr char const* const TRACK_DEBUG_HEADER[] = {
        "WasBatched",
        "DwmNotified",
    };

    static constexpr char const* const OPT_HEADER[] = {
        "QPCTime",
    };

    static constexpr char const* GetHeader(size_t idx)
    {
        if (idx < _countof(PresentMonCsv::REQUIRED_HEADER)) return PresentMonCsv::REQUIRED_HEADER[idx];
        idx -= _countof(PresentMonCsv::REQUIRED_HEADER);

        if (idx < _countof(PresentMonCsv::TRACK_DISPLAY_HEADER)) return PresentMonCsv::TRACK_DISPLAY_HEADER[idx];
        idx -= _countof(PresentMonCsv::TRACK_DISPLAY_HEADER);

        if (idx < _countof(PresentMonCsv::TRACK_DEBUG_HEADER)) return PresentMonCsv::TRACK_DEBUG_HEADER[idx];
        idx -= _countof(PresentMonCsv::TRACK_DEBUG_HEADER);

        if (idx < _countof(PresentMonCsv::OPT_HEADER)) return PresentMonCsv::OPT_HEADER[idx];
        idx -= _countof(PresentMonCsv::OPT_HEADER);

        return "Unknown";
    }

    std::wstring path_;
    size_t line_;
    FILE* fp_;
    size_t headerColumnIndex_[_countof(REQUIRED_HEADER) +
                              _countof(TRACK_DISPLAY_HEADER) +
                              _countof(TRACK_DEBUG_HEADER) +
                              _countof(OPT_HEADER)];
    char row_[1024];
    std::vector<char const*> cols_;
    bool trackDisplay_;
    bool trackDebug_;

    PresentMonCsv();
    bool Open(char const* file, int line, std::wstring const& path);
    void Close();
    bool ReadRow();

    size_t GetColumnIndex(char const* header) const;
};

#define CSVOPEN(_P) Open(__FILE__, __LINE__, _P)

struct PresentMon : PROCESS_INFORMATION {
    static std::wstring exePath_;
    std::wstring cmdline_;
    bool csvArgSet_;

    PresentMon();
    ~PresentMon();

    void AddEtlPath(std::wstring const& etlPath);
    void AddCsvPath(std::wstring const& csvPath);
    void Add(wchar_t const* args);
    void Start(char const* file, int line);

    // Returns true if the process is still running for timeoutMilliseconds
    bool IsRunning(DWORD timeoutMilliseconds=0) const;

    // Expect the process to exit with expectedExitCode within
    // timeoutMilliseconds (or kill it otherwise).
    void ExpectExited(char const* file, int line, DWORD timeoutMilliseconds=INFINITE, DWORD expectedExitCode=0);
};

#define PMSTART() Start(__FILE__, __LINE__)
#define PMEXITED(...) ExpectExited(__FILE__, __LINE__, __VA_ARGS__)

extern std::wstring outDir_;

// PresentMon.cpp
void AddTestFailure(char const* file, int line, char const* fmt, ...);

// PresentMonTests.cpp
bool EnsureDirectoryCreated(std::wstring path);
std::string Convert(std::wstring const& s);
std::wstring Convert(std::string const& s);

// GoldEtlCsvTests.cpp
void AddGoldEtlCsvTests(std::wstring const& dir, size_t relIdx, bool reportAllCsvDiffs);
