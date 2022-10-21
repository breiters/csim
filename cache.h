#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>

#if 0
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define eprintf(...)
#endif

using Addr = size_t;
using StackIterator = std::list<Addr>::iterator;
using AddrMap = std::unordered_map<Addr, StackIterator>;

template <typename T>
static constexpr bool is_pow2(T a) { return !(a & (a - T{1})); }

// find first bit set
template <typename T>
static constexpr T ffs_constexpr(T x)
{
    T n{0};
    while ((x & 1) == 0) {
        x >>= 1;
        n++;
    }
    return n;
}

static_assert(ffs_constexpr(256) == 8, "ffs_constexpr(256) == 8"); // sanity check

struct CacheSet {
    CacheSet(size_t associativity) : associativity_{associativity}
    {
    }

    void on_access(Addr a)
    {
        eprintf("address=%p", a);
        assert(map_.size() == stack_.size());

        auto map_it = map_.find(a);
        if (map_it != map_.end()) {
            eprintf(" in cache\n");
            stack_.splice(stack_.begin(), stack_, map_it->second);
            ++hits_;
        } else {
            eprintf(" not in cache\n");
            // if address not in stack: push front
            stack_.push_front(a);
            map_[a] = stack_.begin();
            ++miss_;

            if (stack_.size() > associativity_) {
                map_.erase(stack_.back());
                stack_.erase(--stack_.end());
                assert(stack_.size() == associativity_);
            }
        }
    }

    size_t hits_{0U};
    size_t miss_{0U};
    size_t associativity_;
    std::list<Addr> stack_{};
    AddrMap map_{};
};

struct Cache {
    Cache() = default;
    Cache(size_t line_size, size_t cache_size, size_t associativity) : line_size_{line_size},
                                                                       cache_size_{cache_size},
                                                                       associativity_{associativity},
                                                                       line_bits_{ffs_constexpr(line_size)},
                                                                       sets_{std::vector<CacheSet>(cache_size / (line_size * associativity), CacheSet{associativity})}
    {
        assert(cache_size >= associativity);
        assert(is_pow2(line_size));
        assert(is_pow2(cache_size));
        assert(is_pow2(associativity));
        assert(associativity > 0);
    }

    size_t addr_hash(Addr a)
    {
        return (size_t)a & (cache_size_ / (line_size_ * associativity_) - 1U);
    }

    void on_access(Addr a)
    {
        size_t set = addr_hash(a);
        sets_[set].on_access(a);
    }

    size_t miss()
    {
        // return std::accumulate(sets_.begin(), sets_.end(), 0U, [](size_t sum, const CacheSet &s){ return sum + s.miss_; });
        // current Intel PIN implementation does not support accumulate, so we use a raw loop...
        size_t sum = 0U;
        for (CacheSet &cs : sets_) {
            sum += cs.miss_;
        }
        return sum;
    }

    size_t hits()
    {
        // return std::accumulate(sets_.begin(), sets_.end(), 0U, [](size_t sum, const CacheSet &s) { return sum + s.hits_; });
        size_t sum = 0U;
        for (CacheSet &cs : sets_) {
            sum += cs.hits_;
        }
        return sum;
    }

    size_t line_size_;
    size_t cache_size_;
    size_t associativity_;
    size_t line_bits_;
    std::vector<CacheSet> sets_;
};