#include <windows.h>
#include <shellapi.h>

#include <cstring>
#include <cwchar>
#include <cstdio>

#pragma warning(disable: 6262) // stack bytes

int argc = 0;
wchar_t ** argw = nullptr;
HANDLE heap = NULL;

wchar_t * get (const wchar_t * filename, const wchar_t * app, const wchar_t * key, const wchar_t * default_ = nullptr);

// replace
//  - within buffer 'p' replaces first placeholder found in 'sub' (delimited by '%' characters) with 'value'
//  - 'value' may be nullptr for empty string
//  - reallocates 'p' if necessary
//  - returns pointer after the placeholder within 'sub'
//     - or nullptr on re-allocation failure
//
wchar_t * replace (wchar_t *& p, wchar_t * sub, const wchar_t * value) {
    const auto nsub = std::wcschr (sub + 1, L'%')
                    ? std::wcschr (sub + 1, L'%') - sub + 1
                    : std::wcslen (sub);
    const auto nval = value
                    ? std::wcslen (value)
                    : 0u;

    if (nval > nsub) {
        auto n = std::wcslen (p) - nsub + nval + 2;
        auto m = sub - p;
        if (auto np = static_cast <wchar_t *> (HeapReAlloc (heap, 0, p, sizeof (wchar_t) * n))) {
            sub = np + m;
            p = np;
        } else
            return nullptr;
    };

    if (nval != nsub) {
        std::memmove (sub + nval, sub + nsub, sizeof (wchar_t) * (std::wcslen (sub) - nsub + 1));
    };

    std::memcpy (sub, value, sizeof (wchar_t) * nval);
    return sub + nval;
};

// translate
//  - primary placeholders replacing routine
//  - attempts to find and replace all placeholders from
//     - command-line string, format: "placeholder=value"
//     - values defined in 'section'
//     - environment variables of the same name
//  - sections must be double-NUL terminated string
//  - reallocates buffer 'p' if necessary
//
void translate (wchar_t *& p, const wchar_t * const filename, const wchar_t * const sections) {
    auto pp = p;
    auto pr = p - 1;

next:
    while (pp && (pp = std::wcschr (p, L'%')) && pp != pr) {
        pr = pp;

        if (auto e = std::wcschr (pp + 1, L'%')) {

            // check command line strings

            for (auto i = 2; i != argc; ++i) {
                if (std::wcsncmp (pp + 1, argw [i], e - pp - 1) == 0) {

                    if (argw [i][e - pp - 1] == L'=') {
                        pp = replace (p, pp, argw [i] + (e - pp - 1) + 1);
                        goto next;
                    };
                };
            };

            // check description strings

            *e = L'\0';

            bool replaced = false;
            auto section = sections;
            while (!replaced && *section) {
                if (auto r = get (filename, section, pp + 1)) {

                    // replace
                    *e = L'%';
                    pp = replace (p, pp, r);
                    replaced = true;
                }

                section += std::wcslen (section) + 1;
            }

            if (!replaced) {

                // search includes



                // try environment variable

                wchar_t ev [32767];
                if (GetEnvironmentVariable (pp + 1, ev, sizeof ev / sizeof ev [0])) {

                    // replace
                    *e = L'%';
                    pp = replace (p, pp, ev);

                } else {

                    // not found, restore
                    *e = L'%';
                };
            };
        };
    };

    return;
};

// get
//  - abstracts ini file read, and immediately translates it
//  - returns nullptr if value is not found
//
wchar_t * get (const wchar_t * filename, const wchar_t * section, const wchar_t * value, const wchar_t * default_) {
    auto length = 16u;
    auto truncation = (section && value) ? 1u : 2u;
    auto p = static_cast <wchar_t *> (HeapAlloc (heap, 0, sizeof (wchar_t) * length));

    if (p) {
        while (GetPrivateProfileStringW (section, value, default_ ? default_ : (wchar_t *) L"\xFFFD", p, length, filename) == length - truncation) {

            length *= 2u;
            if (auto np = static_cast <wchar_t *> (HeapReAlloc (heap, 0, p, sizeof (wchar_t) * length))) {
                p = np;
            } else {
                HeapFree (heap, 0, p);
                return nullptr;
            };
        };

        if (p [0] == 0xFFFD)
            return nullptr;

        translate (p, filename, L"description\0\0");
    };

    return p;
};

