#ifndef COMMUNICATION_TYPE_CONSTRAINTS_HPP
#define COMMUNICATION_TYPE_CONSTRAINTS_HPP

#pragma once

namespace detail {
template <typename T> struct has_signal_type {
private:
  typedef char one;
  typedef struct { char arr[2]; } two;
  template <typename U> struct wrap {};
  template <typename U> static one test(wrap<typename U::signal_type> *);
  template <typename U> static two test(...);

public:
  static const bool value = sizeof(test<T>(0)) == 1;
};

template <typename T, typename... Args> struct are_valid_services {
  static constexpr bool value =
      are_valid_services<T>::value && are_valid_services<Args...>::value;
};

template <typename T> struct are_valid_services<T> {
  static constexpr bool value = has_signal_type<std::decay_t<T>>::value;
};
}

#endif