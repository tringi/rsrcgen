#include <windows.h>
#include <shellapi.h>

#include <cstring>
#include <cwchar>
#include <cstdio>

#include "alloc.h"
#include "file.h"

#pragma warning(disable: 6262) // stack bytes

int argc = 0;
wchar_t ** argw = nullptr;
wchar_t infopath [32768];

linear_allocator <wchar_t> allocator;

// get

wchar_t * get (const wchar_t * filename, const wchar_t * app, const wchar_t * key, const wchar_t * default_ = nullptr);

// replace
//  - within buffer 'p' replaces first placeholder found in 'sub' (delimited by '%' characters) with 'value'
//  - 'value' may be nullptr for empty string
//  - reallocates 'p' if necessary
//  - returns pointer after the placeholder within 'sub' (possibly next placeholder)
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
        if (auto np = allocator.resize (p, n)) {
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
                    break;
                }

                section += std::wcslen (section) + 1;
            }

            if (!replaced) {

                // search includes

                if (auto includes = get (filename, L"include", nullptr)) {
                    while (!replaced && *includes) {
                        if (auto includerelative = get (filename, L"include", includes)) {

                            auto concatlen = std::wcslen (includerelative) + std::wcslen (filename) + 8;
                            auto concat = allocator.alloc (concatlen);
                            auto include = allocator.alloc (concatlen);

                            if (concat && include) {
                                std::wcscpy (concat, filename);
                                std::wcscat (concat, L"\\..\\");
                                std::wcscat (concat, includerelative);

                                if (GetFullPathName (concat, (DWORD) concatlen, include, NULL)) {

                                    // find the string in same sections in included files

                                    auto section = sections;
                                    while (!replaced && *section) {

                                        if (auto r = get (include, section, pp + 1)) {

                                            // replace

                                            *e = L'%';
                                            pp = replace (p, pp, r);
                                            replaced = true;
                                            break;
                                        }
                                        section += std::wcslen (section) + 1;
                                    }
                                }
                            }
                        }
                        includes += std::wcslen (includes) + 1;
                    }
                }

                if (!replaced) {

                    // try environment variable

                    wchar_t ev [32767];
                    if (GetEnvironmentVariable (pp + 1, ev, sizeof ev / sizeof ev [0])) {

                        // replace
                        *e = L'%';
                        pp = replace (p, pp, ev);

                    } else {

                        // not found, restore
                        *e = L'%';
                    }
                }
            }
        }
    }
}

// get
//  - abstracts ini file read, and immediately translates it
//  - returns nullptr if value is not found
//
wchar_t * get (const wchar_t * filename, const wchar_t * section, const wchar_t * value, const wchar_t * default_) {
    auto length = 8u;
    auto truncation = (section && value) ? 1u : 2u;
    auto p = allocator.alloc (length);
    if (p) {
        while (GetPrivateProfileStringW (section, value, default_ ? default_ : (wchar_t *) L"\xFFFD", p, length, filename) == length - truncation) {

            length *= 2u;
            if (auto np = allocator.resize (p, length)) {
                p = np;
            } else
                return nullptr;
        }

        if (p [0] == 0xFFFD)
            return nullptr;

        translate (p, filename, L"description\0\0");
    }
    return p;
}

// escape
//  - returns allocatee copy of 'input' with non-ASCII and backslash characters escaped
//
wchar_t * escape (wchar_t * input) {
    if (!input)
        return input;

    auto n = 0u;
    auto e = 0u;
    auto i = input;
    do {
        if (*i == L'\\') {
            ++e;
        }
        if (*i > 0x7E) {
            ++n;
        }
    } while (*i++);

    if (!e && !n)
        return input;

    if (auto p = allocator.alloc ((i - input) + e + 6 * n)) {
        auto o = p;

        i = input;
        do {
            if (*i == L'\\') {
                *o++ = L'\\';
            }
            if (*i > 0x7E) {
                std::swprintf (o, 7, L"\\x%04x", *i);
                o += 6;
            } else {
                *o++ = *i;
            }
        } while (*i++);

        return p;
    } else
        return input;
}

