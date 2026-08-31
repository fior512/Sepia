#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <new>

using namespace Sepia;

TEST_CASE("AlignedBuffer 64-byte alignment") {
    AlignedBuffer<double> buf(100);
    // posix_memalign called with Alignment=64
    CHECK(reinterpret_cast<uintptr_t>(buf.data()) % 64 == 0);
}

TEST_CASE("AlignedBuffer size and data non-null") {
    AlignedBuffer<double> buf(100);
    CHECK(buf.size() == 100);
    CHECK(buf.data() != nullptr);
}

TEST_CASE("AlignedBuffer move constructor nulls source") {
    AlignedBuffer<double> a(50);
    double* original_ptr = a.data();
    AlignedBuffer<double> b(std::move(a));
    CHECK(b.size() == 50);
    CHECK(b.data() == original_ptr);
    CHECK(a.size() == 0);
    CHECK(a.data() == nullptr);
}

TEST_CASE("AlignedBuffer move assignment nulls source") {
    AlignedBuffer<double> a(50);
    AlignedBuffer<double> b;
    b = std::move(a);
    CHECK(b.size() == 50);
    CHECK(a.size() == 0);
    CHECK(a.data() == nullptr);
}

TEST_CASE("AlignedBuffer operator[] write and read") {
    AlignedBuffer<double> buf(4);
    buf[0] = 1.1; buf[1] = 2.2; buf[2] = 3.3; buf[3] = 4.4;
    CHECK(buf[0] == Catch::Approx(1.1));
    CHECK(buf[3] == Catch::Approx(4.4));
}

TEST_CASE("Arena initial state") {
    detail::Arena arena(1024);
    CHECK(arena.used() == 0);
    CHECK(arena.capacity() == 1024);
}

TEST_CASE("Arena alloc alignment satisfied") {
    detail::Arena arena(256);
    // arena buf_ is 64-byte aligned; first alloc at offset=0,
    // padding=(16-0%16)%16=0; returned ptr inherits buf_ alignment (64%16==0)
    void* p = arena.alloc(16, 16);
    CHECK(reinterpret_cast<uintptr_t>(p) % 16 == 0);
    CHECK(arena.used() == 16);
}

TEST_CASE("Arena alloc OOM throws bad_alloc") {
    detail::Arena arena(64);
    CHECK_THROWS_AS(arena.alloc(65, 1), std::bad_alloc);
}

TEST_CASE("Arena reset returns used to zero") {
    detail::Arena arena(256);
    arena.alloc(64, 8);
    arena.reset();
    CHECK(arena.used() == 0);
}

TEST_CASE("Arena create<T> constructs in place") {
    detail::Arena arena(256);
    int* p = arena.create<int>(42);
    REQUIRE(p != nullptr);
    CHECK(*p == 42);
}
