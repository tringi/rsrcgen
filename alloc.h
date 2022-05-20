#ifndef ALLOC_H
#define ALLOC_H

#include <cstdio>
#include <cwchar>

class linear_allocator_base {
protected:
    void * const    base;
    std::size_t     bytes = 0;

protected:
    linear_allocator_base (std::size_t reserve, std::size_t commit);
    ~linear_allocator_base ();

    bool commit (std::size_t n);
};

template <typename T>
class linear_allocator : linear_allocator_base {
    T *         next;
    T *         prev;

    inline bool commit (std::size_t n) {
        n += this->next - (T *) this->base;
        n *= sizeof (T);

        if (n > this->bytes) {
            return this->linear_allocator_base::commit (n);
        } else
            return true;
    }

public:
    explicit linear_allocator (std::size_t reserve = 0x100'0000, std::size_t commit = 0x1000 / sizeof (T))
        : linear_allocator_base (sizeof (T) * reserve, sizeof (T) * commit)
        , next (static_cast <T *> (this->base))
        , prev (nullptr) {}

    inline T * alloc (std::size_t n) {
        if (this->commit (n)) {
            this->prev = this->next;
            this->next += n;
            return this->prev;
        } else
            return nullptr;
    }

    inline T * resize (T * p, std::size_t n) {
        if (p == this->prev) {
            if (this->commit (n)) {
                this->next = this->prev + n;
                return p;
            }
        } else {
            if (auto q = this->alloc (n)) {
                if (p) {
                    std::wmemcpy (q, p, n);
                }
                return q;
            }
        }
        return nullptr;
    }
};

#endif
