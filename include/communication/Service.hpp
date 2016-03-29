#ifndef SERVICE_HPP
#define SERVICE_HPP

#pragma once

#include <string>
#include <tuple>
#include <memory>

#include <boost/signals2.hpp>

namespace detail {
// Provides implementation for the services with void and not void return
// type. Combines necessary information about each service, such as service
// name and group and which signal will be triggered.
template <typename ResultType, typename... Args> struct ServiceImpl {
  using argument_type = std::tuple<Args...>;
  using result_type = ResultType;
  using signal_type = boost::signals2::signal<result_type(Args...)>;

  /** Construct new service.  */
  ServiceImpl(std::string const &name) : name(name), service(new signal_type) {}

  // Service name
  std::string name;
  // Signal that will be triggered
  std::shared_ptr<signal_type> service;
};
}

/**
/// Implementation for services with return type
*/
template <typename ResultType, typename... Args>
struct Service : detail::ServiceImpl<ResultType, Args...> {
  using Base = detail::ServiceImpl<ResultType, Args...>;
  // Implementation for signals with return type
  Service(std::string const &name) : Base(name) {}
  typename Base::result_type operator()(Args const &... args) {
    return this->service->operator()(args...).value();
  }
};

/**
/// Implementation for services with void return type
*/
template <typename... Args>
struct Service<void, Args...> : detail::ServiceImpl<void, Args...> {
  using Base = detail::ServiceImpl<void, Args...>;
  Service(std::string const &name) : Base(name) {}
  typename Base::result_type operator()(Args const &... args) {
    this->service->operator()(args...);
  }
};

#endif
