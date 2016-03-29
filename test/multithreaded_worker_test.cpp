#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <communication/Workers.hpp>
#include <communication/ServiceBroker.hpp>

#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <type_traits>
#include <string>

class DataProvider : public WorkerBase {
  using result_type = std::string;

public:
  DataProvider(std::string const &name, std::shared_ptr<ServiceBroker> broker)
      : WorkerBase(name, broker) {
    add_service(result_signal);
  }

  void run() {
    static int i = 1;
    for (int i = 0; i < 100; ++i) {
      result_signal(std::to_string(i));
    }
  }

  void start() {
    working_thread_ = std::thread([this]() { run(); });
  }

  std::thread working_thread_;
  Service<void, result_type> result_signal = {worker_name_ + ".result"};
};

class TestContext : public ContextBase<std::string, std::string> {
  std::string run(std::string const &arg) {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
  }
};

class TrivialMultithreaded
    : public WorkerMultiThreaded<std::string, std::string, TestContext> {
  using Base = WorkerMultiThreaded<std::string, std::string, TestContext>;
  using Base::Base;
protected:

  void preprocess(std::string const &arg) override { schedule(arg); }
  Base::result_type postprocess(context_pool_type::result_type &&arg) override { return arg; }
};

TEST(MultithreadedWorkerTest, Process) {
  using TestWorker = TrivialMultithreaded;

  auto broker = std::make_shared<ServiceBroker>();

  DataProvider provider("provider", broker);
  TestWorker multithreaded_worker("worker", broker, {"provider"});
  std::vector<std::string> result;
  std::mutex mtx;
  broker->register_callback("worker.result", [&result, &mtx](std::string s) {
    std::lock_guard<std::mutex> lk(mtx);
    result.emplace_back(s);
  });
  provider.start();

  provider.working_thread_.join();
  while (result.size() != 100u) {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }

  ASSERT_EQ(100u, result.size());
}
