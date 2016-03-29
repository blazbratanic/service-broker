#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <communication/Workers.hpp>
#include <communication/ServiceBroker.hpp>

#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <type_traits>

using trivial_configuration_type = std::string;

struct MockBroker : public ServiceBroker {
  MOCK_METHOD1(remove_service, void(std::string const &value));
  MOCK_METHOD1(add_service_log, void(Service<void, Log> service));
  MOCK_METHOD1(add_service_error,
               void(Service<void, std::exception_ptr> service));
  MOCK_METHOD1(add_service_set_configuration,
               void(Service<void, trivial_configuration_type> service));
  MOCK_METHOD1(add_service_get_configuration,
               void(Service<trivial_configuration_type> service));
  MOCK_METHOD1(register_callback_basic, void(std::string const &));

  template <typename Function>
  std::vector<boost::signals2::connection>
  register_callback(std::string const &name, Function &&fn) {
    register_callback_basic(name);
    return {};
  };

  //template <typename ReturnType, typename... Args>
  //void add_service(Service<ReturnType, Args...> service) {
    //static_assert(false, "Add suitable specialization.");
  //};

  void add_service(Service<void, Log> service) {
    add_service_log(service);
  }

  void add_service(Service<void, std::exception_ptr> service) {
    add_service_error(service);
  }
  
  void add_service(Service<void, trivial_configuration_type> service) {
    add_service_set_configuration(service);
  }
  void add_service(Service<trivial_configuration_type> service) {
    add_service_get_configuration(service);
  }
};

// Here we need a real broker to connect the callbacks
struct MockWorker : WorkerBaseT<ServiceBroker, trivial_configuration_type> {
  using Base = WorkerBaseT<ServiceBroker, trivial_configuration_type>;
  using Base::Base;

  MOCK_METHOD1(set_configuration, void(trivial_configuration_type));
  MOCK_CONST_METHOD0(get_configuration, trivial_configuration_type());
};

/// <summary>
/// Check for service iniitialization
/// </summary>
TEST(WorkerTest, CheckRequiredServices) {
  using TestWorker = WorkerBaseT<MockBroker, trivial_configuration_type>;
  auto broker = std::make_shared<MockBroker>();
  EXPECT_CALL(*broker, add_service_log(testing::_)).Times(1);
  EXPECT_CALL(*broker, add_service_error(testing::_)).Times(1);
  EXPECT_CALL(*broker, add_service_get_configuration(testing::_)).Times(1);
  EXPECT_CALL(*broker, add_service_set_configuration(testing::_)).Times(1);
  EXPECT_CALL(*broker, remove_service(testing::_)).Times(4);
  EXPECT_CALL(*broker, register_callback_basic(testing::_)).Times(2);
  { TestWorker worker("worker", broker); }
}

/// <summary>
/// Test basic setting and getting configuration
/// </summary>
TEST(WorkerTest, SetGetConfiguration) {
  auto broker = std::make_shared<ServiceBroker>();
  MockWorker worker("a", broker);

  EXPECT_CALL(worker, set_configuration("Test")).Times(1);
  EXPECT_CALL(worker, get_configuration())
      .Times(1)
      .WillOnce(testing::Return("TestReturn"));

  broker->call<void, trivial_configuration_type>("configuration.set.a", "Test");
  auto configuration =
      broker->call<trivial_configuration_type>("configuration.get.a");

  ASSERT_EQ(1u, configuration.size());
  ASSERT_EQ("TestReturn", configuration.front());
}

/// <summary>
/// Setting and getting configuration of a single threaded worker
/// </summary>
struct MockSingleThreadedWorker
    : public WorkerSingleThreadedT<std::string, std::string, ServiceBroker,
                                   trivial_configuration_type> {
  using Base = WorkerSingleThreadedT<std::string, std::string, ServiceBroker,
                                     trivial_configuration_type>;
  using Base::Base;

  MOCK_METHOD1(run, std::string(std::string const &value));
  MOCK_METHOD1(set_configuration_,
               void(trivial_configuration_type const &value));
  MOCK_CONST_METHOD0(get_configuration_, trivial_configuration_type());
};

