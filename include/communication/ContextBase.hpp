#ifndef CONTEXT_BASE_HPP
#define CONTEXT_BASE_HPP

#include <mutex>
#include <atomic>

template <typename ArgumentType, typename ResultType,
          typename ConfigurationType>
class ContextBaseT {
public:
  using argument_type = ArgumentType;
  using result_type = ResultType;
  using configuration_type = ConfigurationType;

public:
  ContextBaseT() : configuration_changed_(false) {}

  result_type operator()(argument_type const &arg) {
    if (configuration_changed_) {
      std::unique_lock<std::mutex> storage_lock(configuration_storage_mtx_,
                                                std::defer_lock);
      std::unique_lock<std::mutex> configuration_lock(configuration_mtx_,
                                                      std::defer_lock);
      std::lock(storage_lock, configuration_lock);
      set_configuration_(configuration_storage_);
      configuration_changed_ = false;
    }

    std::lock_guard<std::mutex> configuration_lock(configuration_mtx_);
    return run(arg);
  }

  virtual result_type run(argument_type const &) = 0;

  void set_configuration(configuration_type const &configuration) {
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
  }

  configuration_type get_configuration() const { return get_configuration_(); }

protected:
  virtual void set_configuration_(configuration_type const &configuration) {}
  virtual configuration_type get_configuration_() const {
    return configuration_storage_;
  }

private:
  std::mutex configuration_mtx_;
  std::mutex configuration_storage_mtx_;
  std::atomic<bool> configuration_changed_;
  configuration_type configuration_storage_;
};
#endif