bool parse_charset (const wchar_t * value, int * cp) {
    if (std::wcscmp (value, L"utf-8") == 0 || std::wcscmp (value, L"UTF-8") == 0) {
        *cp = CP_UTF8;
        return true;
    }
    if (std::wcscmp (value, L"utf-16") == 0 || std::wcscmp (value, L"UTF-16") == 0) {
        *cp = CP_UTF16;
        return true;
    }
    return false;
}
int get_charset (const wchar_t * app, const wchar_t * specialized, int default_) {
    int cp;
    if (auto charset = get (app, L"generator", specialized)) {
        if (parse_charset (charset, &cp)) {
            return cp;
        }
    }

    if (auto charset = get (app, L"generator", L"charset")) {
        if (parse_charset (charset, &cp)) {
            return cp;
        }
    }
    return default_;
}

bool istrue (const wchar_t * value) {
    return std::wcscmp (value, L"true") == 0
        || std::wcscmp (value, L"TRUE") == 0
        || std::wcscmp (value, L"True") == 0
        || std::wcscmp (value, L"T") == 0
        || std::wcscmp (value, L"yes") == 0
        || std::wcscmp (value, L"YES") == 0
        || std::wcscmp (value, L"Yes") == 0
        || std::wcscmp (value, L"Y") == 0
        || std::wcstol (value, nullptr, 0) != 0
        ;
}

bool getbool (const wchar_t * app, const wchar_t * key) {
    if (auto value = get (infopath, app, key)) {
        return istrue (value);
    } else
        return false;
}

bool set (const wchar_t * app, const wchar_t * key, const wchar_t * value) {
    return WritePrivateProfileString (app, key, value, infopath);
}
bool set (const wchar_t * app, const wchar_t * key, long long number) {
    wchar_t value [32];
    std::swprintf (value, sizeof value / sizeof value [0], L" %lld", number);
    return set (app, key, value);
}

wchar_t * load (unsigned int id) {
    const wchar_t * ptr = nullptr;
    if (auto length = LoadString (GetModuleHandle (NULL), id, (LPTSTR) &ptr, 0)) {
        if (auto p = allocator.alloc (length + 1)) {
            std::memcpy (p, ptr, sizeof (wchar_t) * length);
            p [length] = L'\0';
            return p;
        }
    }
    return nullptr;
}

void roll_versioninfo (file & f, unsigned int id) {
    while (auto p = load (id++)) {
        translate (p, infopath, L"versioninfo\0description\0\0");
        f.writeln (p);
    }
}
void roll_manifest (file & f, unsigned int id) {
    while (auto p = load (id++)) {
        translate (p, infopath, L"manifest\0description\0\0");
        f.writeln (p);
    }
}

bool SetEnvironmentVariableV (const wchar_t * name, const wchar_t * format, ...) {
    va_list args;
    va_start (args, format);

    wchar_t value [32768];
    std::vswprintf (value, sizeof value / sizeof value [0], format, args);

    va_end (args);
    return SetEnvironmentVariableW (name, value);
}

void SetEnvironmentVariableIf (const wchar_t * section, const wchar_t * value, const wchar_t * variable, const wchar_t * ontrue, const wchar_t * onfalse) {
    if (getbool (section, value)) {
        SetEnvironmentVariable (variable, ontrue);
    } else {
        SetEnvironmentVariable (variable, onfalse);
    }
}

const wchar_t * get_filename (const wchar_t * path) {
    auto f = std::wcsrchr (path, L'/');
    auto b = std::wcsrchr (path, L'\\');

    if (f && b) {
        if (b > f)
            return b + 1;
        else
            return f + 1;
    } else
        if (b)
            return b + 1;
        else
            return f + 1;
}

