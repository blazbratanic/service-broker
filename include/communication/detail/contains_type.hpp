#ifndef CONTAINS_TYPE_HPP
#define CONTAINS_TYPE_HPP

#include <type_traits>

template <typename T, typename Collection>
struct contains_type {};

template <typename T>
struct contains_type<T, std::tuple<>> {
  typedef std::false_type value;
};

template <typename T, typename... Args>
struct contains_type<T, std::tuple<T, Args...>> {
  typedef std::true_type value;
};

template <typename T, typename U, typename... Args>
struct contains_type<T, std::tuple<U, Args...>> {
  typedef typename contains_type<T, std::tuple<Args...>>::value value;
};

#endif