TEST(WorkerSingleThreadedTest, SetGetConfiguration) {
  auto broker = std::make_shared<ServiceBroker>();
  MockSingleThreadedWorker worker("a", broker, {});

  EXPECT_CALL(worker, set_configuration_("Test")).Times(2);
  EXPECT_CALL(worker, get_configuration_())
      .Times(1)
      .WillOnce(testing::Return("TestReturn"));

  worker.set_configuration("Test");
  broker->call<void, trivial_configuration_type>("configuration.set.a", "Test");
  auto configuration =
      broker->call<trivial_configuration_type>("configuration.get.a");

  ASSERT_EQ(1u, configuration.size());
  ASSERT_EQ("TestReturn", configuration.front());
}

/// <summary>
///  Setting and getting configuration of a multithreaded worker
/// </summary>
struct MockContext : ContextBaseT<std::string, std::string, std::string> {
  using Base = ContextBaseT<std::string, std::string, std::string>;
  MOCK_METHOD1(run, std::string(std::string const &));
  MOCK_METHOD1(set_configuration_, void(std::string const &));
  MOCK_CONST_METHOD0(get_configuration_, std::string());
};

struct MockMultiThreadedWorker
    : public WorkerMultiThreadedT<std::string, std::string,
                                  ::testing::NiceMock<MockContext>,
                                  ServiceBroker, std::string> {
  using Base = WorkerMultiThreadedT<std::string, std::string,
                                    ::testing::NiceMock<MockContext>,
                                    ServiceBroker, std::string>;
  using Base::Base;
  MOCK_METHOD1(set_configuration_,
               void(trivial_configuration_type const &value));
  MOCK_CONST_METHOD0(get_configuration_, trivial_configuration_type());
  void preprocess(std::string const &a) override { schedule(a); }
  std::string postprocess(std::string&& a) override { return a; }
};

TEST(WorkerMultiThreadedTest, SetGetConfiguration) {
  auto broker = std::make_shared<ServiceBroker>();
  MockMultiThreadedWorker worker("a", broker, {});

  EXPECT_CALL(worker, set_configuration_("Test")).Times(1);
  EXPECT_CALL(worker, get_configuration_())
      .Times(1)
      .WillOnce(testing::Return("TestReturn"));

  worker.set_configuration("Test");
  auto configuration = worker.get_configuration();

  ASSERT_EQ("TestReturn", configuration);
}

/// <summary>
/// Connect two workers statically
/// </summary>
struct MockSingleThreadedWorkerA
    : public WorkerSingleThreadedT<std::string, std::string, ServiceBroker,
                                   trivial_configuration_type> {
  using Base = WorkerSingleThreadedT<std::string, std::string, ServiceBroker,
                                     trivial_configuration_type>;
  using Base::Base;

  MOCK_METHOD1(run, std::string(std::string const &value));
  MOCK_METHOD1(set_configuration_,
               void(trivial_configuration_type const &value));
  MOCK_CONST_METHOD0(get_configuration_, trivial_configuration_type());
};

struct MockSingleThreadedWorkerB
    : public WorkerSingleThreadedT<std::string, std::string, ServiceBroker,
                                   trivial_configuration_type> {
  using Base = WorkerSingleThreadedT<std::string, std::string, ServiceBroker,
                                     trivial_configuration_type>;
  using Base::Base;

  MOCK_METHOD1(run, std::string(int value));
  MOCK_METHOD1(set_configuration_,
               void(trivial_configuration_type const &value));
  MOCK_CONST_METHOD0(get_configuration_, trivial_configuration_type());
};

TEST(Workers, CompileTimeConnectionCheck) {
  auto broker = std::make_shared<ServiceBroker>();
  MockSingleThreadedWorkerA a("a", broker, {});
  MockSingleThreadedWorkerB b("b", broker, a.result_signal);
}
