#ifndef GET_ELEMEBT_BY_TYPE_HPP
#define GET_ELEMEBT_BY_TYPE_HPP
namespace detail {

template <class T, std::size_t N, class... Args>
struct get_number_of_element_from_tuple_by_type_impl {
  static constexpr auto value = N;
};

template <class T, std::size_t N, class... Args>
struct get_number_of_element_from_tuple_by_type_impl<T, N, T, Args...> {
  static constexpr auto value = N;
};

template <class T, std::size_t N, class U, class... Args>
struct get_number_of_element_from_tuple_by_type_impl<T, N, U, Args...> {
  static constexpr auto value =
      get_number_of_element_from_tuple_by_type_impl<T, N + 1, Args...>::value;
};
}  // namespace detail

template <class T, class... Args>
T get_element_by_type(const std::tuple<Args...>& t) {
  return std::get<detail::get_number_of_element_from_tuple_by_type_impl<
      T, static_cast<size_t>(0), Args...>::value>(t);
}

template <class T, class Collection>
struct get_element_index {};

template <class T, typename... Args>
struct get_element_index<T, std::tuple<Args...>> {
  static constexpr auto value =
      detail::get_number_of_element_from_tuple_by_type_impl<T, 0,
                                                            Args...>::value;
};

template <class T, class... Args>
constexpr int find_element_by_type(const std::tuple<Args...>& t) {
  return detail::get_number_of_element_from_tuple_by_type_impl<
      T, static_cast<size_t>(0), Args...>::value;
}

#endif

