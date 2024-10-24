#include <algorithm>
#include <chrono>
#include <ratio>
#include <vector>
#include <cmath>
#include <random>
#include <iostream>
#include <ranges>
#include <functional>
#include <numeric>

static const size_t NUM_ITEMS = 50000000;
static const size_t SEED = 42;
static std::mt19937 gen(SEED);
static std::mt19937 gen2(SEED);

float next_float() {
    std::uniform_real_distribution<> dist(-1.0, 1.0);
    return dist(gen);
}

unsigned int next_int() {
    //std::uniform_int_distribution<unsigned int> dist(128u, 1024u);
    std::uniform_int_distribution<unsigned int> dist(1u, 64u);
    return dist(gen2);
}

class Timer {
    using clock = std::chrono::high_resolution_clock;

public:
    void start() { t0 = clock::now(); }
    void stop() { t1 = clock::now(); }

    double elapsed_ms() const {
        auto d = std::chrono::duration<double, std::milli>(t1 - t0);
        return d.count();
    }

private:
    clock::time_point t0;
    clock::time_point t1;
};

//struct alignas(64) Triangle {
struct Triangle {
    float x0, y0;
    float x1, y1;
    float x2, y2;
    //char padding1[128-24];

    Triangle()
    :x0(next_float()), y0(next_float()),
     x1(next_float()), y1(next_float()),
     x2(next_float()), y2(next_float())
    {
    }

    float area() const {
        return 0.5 * std::abs(x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1));
    }

    void print() {
        std::cout
            << "(" << x0 << ", " << y0 << ")" << " - "
            << "(" << x1 << ", " << y1 << ")" << " - "
            << "(" << x2 << ", " << y2 << ")" << "\n";
    }

};

// DoNotOptimize() copied from Google Benchmark (https://github.com/google/benchmark)
//=========
#if defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !defined(__clang__)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE
#endif

template <class Tp>
inline ALWAYS_INLINE void DoNotOptimize(Tp& value) {
#if defined(__clang__)
  asm volatile("" : "+r,m"(value) : : "memory");
#else
  asm volatile("" : "+m,r"(value) : : "memory");
#endif
}
//=========

template<typename F>
double benchmark(int iter_count, int warmup_count, F fun) {
    for (volatile int i = 0; i < warmup_count; i++) {
        fun(-1);
    }
    Timer timer;
    timer.start();
    for (volatile int i = 0; i < iter_count; i++) {
        fun(i);
    }
    timer.stop();
    return timer.elapsed_ms() / iter_count;
}

int main(int argc, char* argv[]) {
    Timer timer;
    float sum;
    float out_sum;

    std::vector<Triangle> items(NUM_ITEMS);

    std::cout << "struct size: " << sizeof(Triangle) << "\n";

    std::cout << "--- Contiguous data\n";
    double time = benchmark(5, 2, [&](int iter) {
        float sum = 0.0;
        for(const auto it : items) {
            sum += it.area();
        }
        DoNotOptimize(sum);
    });
    std::cout << "time: " << time << " ms\n";

    std::cout << "--- Contiguous pointers to contiguous data\n";

    std::vector<const Triangle *> item_ptrs;
    item_ptrs.reserve(NUM_ITEMS);
    for(auto &it : items) {
        item_ptrs.push_back(&it);
    }
    time = benchmark(5, 2, [&](int iter) {
        float sum = 0.0;
        for(const auto ptr : item_ptrs) {
            sum += ptr->area();
        }
        DoNotOptimize(sum);
    });
    std::cout << "time: " << time << " ms\n";

    std::cout << "--- Contiguous pointers to scattered data\n";

    gen.seed(SEED);
    item_ptrs.clear();
    for(int i = 0; i < NUM_ITEMS; ++i) {
        item_ptrs.push_back(new Triangle);
    }

    time = benchmark(5, 2, [&](int iter) {
        float sum = 0.0;
        for(const auto ptr : item_ptrs) {
            sum += ptr->area();
        }
        DoNotOptimize(sum);
    });
    std::cout << "time: " << time << " ms\n";

    std::cout << "--- Contiguous pointers to very scattered data\n";
    gen.seed(SEED);
    item_ptrs.clear();
    std::vector<char *> garbage;
    for(int i = 0; i < NUM_ITEMS; ++i) {
        item_ptrs.push_back(new Triangle);

        auto garbage_size = next_int();
        garbage.push_back(new char[garbage_size]);
    }

    time = benchmark(5, 2, [&](int iter) {
        float sum = 0.0;
        for(const auto ptr : item_ptrs) {
            sum += ptr->area();
        }
        DoNotOptimize(sum);
    });
    std::cout << "time: " << time << " ms\n";

    std::for_each(garbage.begin(), garbage.end(), [](char* data) { delete[] data; });
    garbage.clear();

    std::cout << "--- Contiguous pointers to very scattered pointers to contiguous data\n";
    gen.seed(SEED);
    std::vector<Triangle **> item_ptr_of_ptrs;
    item_ptr_of_ptrs.reserve(NUM_ITEMS);
    for(int i = 0; i < NUM_ITEMS; ++i) {
        Triangle ** ptr = new Triangle*;
        *ptr = &items[i];
        item_ptr_of_ptrs.push_back(ptr);

        auto garbage_size = next_int();
        garbage.push_back(new char[garbage_size]);
    }

    time = benchmark(5, 2, [&](int iter) {
        float sum = 0.0;
        for(const auto ptr : item_ptr_of_ptrs) {
            sum += (*ptr)->area();
        }
        DoNotOptimize(sum);
    });
    std::cout << "time: " << time << " ms\n";

    std::for_each(garbage.begin(), garbage.end(), [](char* data) { delete[] data; });
    garbage.clear();

    return 0;
}