// escape
//  - returns allocatee copy of 'input' with non-ASCII and backslash characters escaped
//
const wchar_t * escape (const wchar_t * const input) {
    if (!input)
        return input;

    auto n = 0u;
    auto e = 0u;
    auto i = input;
    do {
        if (*i == L'\\') {
            ++e;
        };
        if (*i > 0x7E) {
            ++n;
        };
    } while (*i++);

    if (auto p = static_cast <wchar_t *> (HeapAlloc (heap, 0, sizeof (wchar_t) * ((i - input) + e + 6 * n)))) {
        auto o = p;

        i = input;
        do {
            if (*i == L'\\') {
                *o++ = L'\\';
            };
            if (*i > 0x7E) {
                std::swprintf (o, 7, L"\\x%04x", *i);
                o += 6;
            } else {
                *o++ = *i;
            };
        } while (*i++);

        return p;
    } else
        return input;
};

bool istrue (const wchar_t * value) {
    return std::wcscmp (value, L"true") == 0
        || std::wcscmp (value, L"TRUE") == 0
        || std::wcscmp (value, L"True") == 0
        || std::wcscmp (value, L"T") == 0
        || std::wcscmp (value, L"yes") == 0
        || std::wcscmp (value, L"YES") == 0
        || std::wcscmp (value, L"Tes") == 0
        || std::wcscmp (value, L"Y") == 0
        || std::wcstol (value, nullptr, 0) != 0
        ;
};

bool getbool (const wchar_t * app, const wchar_t * key) {
    if (auto value = get (argw [1], app, key)) {
        if (istrue (value)) {
            return true;
        };
    };
    return false;
};

bool set (const wchar_t * app, const wchar_t * key, const wchar_t * value) {
    return WritePrivateProfileString (app, key, value, argw [1]);
};

bool set (const wchar_t * app, const wchar_t * key, long long number) {
    wchar_t value [32];
    _snwprintf (value, sizeof value / sizeof value [0], L" %I64d", number);

    return set (app, key, value);
};

wchar_t * load (unsigned int id) {
    const wchar_t * ptr = nullptr;
    if (auto len = LoadString (GetModuleHandle (NULL), id, (LPTSTR) &ptr, 0)) {
        if (auto p = static_cast <wchar_t *> (HeapAlloc (heap, 0, sizeof (wchar_t) * (len + 1)))) {

            std::memcpy (p, ptr, sizeof (wchar_t) * len);
            p [len] = L'\0';

            return p;
        };
    };

    return nullptr;
};

bool roll_versioninfo_string (HANDLE h, const wchar_t * p) {
    DWORD written;
    char string [65536];

    if (auto length = WideCharToMultiByte (CP_ACP, 0, p, -1, string, sizeof string - 3, NULL, NULL)) {
        WriteFile (h, string, length - 1, &written, NULL);
        return true;
    } else
        return false;
};

void roll_versioninfo (HANDLE h, unsigned int id) {
    while (auto p = load (id++)) {
        translate (p, argw [1], L"versioninfo\0description\0\0");

        if (roll_versioninfo_string (h, p)) {
            DWORD written;
            WriteFile (h, "\r\n", 2, &written, NULL);
        };
    };
    return;
};


void roll_manifest (HANDLE h, unsigned int id) {
    while (auto p = load (id++)) {
        translate (p, argw [1], L"manifest\0description\0\0");

        DWORD written;
        WriteFile (h, p, sizeof (wchar_t) * std::wcslen (p), &written, NULL);
        WriteFile (h, L"\r\n", sizeof (wchar_t) * 2, &written, NULL);
    };
    return;
};

bool SetEnvironmentVariableV (const wchar_t * name, const wchar_t * format, ...) {
    va_list args;
    va_start (args, format);

    wchar_t value [32768];
    std::vswprintf (value, sizeof value / sizeof value [0], format, args);

    va_end (args);
    return SetEnvironmentVariableW (name, value);
};

