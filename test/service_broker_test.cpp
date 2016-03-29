#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <communication/ServiceBroker.hpp>

#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <type_traits>

TEST(SanitizeName, Sanitize) {
  ASSERT_EQ("", detail::sanitize_name(".."));
  ASSERT_EQ("a", detail::sanitize_name(".a"));
  ASSERT_EQ("a", detail::sanitize_name(".a."));
  ASSERT_EQ("a.b", detail::sanitize_name(".a.b"));
  ASSERT_EQ("a.b.c", detail::sanitize_name("a.b.c"));
}

TEST(ServiceDirectoryTest, AddService) {
  ServiceDirectory service_directory;
  ASSERT_THROW(service_directory.add_service(""), broker_error);
  service_directory.add_service("a.b");
  ASSERT_THROW(service_directory.add_service(".a.b"), broker_error);
  service_directory.add_service("a.c");
  ASSERT_THROW(service_directory.add_service("a.c"), broker_error);
}

TEST(ServiceDirectoryTest, RemoveService) {
  ServiceDirectory service_directory;
  service_directory.add_service("a.b");
  service_directory.add_service("a.c");
  service_directory.add_service("b.a");
  service_directory.add_service("b.b");
  service_directory.remove_service("a.c");
  ASSERT_NO_THROW(service_directory.add_service("a.c"));

  service_directory.remove_service("a");
  ASSERT_NO_THROW(service_directory.add_service("a.b"));
  ASSERT_NO_THROW(service_directory.add_service("a.c"));
  ASSERT_THROW(service_directory.add_service("b.a"), broker_error);
  ASSERT_THROW(service_directory.add_service("b.b"), broker_error);

  service_directory.remove_service("");
  ASSERT_NO_THROW(service_directory.add_service("a.b"));
  ASSERT_NO_THROW(service_directory.add_service("a.c"));
  ASSERT_NO_THROW(service_directory.add_service("b.a"));
  ASSERT_NO_THROW(service_directory.add_service("b.b"));
}

TEST(ServiceDirectoryTest, ListServices) {
  ServiceDirectory service_directory;
  service_directory.add_service("a.b");
  service_directory.add_service("a.c");
  service_directory.add_service("b.a");
  service_directory.add_service("b.b");

  ASSERT_EQ(4u, service_directory.list_services().size());
  ASSERT_EQ(2u, service_directory.list_services("a").size());
  ASSERT_EQ(2u, service_directory.list_services("b").size());
  service_directory.remove_service("b");
  ASSERT_EQ(2u, service_directory.list_services().size());
}

TEST(ServiceBrokerTest, Constructor) { ServiceBroker broker; }

TEST(ServiceInfoTest, AnyCastExactTest) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  boost::any t = service;
  auto s2 = boost::any_cast<Service<void, std::string>>(t);
}

TEST(ServiceInfoTest, AnyCastCVTest) {
  ServiceBroker broker;
  Service<void, std::string  const &> service = {"test"};
  boost::any t = service;
  auto s2 = boost::any_cast<Service<void, std::string const &>>(t);
}


TEST(ServiceBrokerTest, AddService) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
}

TEST(ServiceBrokerTest, RemoveService) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
  broker.remove_service(service.name);
  broker.add_service(service);
}

TEST(ServiceBrokerTest, RemoveCallback) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
  broker.remove_service("test");
}

TEST(ServiceBrokerTest, GetService) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
  auto s2 = broker.get_service<void, std::string>("test");
}

void test(std::string e) { std::cout << e << std::endl; }

TEST(ServiceBrokerTest, RegisterCallback1) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
  broker.register_callback("test", &test);
}

TEST(ServiceBrokerTest, RegisterCallback2) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
  broker.register_callback("test", test);
}

TEST(ServiceBrokerTest, RegisterLambdaCallback) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);

  auto lambda = [](std::string e) { std::cout << e; };
  broker.register_callback("test", lambda);
}

TEST(ServiceBrokerTest, SignalService) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);
  broker.register_callback("test", &test);
}

TEST(ServiceBrokerTest, CallbackOutOfScope) {
  ServiceBroker broker;
  Service<void, std::string> service = {"test"};
  broker.add_service(service);

  {
    auto lambda = [](std::string e) { ASSERT_EQ("test", e); };
    broker.register_callback("test", lambda);
  }
  service("test");
}

TEST(ServiceBrokerTest, RegisterCallbackToMultipleServices) {
  ServiceBroker broker;

  std::vector<Service<void, std::string>> services;
  for (int i = 0; i < 10; ++i) {
    auto service = Service<void, std::string>{"log.test" + std::to_string(i)};
    broker.add_service(service);
    services.emplace_back(service);
  }

  {
    auto lambda = [](std::string e) { std::cout << e << std::endl; };
    broker.register_callback("log", lambda);
  }

  for (auto &service : services) {
    service("test");
  }
}

TEST(ServiceBrokerTest, VoidCall) {
  ServiceBroker broker;

  int counter = 0;

  for (int i = 0; i < 10; ++i) {
    Service<void> service("config.test" + std::to_string(i));
    service.service->connect([&counter]() { ++counter; });
    broker.add_service(service);
  }

  for (int i = 0; i < 10; ++i) {
    broker.call<void>("config.test" + std::to_string(i));
    ASSERT_EQ(i + 1, counter);
  }

  broker.call<void>("config");
  ASSERT_EQ(20, counter);
}

TEST(ServiceBrokerTest, CallWithReturn) {
  ServiceBroker broker;

  for (int i = 0; i < 10; ++i) {
    Service<std::string> service("config.test" + std::to_string(i));
    service.service->connect([i]() { return std::to_string(i); });
    broker.add_service(service);
  }

  auto combiner = [](std::vector<std::string> const &results) {
    std::stringstream ss;
    for (const auto &e : results) {
      ss << e;
    }
    return ss.str();
  };

  for (int i = 0; i < 10; ++i) {
    ASSERT_EQ(std::to_string(i),
              broker.call_combine<std::string>("config.test" + std::to_string(i),
                                       combiner));
  }

  ASSERT_EQ("0123456789", broker.call_combine<std::string>("config", combiner));
}
