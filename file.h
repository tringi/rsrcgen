#ifndef FILE_H
#define FILE_H

#include <Windows.h>
#include <cstddef>

#ifndef CP_UTF16
#define CP_UTF16 1200
#endif
/*#ifndef CP_UTF16_BE
#define CP_UTF16_BE 1201
#endif
#ifndef CP_UTF32
#define CP_UTF32 12000
#endif
#ifndef CP_UTF32_BE
#define CP_UTF32_BE 12001
#endif*/

// file
//  - crude output file abstraction
//  - does not throw
//
class file {
    HANDLE  h;
    UINT    cp; // codepage

public:
    file (const wchar_t * filename, UINT cp);
    ~file ();

    // created
    //  - returns true if file was created
    //
    bool created () const;

    // write[ln]
    //  - converts (if needed) string to 'cp' and appends to file
    //  - writeln appends CR-LF
    //
    bool write (const wchar_t * string);
    bool write (const wchar_t * string, std::size_t length);
    bool writeln (const wchar_t * string);

private:
    file (const file &) = delete;
    file & operator = (const file &) = delete;
};

#endif
