#ifndef TUPLE_UTIL_HPP
#define TUPLE_UTIL_HPP

template <typename T, typename Collection>
struct prepend_type {};

template <typename T, typename... Args>
struct prepend_type<T, std::tuple<Args...>> {
  typedef typename std::tuple<T, Args...> type;
};

template <typename T, typename Collection>
struct append_type {};

template <typename T, typename... Args>
struct append_type<T, std::tuple<Args...>> {
  typedef typename std::tuple<Args..., T> type;
};

#endif
