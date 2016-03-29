#ifndef CONCAT_HPP
#define CONCAT_HPP

#pragma once

#include <type_traits>
#include <unordered_map>
#include <queue>
#include <iostream>
#include <mutex>
#include <array>
#include <numeric>

#include <threadpool/ThreadedQueue.hpp>

namespace detail {
template <typename S, typename T, typename... Args>
struct type_occurences {
  static constexpr int value =
      std::is_same<S, T>::value + type_occurences<S, Args...>::value;
};

template <typename S, typename T>
struct type_occurences<S, T> {
  static constexpr int value = std::is_same<S, T>::value;
};

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

template <class T, class... Args>
T &get_element_by_type(std::tuple<Args...> &t) {
  return std::get<detail::get_number_of_element_from_tuple_by_type_impl<
      T, 0, Args...>::value>(t);
}

template <class T, class... Args>
const T &get_element_by_type(std::tuple<Args...> const &t) {
  return std::get<detail::get_number_of_element_from_tuple_by_type_impl<
      T, 0, Args...>::value>(t);
}
}  // namespace detail

// Structure for combining the data from multiple streams. Data from all of the
// streams must be indexable by the Indexer (must be default constructible).
// Each value is assigned to the structure via Assigner (must be default
// constructible). This enables various combinations, for example: combining
// input vector and existing vector inside the storage.
template <class Index, class Assignment, class CompleteCondition,
          class EraseCondition, typename T, typename... Args>
class ConcatImpl {
 public:
  using index_type = decltype(std::declval<Index>()(std::declval<T const &>()));
  using index_function = Index;
  using assign_function = Assignment;
  using complete_function = CompleteCondition;
  using erase_function = EraseCondition;

  using result_type = std::tuple<T, Args...>;

  static constexpr std::size_t narguments = sizeof...(Args) + 1u;

 public:
  // Put new element to the concatenator. If the put element has a unique type,
  // user does not have to specify the element's index.
  template <typename S,
            typename = typename std::enable_if<
                detail::type_occurences<typename std::decay<S>::type, T,
                                        Args...>::value == 1>::type>
  void put(S &&value) {
    static constexpr std::size_t element_idx =
        detail::get_number_of_element_from_tuple_by_type_impl<
            typename std::decay<S>::type, 0, T, Args...>::value;
    put<element_idx>(std::forward<S>(value));
  }

  // Put new element to the concatenator to the specified position in the tuple.
  template <std::size_t element_idx, typename S>
  void put(S &&value) {
    std::lock_guard<std::mutex> lk(mtx_);

    // Calculate value's index
    const auto idx = index_function()(value);

    // Find existing element in the data storage if exists
    auto element_it = data_.find(idx);

    // Construct new element in the storage if no element exists
    if (element_it == data_.end()) {
      auto result = data_.insert(std::make_pair(
          idx, std::make_tuple(std::chrono::high_resolution_clock::now(),
                               std::array<short, narguments>{},
                               std::tuple<T, Args...>())));
      element_it = result.first;
    }

    assert(element_it != data_.end());

    // Add element to the storage. Indirection via Assigner enables different
    // assignment strategies.
    assign_function()(std::get<element_idx>(std::get<2>(element_it->second)),
                      value);

    // Increment assignment count by one, so that we know when all the values
    // for the specific index are present. bitset was replaced by
    // array<short,...> to enable more complex erase conditions.
    ++std::get<1>(element_it->second)[element_idx];

    // If all the elements are present in the concatenated object, add it to the
    // output queue.
    if (complete_function().operator() < T, Args... > (element_it->second)) {
      output_queue_.push_back(std::get<2>(element_it->second));
    }

    // Should we delete the complete or incomplete element from the storage?
    if (erase_function().operator() < T, Args... > (element_it->second)) {
      data_.erase(element_it);
    }
  }

  // Try to get any concatenated objects. Only complete objects will be present
  // here
  bool try_get(std::tuple<T, Args...> &result) {
    return output_queue_.try_pull_front(result) == queue_op_status::success;
  }

  // Get any concatenated objects, or wait until any is present.
  std::tuple<T, Args...> get() { return output_queue_.pull_front(); }

  // Return storage size -- size of incomplete data
  std::size_t size() const { return data_.size(); }

 private:
  // Stored incomplete data. Data is indexed by the indexed type. Besides
  // required data, we also store the time of the first entry for the index and
  // keep track of the number of obtained elements for each index.
  std::unordered_map<
      index_type,
      std::tuple<std::chrono::high_resolution_clock::time_point,
                 std::array<short, narguments>, std::tuple<T, Args...>>> data_;

  // All complete data is pushed to the output_queue
  threaded_queue<std::tuple<T, Args...>> output_queue_;
  std::mutex mtx_;
};

struct DefaultIndex {
  using index_type = int;

  template <typename T>
  index_type operator()(T const &value) {
    return index(value);
  }
};

struct DefaultAssignment {
  template <typename T>
  void operator()(T &dst, T const &src) {
    dst = src;
  }
};

struct DefaultCompleteCondition {
  template <typename T, typename... Args>
  bool operator()(std::tuple<std::chrono::high_resolution_clock::time_point,
                             std::array<short, sizeof...(Args) + 1>,
                             std::tuple<T, Args...>> const &datum) {
    return std::accumulate(std::get<1>(datum).cbegin(),
                           std::get<1>(datum).cend(),
                           0) == sizeof...(Args) + 1u;
  }
};

using DefaultEraseCondition = DefaultCompleteCondition;

template <typename... Args>
using Concat =
    ConcatImpl<DefaultIndex, DefaultAssignment, DefaultCompleteCondition,
               DefaultEraseCondition, Args...>;

#endif
