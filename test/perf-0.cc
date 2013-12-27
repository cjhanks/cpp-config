
#include "config.hh"
#include <cassert>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <vector>


#define TEST(_functor_, ...) \
    do { \
        clock_t c = clock(); \
        _functor_(__VA_ARGS__); \
        fprintf(stderr, "%ld\n", clock() - c); \
    } while (0)

enum { COUNT = 10000 };

static void test_cfg();
static void test_lup();
static void test_dat(const std::vector<int64_t>&);

int 
main() { 
    config::initialize("./test/perf-0.cfg");
    
    TEST(test_cfg);
    /* -----------------------------*/
    TEST(test_lup);
    
    /* -----------------------------*/
    {
    std::vector<int64_t> vec;

    for (size_t q = 0; q < 13; ++q)
        vec.push_back(q);
    TEST(test_dat, vec);
    }
    return 0;
}

static void
test_cfg() {
    const kwarg_vector& vec = CFG->vector("data_vector");

    for (size_t i = 0; i < COUNT; ++i) {
        ssize_t Q = 0;
        for (auto it  = vec->cbegin();
                  it != vec->cend();
                ++it) 
            assert((*it)->as<int64_t>() == Q++);
    }
}

static void
test_lup() {
    for (size_t i = 0; i < COUNT; ++i) {
        ssize_t Q = 0;
        for (auto it  = CFG->vector("data_vector")->cbegin();
                  it != CFG->vector("data_vector")->cend();
                ++it) 
            assert((*it)->as<int64_t>() == Q++);
    }
}


static void
test_dat(const std::vector<int64_t>& vec) {
    for (size_t i = 0; i < COUNT; ++i) {
        ssize_t Q = 0;
        for (auto it  = vec.begin() ;
                  it != vec.end()   ;
                ++it) 
            assert(*it == Q++);
    }
}
