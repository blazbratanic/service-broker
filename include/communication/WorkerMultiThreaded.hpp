#ifndef WORKER_MULTI_THREADED_HPP
#define WORKER_MULTI_THREADED_HPP

#include <boost/signals2.hpp>
#include <threadpool/ThreadedQueue.hpp>
#include <threadpool/ContextPool.hpp>
#include <threadpool/ExecutorPool.hpp>

#include "detail/type_constraints.hpp"

#include "WorkerBase.hpp"
#include "ServiceBroker.hpp"

/// <summary>
/// WorkerMultiThreaded provides execution platform that allows for
/// multithreaded execution of the tasks, where each task has its own execution
/// context.
/// </summary>
template <typename ArgumentType, typename ResultType, typename ContextType,
          typename BrokerType, typename ConfigurationType>
class WorkerMultiThreadedT : public WorkerBaseT<BrokerType, ConfigurationType> {
 public:
  using Base = WorkerBaseT<BrokerType, ConfigurationType>;
  using argument_type = ArgumentType;
  using result_type = ResultType;
  using context_type = ContextType;
  using context_pool_type = context_pool<ContextType>;
  using broker_type = BrokerType;
  using configuration_type = ConfigurationType;

  static_assert(std::is_default_constructible<result_type>::value,
                "ResultType must be default constructible.");

  // Signal result
  Service<void, result_type> result_signal = {this->worker_name_ + ".result"};

 public:
  // Default constructor
  WorkerMultiThreadedT(std::string const &worker_name,
                       std::shared_ptr<BrokerType> broker)
      : Base(worker_name, broker),
        context_pool_(new context_pool_type()),
        executors_(4, context_pool_),
        terminate_(false) {
    // Add service result to the broker
    this->add_service(result_signal);
    // Start thread that performs preprocessing and post-processing
    worker_thread_ = std::thread([this]() { run_(); });
  }

  // Inherit default constructor and connect input to all signals in the input
  WorkerMultiThreadedT(std::string const &worker_name,
                       std::shared_ptr<BrokerType> broker,
                       std::vector<std::string> const &inputs)
      : WorkerMultiThreadedT(worker_name, broker) {
    // Connect to all inputs
    for (const auto &input : inputs) {
      this->register_callback(
          input + ".result",
          [this, &input](argument_type task) { task_queue_.push_back(task); });
    }
  }

  // Inherit default constructor and connect all inputs to the task_queue_.
  template <typename... InputServices>
  WorkerMultiThreadedT(std::string const &worker_name,
                       std::shared_ptr<BrokerType> broker,
                       InputServices &... inputs)
      : WorkerMultiThreadedT(worker_name, broker) {
    static_assert(detail::are_valid_services<InputServices...>::value,
                  "Invalid input. Input should be of type Service.");
    register_callback(
        [this](argument_type const &task) { task_queue_.push_back(task); },
        std::forward<InputServices>(inputs)...);
  }

  ~WorkerMultiThreadedT() {
    this->remove_service(result_signal);
    terminate_ = true;
    worker_thread_.join();
  }

  // Set configuration for all contexts
  void set_configuration(configuration_type configuration) override final {
    for (auto context : context_pool_->contexts()) {
      context->set_configuration(configuration);
    }
    Base::set_configuration(configuration);
  };

  // Return number of pending tasks
  std::size_t pending() const noexcept { return executors_.pending(); }
  // Return performance statistics (min, max, avg execution times)
  PerformanceStatistics performance_statistics() const override final {
    return executors_.performance_statistics();
  }

 protected:
  // Prepare the data and schedule it to the executor pool
  virtual void preprocess(argument_type const &arg) = 0;

  // Retrieve the data from
  //  the executor pool and perform post-processing
  virtual result_type postprocess(
      typename context_pool_type::result_type &&arg) = 0;

  // Schedule task for execution
  void schedule(typename context_pool_type::argument_type const &task) {
    executors_.schedule_task(task);
  }

  // Pull data from the input queue, pre- and post-process it. All
  // tasks must be scheduled for execution in the pre-processing step. This
  // decision was made to enable user to split larger input tasks into many
  // smaller tasks.
  void run_() {
    while (!terminate_) {
      try {
        // Update deferred configuration
        if (this->configuration_changed_) {
          // It is safe to change configuration when pre- and post-processing is
          // not running.
          this->update_configuration();
        }

        // Preprocess and schedule tasks
        argument_type task;
        if (task_queue_.try_pull_front(task) == queue_op_status::success) {
          std::lock_guard<std::mutex> lk(this->configuration_mtx_);
          preprocess(task);
        }
        // Post-process tasks and signal result
        std::future<typename context_pool_type::result_type> result;
        if (executors_.result_queue.try_pull_front(result) ==
            queue_op_status::success) {
          std::lock_guard<std::mutex> lk(this->configuration_mtx_);
          result_signal(postprocess(result.get()));
        }
        // Maybe we should execute this in two separate threads with conditional
        // variable for waking up?
        std::this_thread::sleep_for(std::chrono::microseconds(50));
      } catch (...) {
        this->error(std::current_exception());
      }
    }
  }

 protected:
  // Module's task queue.
  threaded_queue<argument_type> task_queue_;

 private:
  // Context pool provides context for executions, i.e. provides function object
  // classes.
  std::shared_ptr<context_pool_type> context_pool_;

  // Executor pool executes tasks in contexts
  executor_pool<context_pool_type> executors_;

  // Terminate thread that performs pre- and post-processing
  std::atomic<bool> terminate_;
  // Thread that performs pre and post-processing in the module
  std::thread worker_thread_;
};

#endif
