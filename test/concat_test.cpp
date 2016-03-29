#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <type_traits>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <communication/Concat.hpp>

struct A {
  int id;
};
int index(A const &a) { return a.id; }
struct B {
  int id;
};
int index(B const &a) { return a.id; }
struct C {
  int id;
};
int index(C const &a) { return a.id; }

TEST(ConcatTest, TypeOccurences) {
  constexpr int v0 = detail::type_occurences<int, int, int, int, bool>::value;
  ASSERT_EQ(3u, v0);
  constexpr int v1 = detail::type_occurences<bool, int, int, int, bool>::value;
  ASSERT_EQ(1u, v1);
}

TEST(ConcatTest, Concatenate) {
  Concat<A, A, B, C> concat;
  A a1 = {1};
  A a2 = {1};
  B b = {1};
  C c = {1};

  decltype(concat)::result_type result;

  concat.put<0>(a1);
  ASSERT_FALSE(concat.try_get(result));
  concat.put<1>(a2);
  ASSERT_FALSE(concat.try_get(result));
  concat.put(b);
  ASSERT_FALSE(concat.try_get(result));
  concat.put(c);
  ASSERT_TRUE(concat.try_get(result));
}

TEST(ConcatTest, ConcatenateThreaded) {
  Concat<A, A, B, C> concat;

  // Main thread will fill two variables. The other two variables will be filled
  // by an auxiliary thread.
  A a1 = {1};
  A a2 = {1};
  decltype(concat)::result_type result;

  concat.put<0>(a1);
  ASSERT_FALSE(concat.try_get(result));
  concat.put<1>(a2);
  ASSERT_FALSE(concat.try_get(result));

  auto put_thread = std::thread([&concat]() {
    B b = {1};
    C c = {1};
    concat.put(b);
    concat.put(c);
  });

  // Wait until the auxiliary thread fills the remaining variables and obtain
  // result.
  auto result2 = concat.get();

  // Check if the result is correct
  ASSERT_EQ(1, std::get<0>(result2).id);
  ASSERT_EQ(1, std::get<1>(result2).id);
  ASSERT_EQ(1, std::get<2>(result2).id);
  ASSERT_EQ(1, std::get<3>(result2).id);

  put_thread.join();
}

TEST(ConcatTest, ConcatenateThreaded2) {
  Concat<A, A, B, C> concat;

  auto put_thread1 = std::thread([&concat]() {
    for (int i = 0; i < 10000; ++i) {
      A a1 = {i};
      concat.put<0>(a1);
    }
  });

  auto put_thread2 = std::thread([&concat]() {
    for (int i = 0; i < 10000; ++i) {
      A a2 = {i};
      concat.put<1>(a2);
    }
  });

  auto put_thread3 = std::thread([&concat]() {
    for (int i = 0; i < 10000; ++i) {
      B b = {i};
      concat.put(b);
    }
  });

  auto put_thread4 = std::thread([&concat]() {
    for (int i = 0; i < 10000; ++i) {
      C c = {i};
      concat.put(c);
    }
  });

  for (int i = 0; i < 10000; ++i) {
    auto result = concat.get();
    auto a1 = std::get<0>(result).id;
    auto a2 = std::get<1>(result).id;
    auto b = std::get<2>(result).id;
    auto c = std::get<3>(result).id;
    ASSERT_EQ(a1, a2);
    ASSERT_EQ(a1, b);
    ASSERT_EQ(a1, c);
  }
  put_thread1.join();
  put_thread2.join();
  put_thread3.join();
  put_thread4.join();
}
