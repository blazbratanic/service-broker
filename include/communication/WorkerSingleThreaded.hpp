#ifndef WORKER_SINGLE_THREADED_HPP
#define WORKER_SINGLE_THREADED_HPP

#include <boost/signals2.hpp>

#include <threadpool/ThreadedQueue.hpp>

#include "detail/type_constraints.hpp"
#include "ServiceBroker.hpp"
#include "WorkerBase.hpp"

template <typename ArgumentType, typename ResultType, typename BrokerType,
          typename ConfigurationType>
class WorkerSingleThreadedT
    : public WorkerBaseT<BrokerType, ConfigurationType> {
 public:
  using Base = WorkerBaseT<BrokerType, ConfigurationType>;
  using argument_type = ArgumentType;
  using result_type = ResultType;
  using broker_type = BrokerType;
  using configuration_type = ConfigurationType;

  static_assert(std::is_default_constructible<result_type>::value,
                "ResultType must be default constructible.");

  Service<void, result_type> result_signal = {this->worker_name_ + ".result"};

 public:
  WorkerSingleThreadedT(std::string const &worker_name,
                        std::shared_ptr<BrokerType> broker)
      : Base(worker_name, broker), terminate_(false) {
    worker_thread_ = std::thread([this]() { run_(); });
    this->add_service(result_signal);
  }

  WorkerSingleThreadedT(std::string const &worker_name,
                        std::shared_ptr<BrokerType> broker,
                        std::vector<std::string> const &inputs)
      : WorkerSingleThreadedT(worker_name, broker) {
    for (const auto &input : inputs) {
      this->register_callback(
          input + ".result",
          [this, &input](argument_type task) { task_queue_.push_back(task); });
    }
  }

  template <typename... InputServices>
  WorkerSingleThreadedT(std::string const &worker_name,
                        std::shared_ptr<BrokerType> broker,
                        InputServices &&... inputs)
      : WorkerSingleThreadedT(worker_name, broker) {
    static_assert(detail::are_valid_services<InputServices...>::value,
                  "Invalid input. Input should be of type Service.");
    this->register_callback(
        [this](argument_type const &task) { task_queue_.push_back(task); },
        std::forward<InputServices>(inputs)...);
  }

  ~WorkerSingleThreadedT() {
    this->remove_service(result_signal);
    terminate_ = true;
    task_queue_.push_back(argument_type());
    worker_thread_.join();
  }

  std::size_t pending() const noexcept { return task_queue_.size(); }

  // Return performance statistics (min, max, avg execution times)
  PerformanceStatistics performance_statistics() const override final { return timings_; }

 protected:
  virtual result_type run(argument_type const &) {
    std::this_thread::yield();
    return result_type();
  }

  void run_() {
    while (!terminate_) {
      try {
        // Update deferred configuration
        if (this->configuration_changed_) {
          this->update_configuration();
        }
        auto task = task_queue_.pull_front();

        // TODO: Implement this with conditional_variable
        if (!terminate_) {
          // Lock changes to configuration
          std::lock_guard<std::mutex> lk(this->configuration_mtx_);

          // Measure execution time
          auto start_time = std::chrono::high_resolution_clock::now();
          // Execute task
          auto result = run(task);
          auto end_time = std::chrono::high_resolution_clock::now();
          timings_.update(end_time - start_time);

          // Signal result to the connected callbacks
          result_signal(result);
        }
      } catch (std::exception &e) {
        this->error(std::make_exception_ptr(e));
      }
    }
  }

 protected:
  threaded_queue<argument_type> task_queue_;

 private:
  // Terminate main service thread which performs pre- and post-processing.
  std::atomic<bool> terminate_;
  // Main service thread which performs pre- and post-processing.
  std::thread worker_thread_;
  // Measure execution time
  PerformanceStatistics timings_;
};
#endif
