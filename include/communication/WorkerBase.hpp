#ifndef WORKER_HPP
#define WORKER_HPP

#include <thread>
#include <mutex>
#include <atomic>

#include <boost/signals2.hpp>

#include <cxml/cxml.hpp>

#include "detail/type_constraints.hpp"
#include "threadpool/PerformanceStatistics.hpp"
#include "ServiceBroker.hpp"
#include "Log.hpp"

#define LOG(severity, message) log({Log::Severity::severity, message});

template <class BrokerType, class ConfigurationType>
class WorkerBaseT {
 public:
  template <typename T>
  using signal_t = boost::signals2::signal<T>;
  using slot_type = boost::signals2::connection;
  using broker_type = BrokerType;
  using configuration_type = ConfigurationType;
  using self_type = WorkerBaseT<BrokerType, ConfigurationType>;

  static_assert(std::is_default_constructible<configuration_type>::value,
                "Configuration type must be default constructible!");

 public:
  WorkerBaseT(std::string const &worker_name,
              std::shared_ptr<BrokerType> broker)
      : worker_name_(worker_name),
        broker_(broker),
        configuration_changed_(false) {
    // Register all services with the broker
    add_service(log, error, on_set_configuration, on_get_configuration);

    // Register callbacks
    register_callback(on_set_configuration.name,
                      [this](configuration_type configuration) {
                        set_configuration(configuration);
                      });
    register_callback(on_get_configuration.name,
                      [this]() { return get_configuration(); });
  }

  // Disconnect all connections and remove all services
  virtual ~WorkerBaseT() {
    for (auto &connection : slots_) {
      connection.disconnect();
    }
    remove_service(log, error, on_set_configuration, on_get_configuration);
  }

  // Update configuration storage and set changed flag to true. Implementation
  // of the derived class should take care to safely update the actual
  // configuration.
  virtual void set_configuration(configuration_type configuration) {
    // Set temporary configuration storage
    std::lock_guard<std::mutex> storage_lock(configuration_storage_mtx_);
    configuration_storage_ = configuration;

    // Check if we can update configuration immediately
    std::unique_lock<std::mutex> configuration_lock(configuration_mtx_,
                                                    std::try_to_lock_t());
    if (configuration_lock.owns_lock()) {
      // Immediately set configuration
      set_configuration_(configuration);
    } else {
      // Defer configuration change
      configuration_changed_ = true;
    }
  };

  // Get configuration. Configuration can probably be returned without much
  // synchronization.
  virtual configuration_type get_configuration() const {
    return get_configuration_();
  };

  virtual PerformanceStatistics performance_statistics() const {
    return PerformanceStatistics();
  }

  // Return workers name
  std::string name() const { return worker_name_; }

 protected:
  // Add services to the broker
  template <typename ServiceT, typename... Args>
  void add_service(ServiceT &&service, Args &&... args) {
    broker_->add_service(service);
    add_service(std::forward<Args>(args)...);
  }
  void add_service() {}

  // Remove services from the broker
  template <typename ServiceT, typename... Args>
  void remove_service(ServiceT &&service, Args &&... args) {
    broker_->remove_service(service.name);
    remove_service(std::forward<Args>(args)...);
  }
  void remove_service() {}

  // Re-implement this function to get the desired functionality
  virtual void set_configuration_(configuration_type const &configuration) {}

  // Re-implement this function to get the desired functionality
  virtual configuration_type get_configuration_() const {
    std::lock_guard<std::mutex> lck(configuration_storage_mtx_);
    return configuration_storage_;
  }

  // Connect callback to the service via service broker
  template <typename Callback>
  void register_callback(std::string const &service_name, Callback &&callback) {
    try {
      auto connections = broker_->register_callback(
          service_name, std::forward<Callback>(callback));
      for (auto &c : connections) {
        slots_.emplace_back(std::move(c));
      }
    } catch (broker_error &e) {
      LOG(Severe, e.what());
    }
  }

  // Connect callback to the services
  template <typename Callback>
  void register_callback(Callback &&) {}
  template <typename Callback, typename InputService, typename... Args>
  std::enable_if_t<detail::has_signal_type<InputService>::value>
  register_callback(Callback &&callback, InputService &input_service,
                    Args &... input_services) {
    slots_.emplace_back(input_service.service->connect(callback));
    register_callback(std::forward<Callback>(callback),
                      std::forward<Args>(input_services)...);
  }

  // Should be called when it is safe to set deferred configuration.
  void update_configuration() {
    std::unique_lock<std::mutex> storage_lock(configuration_storage_mtx_,
                                              std::defer_lock);
    std::unique_lock<std::mutex> configuration_lock(configuration_mtx_,
                                                    std::defer_lock);
    std::lock(storage_lock, configuration_lock);
    set_configuration_(configuration_storage_);
    configuration_changed_ = false;
  }

  // Should be locked while configuration should be unchanged
  mutable std::mutex configuration_mtx_;

  // Configuration storage, for deferring configuration change if it cannot be
  // safely changed. If configuration change was deferred,
  // configuration_changed_ flag is set.
  configuration_type configuration_storage_;
  mutable std::mutex configuration_storage_mtx_;
  std::atomic<bool> configuration_changed_;

  // Service broker
  std::shared_ptr<BrokerType> broker_;
  // All connections
  std::vector<slot_type> slots_;
  // Worker name
  std::string worker_name_ = "worker";
  // Logging signal
  Service<void, Log> log = {"log." + worker_name_};
  // Error signal
  Service<void, std::exception_ptr> error = {"error." + worker_name_};
  // Set configuration slot
  Service<void, configuration_type> on_set_configuration = {
      "configuration.set." + worker_name_};
  // Get configuration slot
  Service<configuration_type> on_get_configuration = {"configuration.get." +
                                                      worker_name_};
};

#endif
