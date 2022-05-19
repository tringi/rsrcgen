#ifndef ALLOC_H
#define ALLOC_H

#include <cstdio>
#include <cwchar>

class linear_allocator_base {
    std::size_t bytes = 0;

protected:
    void * init (std::size_t reserve, std::size_t commit);
    void dump (void *);
    bool commit (void *, std::size_t n);
};

template <typename T>
class linear_allocator : linear_allocator_base {
    T * const   base;
    T *         next;
    T *         prev;

    bool commit (std::size_t n) {
        return linear_allocator_base::commit (this->base, (this->next - this->base + n) * sizeof (T));
    }

public:
    explicit linear_allocator (std::size_t reserve, std::size_t commit)
        : base ((T *) this->init (sizeof (T) * reserve, sizeof (T) * commit))
        , next (base)
        , prev (nullptr) {}

    ~linear_allocator () {
        this->dump (this->base);
    }

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