int main () {
    printf ("generating version info and manifest...\n"); // TODO: read from our rsrc
    heap = GetProcessHeap ();

    if ((argw = CommandLineToArgvW (GetCommandLineW (), &argc))) {
        if (argc > 1) {

            // common

            if (getbool (L"generator", L"autoincrement")) {
                if (auto build = get (argw [1], L"description", L"build")) {
                    set (L"description", L"build", std::wcstoll (build, nullptr, 10) + 1LL);
                };
            };

            // set custom macros

            SYSTEMTIME st;
            GetLocalTime (&st);
            SetEnvironmentVariableV (L"YEAR", L"%u", st.wYear);
            SetEnvironmentVariableV (L"MONTH", L"%u", st.wMonth);
            SetEnvironmentVariableV (L"DAY", L"%u", st.wDay);

            if (auto value = get (argw [1], L"versioninfo", L"language")) {
                SetEnvironmentVariableV (L"versioninfolangid", L"%04X", std::wcstoul (value, nullptr, 0));
            } else {
                SetEnvironmentVariable (L"versioninfolangid", L"0409");
            };

            if (auto value = get (argw [1], L"versioninfo", L"codepage")) {
                SetEnvironmentVariableV (L"versioninfocp", L"%d", std::wcstoul (value, nullptr, 0));
                SetEnvironmentVariableV (L"versioninfocphexa", L"%04X", std::wcstoul (value, nullptr, 0));
            } else {
                SetEnvironmentVariable (L"versioninfocp", L"1200");
                SetEnvironmentVariable (L"versioninfocphexa", L"04B0");
            };

            if (getbool (L"versioninfo", L"dll")) {
                SetEnvironmentVariable (L"versioninfofiletype", L"VFT_DLL");
            } else {
                SetEnvironmentVariable (L"versioninfofiletype", L"VFT_APP");
            };

            if (getbool (L"versioninfo:ff", L"debug")) {
                SetEnvironmentVariable (L"versioninfoffdebug", L" | VS_FF_DEBUG");
            } else {
                SetEnvironmentVariable (L"versioninfoffdebug", L" ");
            };
            if (getbool (L"versioninfo:ff", L"prerelease")) {
                SetEnvironmentVariable (L"versioninfoffprerelease", L" | VS_FF_PRERELEASE");
            } else {
                SetEnvironmentVariable (L"versioninfoffprerelease", L" ");
            };
            if (getbool (L"versioninfo:ff", L"private")) {
                SetEnvironmentVariable (L"versioninfoffprivate", L" | VS_FF_PRIVATEBUILD");
            } else {
                SetEnvironmentVariable (L"versioninfoffprivate", L" ");
            };
            if (getbool (L"versioninfo:ff", L"special")) {
                SetEnvironmentVariable (L"versioninfoffspecial", L" | VS_FF_SPECIALBUILD");
            } else {
                SetEnvironmentVariable (L"versioninfoffspecial", L" ");
            };

            // RSRC

            if (auto filename = get (argw [1], L"versioninfo", L"filename")) {
                wprintf (L"%s\n", filename);

                auto h = CreateFile (filename, GENERIC_WRITE, 0, NULL,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (h != INVALID_HANDLE_VALUE) {

                    // title

                    roll_versioninfo (h, 0x100);

                    // values

                    if (auto values = get (argw [1], L"versioninfo:values", nullptr)) {
                        while (*values) {
                            if (auto value = get (argw [1], L"versioninfo:values", values)) {
                                DWORD written;

                                WriteFile (h, "            VALUE L\"", 20, &written, NULL);
                                roll_versioninfo_string (h, values);
                                WriteFile (h, "\", L\"", 5, &written, NULL);
                                roll_versioninfo_string (h, escape (value));
                                WriteFile (h, "\"\r\n", 3, &written, NULL);

                            };
                            values += std::wcslen (values) + 1;
                        };
                    };

                    // remaining

                    roll_versioninfo (h, 0x120);

                    // include manifest reference into generated RC

                    if (getbool (L"versioninfo", L"manifest")) {
                        if (auto filename = escape (get (argw [1], L"manifest", L"filename"))) {

                            DWORD written;
                            char buffer [256];

                            WriteFile (h, "1 24 L\"", 7, &written, NULL);

                            if (auto length = WideCharToMultiByte (CP_ACP, 0, filename, -1, buffer, sizeof buffer, NULL, NULL)) {
                                WriteFile (h, buffer, length - 1, &written, NULL);
                            };

                            WriteFile (h, "\"\r\n", 3, &written, NULL);
                        };
                    };
                    CloseHandle (h);
                };
            };

            // MANIFEST

            if (auto filename = get (argw [1], L"manifest", L"filename")) {
                wprintf (L"%s\n", filename);

                auto h = CreateFile (filename, GENERIC_WRITE, 0, NULL,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (h != INVALID_HANDLE_VALUE) {

                    DWORD written;
                    WriteFile (h, "\xFF\xFE", 2, &written, NULL);

                    roll_manifest (h, 0x200);

                    if (get (argw [1], L"manifest", L"requestedExecutionLevel")) {
                        roll_manifest (h, 0x220);
                    };
                    // PerMonitorV2,PerMonitor
                    if (get (argw [1], L"manifest", L"dpiAware") || get (argw [1], L"manifest", L"dpiAwareness") || get (argw [1], L"manifest", L"longPathAware")) {
                        roll_manifest (h, 0x230);
                        if (get (argw [1], L"manifest", L"dpiAware")) {
                            roll_manifest (h, 0x233);
                        };
                        if (get (argw [1], L"manifest", L"dpiAwareness")) {
                            roll_manifest (h, 0x237);
                        };
                        if (get (argw [1], L"manifest", L"longPathAware")) {
                            roll_manifest (h, 0x235);
                        };
                        if (get (argw [1], L"manifest", L"heapType")) {
                            roll_manifest (h, 0x239);
                        };
                        roll_manifest (h, 0x23D);
                    };
                    if (get (argw [1], L"manifest", L"supportedOS:1")) {
                        roll_manifest (h, 0x240);

                        auto i = 1;
                        do {
                            wchar_t name [64];
                            _snwprintf (name, sizeof name / sizeof name [0], L"supportedOS:%d", i);
                            if (auto guid = get (argw [1], L"manifest", name)) {
                                if (auto comment = std::wcschr (guid, L' ')) {
                                    *comment = L'\0';
                                    ++comment;
                                    SetEnvironmentVariable (L"manifestoscomment", comment);
                                    SetEnvironmentVariable (L"manifestos", guid);
                                    roll_manifest (h, 0x250);
                                } else {
                                    SetEnvironmentVariable (L"manifestos", guid);
                                    roll_manifest (h, 0x252);
                                }

                                ++i;
                            } else
                                break;
                        } while (true);

                        if (auto maxversiontested = get (argw [1], L"manifest", L"maxversiontested")) {
                            SetEnvironmentVariable (L"maxversiontested", maxversiontested);
                            roll_manifest (h, 0x254);
                        }

                        roll_manifest (h, 0x260);
                    };
                    if (get (argw [1], L"manifest", L"dependentAssembly:1")) {
                        roll_manifest (h, 0x270);

                        auto i = 1;
                        do {
                            wchar_t name [64];
                            _snwprintf (name, sizeof name / sizeof name [0], L"dependentAssembly:%d", i);
                            if (auto dependency = get (argw [1], L"manifest", name)) {
                                if (auto version = std::wcschr (dependency, L' ')) {
                                    *version = L'\0';
                                    ++version;
                                    if (auto token = std::wcschr (version, L' ')) {
                                        *token = L'\0';
                                        ++token;

                                        SetEnvironmentVariable (L"manifestdependency", dependency);
                                        SetEnvironmentVariable (L"manifestdependencyversion", version);
                                        SetEnvironmentVariable (L"manifestdependencytoken", token);
                                        roll_manifest (h, 0x280);
                                    };
                                };
                                ++i;
                            } else
                                break;
                        } while (true);

                        roll_manifest (h, 0x290);
                    };

                    roll_manifest (h, 0x2F0);
                    CloseHandle (h);
                };
            };

            return ERROR_SUCCESS;
        } else
            return ERROR_INVALID_PARAMETER;
    } else
        return GetLastError ();
}

