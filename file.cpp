#include "file.h"

#include <cstring>
#include <cwchar>

#pragma warning(disable: 6262) // stack bytes

file::file (const wchar_t * filename, UINT cp)
    : h (CreateFile (filename, GENERIC_WRITE, 0, NULL,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
    , cp (cp) {

    if (this->h != INVALID_HANDLE_VALUE) {
        switch (cp) {
            case CP_UTF8:
            case CP_UTF16:
                this->write (L"\xFEFF", 1);
        }
    }
}

file::~file () {
    if (this->h != INVALID_HANDLE_VALUE) {
        CloseHandle (this->h);
    }
}

bool file::created () const {
    return this->h != INVALID_HANDLE_VALUE;
}

bool file::write (const wchar_t * string) {
    if (this->h == INVALID_HANDLE_VALUE)
        return false;

    DWORD written;
    if (this->cp != CP_UTF16) {

        char buffer [65536];
        if (auto length = WideCharToMultiByte (this->cp, 0, string, -1, buffer, sizeof buffer, NULL, NULL)) {
            return WriteFile (this->h, buffer, length - 1, &written, NULL);
        } else
            return false;
    } else
        return WriteFile (this->h, string, (DWORD) (sizeof (wchar_t) * std::wcslen (string)), &written, NULL);
}

bool file::write (const wchar_t * string, std::size_t size) {
    if (this->h == INVALID_HANDLE_VALUE)
        return false;

    DWORD written;
    if (this->cp != CP_UTF16) {

        static char buffer [65536];
        if (auto length = WideCharToMultiByte (this->cp, 0, string, (DWORD) size, buffer, sizeof buffer, NULL, NULL)) {
            return WriteFile (this->h, buffer, length, &written, NULL);
        } else
            return false;
    } else
        return WriteFile (this->h, string, (DWORD) (sizeof (wchar_t) * size), &written, NULL);
}

bool file::writeln (const wchar_t * string) {
    return this->write (string)
        && this->write (L"\r\n", 2);
}