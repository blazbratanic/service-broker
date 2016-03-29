#ifndef COMBINER_HPP
#define COMBINER_HPP

#pragma once

#include <type_traits>

#include "Service.hpp"
#include "detail/type_constraints.hpp"

// Helper class which connects all services to the combine_data functions in the
// derived class. It should be used, if you require capability of merging the
// input from multiple services. 
// Example: 
// class ServiceA : public WorkerMultiThreaded<...>
// requires capability of combining data from several services before processing.
// Use Combiner as a CRTP:
// class ServiceA : public WorkerMultiThreaded<...>, public Combiner<ServiceA>
// {};
// and implement combine_data functions matching the types of all input services.
template <class Derived> struct Combiner {
  template <typename... Services> Combiner(Services &&... services) {
    connect_combiner(std::forward<Services>(services)...);
  }

  // Connects service's signal to the combine_data function template
  template <typename ResultType, typename... Args>
  void connect_combiner(Service<ResultType, Args...> &service) {
    // TODO: Add to slots
    service.service->connect([this](Args const &... arg) {
      static_cast<Derived *>(this)->combine_data(arg...);
    });
  }

  // Connects service's signal to the combine_data function template
  template <typename InputService, typename... InputServices>
  std::enable_if_t<
      detail::are_valid_services<InputService, InputServices...>::value>
  connect_combiner(InputService &&service, InputServices &&... services) {
    connect_combiner(service);
    connect_combiner(std::forward<InputServices>(services)...);
  }
};

#endif