int main () {
    std::printf ("%08X\n", GetVersion ());
    if ((argw = CommandLineToArgvW (GetCommandLineW (), &argc))) {
        if (argc > 1) {

            // fix .info path

            wchar_t * infofile = infopath;
            if (!GetFullPathName (argw [1], 32768, infopath, &infofile)) {
                std::wcscpy (infopath, argw [1]);
            }
            wprintf (L"rsrcgen %s > ", infofile);

            // common

            if (getbool (L"generator", L"autoincrement")) {
                if (auto build = get (infopath, L"description", L"build")) {
                    set (L"description", L"build", std::wcstoll (build, nullptr, 10) + 1LL);
                }
            }
            
            // set custom macros

            SYSTEMTIME st;
            GetLocalTime (&st);
            SetEnvironmentVariableV (L"YEAR",  L"%u", st.wYear);
            SetEnvironmentVariableV (L"MONTH", L"%u", st.wMonth);
            SetEnvironmentVariableV (L"DAY",   L"%u", st.wDay);

            if (auto value = get (infopath, L"versioninfo", L"language")) {
                SetEnvironmentVariableV (L"versioninfolangid", L"%04X", std::wcstoul (value, nullptr, 0));
            } else {
                SetEnvironmentVariable (L"versioninfolangid", L"0409");
            }

            if (auto value = get (infopath, L"versioninfo", L"codepage")) {
                SetEnvironmentVariableV (L"versioninfocp",     L"%d", std::wcstoul (value, nullptr, 0));
                SetEnvironmentVariableV (L"versioninfocphexa", L"%04X", std::wcstoul (value, nullptr, 0));
            } else {
                SetEnvironmentVariable (L"versioninfocp",     L"1200");
                SetEnvironmentVariable (L"versioninfocphexa", L"04B0");
            }

            SetEnvironmentVariableIf (L"versioninfo",    L"dll",        L"versioninfofiletype",     L"VFT_DLL",                 L"VFT_APP");
            SetEnvironmentVariableIf (L"versioninfo:ff", L"debug",      L"versioninfoffdebug",      L" | VS_FF_DEBUG",          L" ");
            SetEnvironmentVariableIf (L"versioninfo:ff", L"prerelease", L"versioninfoffprerelease", L" | VS_FF_PRERELEASE",     L" ");
            SetEnvironmentVariableIf (L"versioninfo:ff", L"private",    L"versioninfoffprivate",    L" | VS_FF_PRIVATEBUILD",   L" ");
            SetEnvironmentVariableIf (L"versioninfo:ff", L"special",    L"versioninfoffspecial",    L" | VS_FF_SPECIALBUILD",   L" ");

            // RSRC

            if (auto filename = get (infopath, L"versioninfo", L"filename")) {

                file f (filename, get_charset (infopath, L"versioncharset", CP_ACP));
                if (f.created ()) {
                    std::wprintf (L"%s", get_filename (filename));

                    // title

                    roll_versioninfo (f, 0x1000);

                    // values

                    if (auto values = get (infopath, L"versioninfo:values", nullptr)) {
                        while (*values) {
                            if (auto value = get (infopath, L"versioninfo:values", values)) {
                                f.write (L"            VALUE L\"");
                                f.write (values);
                                f.write (L"\", L\"");
                                f.write (escape (value));
                                f.write (L"\"\r\n");
                            }
                            values += std::wcslen (values) + 1;
                        }
                    }
                    
                    // remaining

                    roll_versioninfo (f, 0x1020);

                    // include manifest reference into generated RC

                    if (getbool (L"versioninfo", L"manifest")) {
                        if (auto filename = escape (get (infopath, L"manifest", L"filename"))) {
                            f.write (L"1 24 L\"");
                            f.write (filename);
                            f.write (L"\"\r\n");
                        }
                    }
                }
            } else {
                return ERROR_FILE_NOT_FOUND;
            }

            // MANIFEST

            if (auto filename = get (infopath, L"manifest", L"filename")) {
                auto cp = get_charset (infopath, L"manifestcharset", CP_UTF8);

                file f (filename, cp);
                if (f.created ()) {
                    std::wprintf (L" & %s", get_filename (filename));

                    switch (cp) {
                        case CP_UTF16:
                            SetEnvironmentVariable (L"manifestcharset", L"UTF-16");
                            break;
                        default:
                            SetEnvironmentVariable (L"manifestcharset", L"UTF-8");
                            break;
                    }

                    roll_manifest (f, 0x2000);

                    if (get (infopath, L"manifest", L"requestedExecutionLevel")) {
                        roll_manifest (f, 0x2200);
                    }

                    // main properties

                    roll_manifest (f, 0x2300);
                        
                    if (get (infopath, L"manifest", L"heapType")) roll_manifest (f, 0x2320);
                    if (get (infopath, L"manifest", L"longPathAware")) roll_manifest (f, 0x2322);
                    if (get (infopath, L"manifest", L"activeCodePage")) roll_manifest (f, 0x2324);
                    if (get (infopath, L"manifest", L"printerDriverIsolation")) roll_manifest (f, 0x2326);
                    if (get (infopath, L"manifest", L"disableWindowFiltering")) roll_manifest (f, 0x2328);

                    // TODO: some custom dpi level, auto generate these 3

                    if (get (infopath, L"manifest", L"dpiAware")) roll_manifest (f, 0x2350);
                    if (get (infopath, L"manifest", L"dpiAwareness")) roll_manifest (f, 0x2352);
                    if (get (infopath, L"manifest", L"gdiScaling")) roll_manifest (f, 0x2354);

                    // TODO: some custom scrolling level, auto generate these 2

                    if (get (infopath, L"manifest", L"highResolutionScrollingAware")) roll_manifest (f, 0x2370);
                    if (get (infopath, L"manifest", L"ultraHighResolutionScrollingAware")) roll_manifest (f, 0x2372);

                    roll_manifest (f, 0x23F0);

                    // supported OS

                    if (get (infopath, L"manifest", L"supportedOS:1")) {
                        roll_manifest (f, 0x2400);

                        auto i = 1;
                        do {
                            wchar_t name [64];
                            std::swprintf (name, sizeof name / sizeof name [0], L"supportedOS:%d", i);

                            if (auto guid = get (infopath, L"manifest", name)) {
                                if (auto comment = std::wcschr (guid, L' ')) {
                                    *comment = L'\0';
                                    ++comment;
                                    SetEnvironmentVariable (L"manifestoscomment", comment);
                                    SetEnvironmentVariable (L"manifestos", guid);
                                    roll_manifest (f, 0x2410);
                                } else {
                                    SetEnvironmentVariable (L"manifestos", guid);
                                    roll_manifest (f, 0x2412);
                                }
                                ++i;
                            } else
                                break;
                        } while (true);

                        if (auto maxversiontested = get (infopath, L"manifest", L"maxversiontested")) {
                            SetEnvironmentVariable (L"maxversiontested", maxversiontested);
                            roll_manifest (f, 0x2414);
                        }

                        roll_manifest (f, 0x24F0);
                    }
                    if (get (infopath, L"manifest", L"dependentAssembly:1")) {
                        roll_manifest (f, 0x2700);

                        auto i = 1;
                        do {
                            wchar_t name [64];
                            std::swprintf (name, sizeof name / sizeof name [0], L"dependentAssembly:%d", i);

                            if (auto dependency = get (infopath, L"manifest", name)) {
                                if (auto version = std::wcschr (dependency, L' ')) {
                                    *version = L'\0';
                                    ++version;

                                    if (auto token = std::wcschr (version, L' ')) {
                                        *token = L'\0';
                                        ++token;

                                        SetEnvironmentVariable (L"manifestdependency", dependency);
                                        SetEnvironmentVariable (L"manifestdependencyversion", version);
                                        SetEnvironmentVariable (L"manifestdependencytoken", token);
                                        roll_manifest (f, 0x2710);
                                    }
                                }
                                ++i;
                            } else
                                break;
                        } while (true);

                        roll_manifest (f, 0x27F0);
                    }
                    roll_manifest (f, 0x2FF0);
                }
            } else {
                std::wprintf (L"\n");
                return ERROR_FILE_NOT_FOUND;
            }

            std::wprintf (L"\n");
            return ERROR_SUCCESS;
        } else
            return ERROR_INVALID_PARAMETER;
    } else
        return GetLastError ();
}

