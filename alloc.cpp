#include <Windows.h>
#include "alloc.h"

linear_allocator_base::linear_allocator_base (std::size_t reserve, std::size_t commit)
    : base (VirtualAlloc (NULL, reserve, MEM_RESERVE, PAGE_READWRITE)) {
    
    if (this->base) {
        this->commit (commit);
    }
}

bool linear_allocator_base::commit (std::size_t n) {
    n = (n + 4095) & ~4095; // round up 4k

    if (VirtualAlloc (this->base, n, MEM_COMMIT, PAGE_READWRITE)) {
        this->bytes = n;
        return true;
    } else
        return false;
}

linear_allocator_base::~linear_allocator_base () {
    /*if (auto h = CreateFile (L"dump.dat", GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) {
        DWORD written;
        WriteFile (h, this->base, (DWORD) this->bytes, &written, NULL);
        CloseHandle (h);
    }// */
    // std::printf ("DEBUG: commit was %zu\n", this->bytes);// */
    // VirtualFree (this->base, 0, MEM_RELEASE);
}
