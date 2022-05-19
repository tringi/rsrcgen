#include <Windows.h>
#include "alloc.h"

void * linear_allocator_base::init (std::size_t reserve, std::size_t commit) {
    auto p = VirtualAlloc (NULL, reserve, MEM_RESERVE, PAGE_READWRITE);
    if (p) {
        this->commit (p, commit);
    }
    return p;
}

bool linear_allocator_base::commit (void * base, std::size_t n) {
    if (n > this->bytes) {
        n = (n + 4095) & ~4095; // round up 4k

        if (VirtualAlloc (base, n, MEM_COMMIT, PAGE_READWRITE)) {
            this->bytes = n;
        } else
            return false;
    }
    return true;
}

void linear_allocator_base::dump (void * p) {
    if (auto h = CreateFile (L"dump.dat", GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) {
        DWORD written;
        WriteFile (h, p, bytes, &written, NULL);
        CloseHandle (h);
    }
    std::printf ("DEBUG: commit was %zu\n", bytes);
    // VirtualFree (this->base, 0, MEM_RELEASE);
}
