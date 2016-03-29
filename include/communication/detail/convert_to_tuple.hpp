#ifndef CONVERT_TO_TUPLE_HPP
#define CONVERT_TO_TUPLE_HPP

#include <tuple>

template <typename... Args>
struct convert_to_tuple {
  // base case, nothing special,
  // just use the arguments directly
  // however they need to be used
  typedef std::tuple<Args...> type;
};

#endif
