#ifndef SERVICE_BROKER_HPP
#define SERVICE_BROKER_HPP

#include <boost/signals2.hpp>
#include <boost/any.hpp>
#include <boost/property_tree/ptree.hpp>

#include <unordered_map>
#include <stdexcept>
#include <tuple>
#include <stack>

#include "Service.hpp"
#include "detail/traits.hpp"

/** A broker error. */
class broker_error : public std::exception {
 public:
  broker_error(const char *err) : e(err) {}
  broker_error(std::string const &err) : e(err) {}

  const char *what() { return e.c_str(); }

 private:
  std::string e;
};

namespace detail {
// Removes leading and trailing and duplicate dots.
static inline std::string sanitize_name(std::string name) {
  if (name.empty()) {
    return name;
  }
  auto it = name.begin();
  auto dst_it = it;
  bool found = true;
  for (; it != name.end(); ++it) {
    if (!found || *it != '.') {
      *dst_it++ = *it;
      found = false;
    } else {
      found = true;
    }
  }
  if (dst_it != name.begin() && *std::prev(dst_it) == '.') {
    --dst_it;
  }
  name.erase(dst_it, name.end());
  return name;
}
}

// Contains a directory of all services. ServiceDirectory does not allow
// multiple services with the same name or service with the same name as the
// group.
class ServiceDirectory {
 public:
  // Add a service to the group. Service name should be
  // "group1.subgroup1.subgroup2.service_name". Any missing groups are created
  // automatically. To add a service with the same name as the group, the
  // group must be deleted first.
  void add_service(std::string const &raw_name) {
    auto name = detail::sanitize_name(raw_name);
    if (name.empty()) {
      throw broker_error("Service must have a name");
    }
    if (auto node_t = node_type(name)) {
      if (node_t == NodeType::Group) {
        throw broker_error("Group with this name already exists.");
      }
      throw broker_error("Service with this name already exists.");
    }

    // Since the property_tree automatically generates missing nodes with empty
    // property, we mark service nodes with "service" string
    tree.put(name, "service");
  }

  // Removes a service or a whole group with the given path. Empty groups are
  // not removed automatically. To clean up the directory and remove empty
  // groups, call "prune".
  void remove_service(std::string const &raw_name) {
    if (auto node = tree.get_child_optional(detail::sanitize_name(raw_name))) {
      node->clear();
      // node does not store its parent, therefore it is not possible to remove
      // it from the tree. To delete a node we simply clear its contents and
      // mark it deleted.
      node->put_value("deleted");
    }
  }

  // Recursively list all services in the group. Does not include groups.
  std::vector<std::string> list_services(
      std::string const &raw_name = "") const {
    std::vector<std::string> result;
    auto name = detail::sanitize_name(raw_name);

    auto node = tree.get_child_optional(name);
    // No node with this name exists
    if (!node) {
      return result;
    }
    // Selected node is a service
    if (node->data() == "service") {
      result.emplace_back(name);
      return result;
    }

    // Selected node is a group
    using iterator_t = boost::property_tree::ptree::const_assoc_iterator;
    std::stack<std::pair<iterator_t, iterator_t>> iterator_stack;
    std::stack<std::string> name_stack;
    // Correctly handle root nodes
    name_stack.push(name == "" ? "" : name + ".");

    // Walk through the tree. This implementation uses std::stack to store
    // iterators. Alternative implementation would be possible with recursion.
    auto it = node->ordered_begin();
    auto it_end = node->not_found();
    while (true) {
      while (it != it_end) {
        if (it->second.data() == "service") {
          result.emplace_back(name_stack.top() + it->first);
          ++it;
        } else {
          iterator_stack.emplace(it, it_end);
          name_stack.push(name_stack.top() + it->first + ".");
          it_end = it->second.not_found();
          it = it->second.ordered_begin();
        }
      }
      if (!iterator_stack.empty()) {
        it = std::next(iterator_stack.top().first);
        it_end = iterator_stack.top().second;
        iterator_stack.pop();
        name_stack.pop();
      } else {
        break;
      }
    }
    return result;
  }

  enum class NodeType { Group, Service };

  // Return type of the given node in the directory (Group or Service). Returns
  // nullopt if no node exists.
  boost::optional<NodeType> node_type(std::string const &raw_name) const {
    auto node = tree.get_optional<std::string>(detail::sanitize_name(raw_name));

    if (node && *node != "deleted") {
      return *node == "" ? boost::make_optional(NodeType::Group)
                         : boost::make_optional(NodeType::Service);
    }
    return boost::none;
  }

  // Remove all elements from the directory
  void clear() { tree.clear(); }

 private:
  boost::property_tree::ptree tree;
};

/** A service broker. Service broker brokers services amongst different
 * actors/workers/modules. */
class ServiceBroker {
 public:
  /**
   Add a service.

   @exception broker_error Thrown when a service with the same name
   already exists.

   @tparam Result Type of the result.
   @tparam Args   Type of service arguments.
   @param service
   */
  template <typename Result, typename... Args>
  void add_service(Service<Result, Args...> service) {
    service_directory_.add_service(service.name);
    services_[service.name] = service;
  }

  /**
   Remove a service.

   @exception broker_error Thrown when a service does not exist.

   @param name  Service name.
   @param group Service group.
   */
  std::size_t remove_service(std::string const &name) {
    auto service_names = service_directory_.list_services(name);
    for (const auto &name : service_names) {
      auto it = services_.find(name);
      assert(it != services_.end());
      services_.erase(it);
    }
    service_directory_.remove_service(name);
    return service_names.size();
  }

  /**
   Register callback to all services in a group.

   @tparam Function Type of the callback.
   @param group             Service group.
   @param [in,out] callback The callback.

   @return A std::vector&lt;boost::signals2::connection&gt;
   */
  template <typename Function>
  std::vector<boost::signals2::connection> register_callback(
      std::string const &name, Function &&callback) {
    std::vector<boost::signals2::connection> connections;

    auto service_names = service_directory_.list_services(name);
    if (service_names.empty()) {
      throw broker_error(
          "Cannot register callback. No service or group with "
          "this name exists.");
    }
    for (const auto &name : service_names) {
      connections.emplace_back(
          register_callback_(name, std::forward<Function>(callback)));
    }
    return connections;
  }

  /// @brief Call all services inside the directory 'name'.
  // Throws broker_error if name matches no service, or if the input
  // arguments are of invalid type. Function returns an array of results. If
  // you want to combine the array of results to a single result, use
  // call_combine and provide a Combiner.
  ///
  /// @tparam ResultType
  /// @tparam Args
  /// @param name Service name
  /// @param args Function call arguments
  ///
  /// @return An array of results.
  template <typename ResultType, typename... Args>
  typename std::enable_if_t<!std::is_void<ResultType>::value,
                            std::vector<ResultType>>
  call(std::string const &name, Args &&... args) {
    auto service_names = service_directory_.list_services(name);
    if (service_names.empty()) {
      throw broker_error("No service or group with this name exists.");
    }
    std::vector<ResultType> result;
    for (const auto &name : service_names) {
      result.emplace_back(
          call_<ResultType, Args...>(name, std::forward<Args>(args)...));
    }
    return result;
  }

  /// @brief Call all services inside the directory 'name'.
  // Throws broker_error if name matches no service, or if the input
  // arguments are of invalid type. Before returning, all results are combined
  // with the given Combiner.
  ///
  /// @tparam ResultType
  /// @tparam Args
  /// @param name Service name
  /// @param args Function call arguments
  ///
  /// @return A combined result.
  template <typename ResultType, class Combiner, typename... Args>
  typename std::enable_if_t<!std::is_void<ResultType>::value, ResultType>
  call_combine(std::string const &name, Combiner &&combiner, Args &&... args) {
    return combiner(call<ResultType>(name, std::forward<Args>(args)...));
  }

  /// @brief Call all services inside the directory 'name'.
  // Throws broker_error if name matches no service, or if the input
  // arguments are of invalid type. This is a specialization for services with
  // void return type.
  ///
  /// @tparam ResultType
  /// @tparam Args
  /// @param name Service name
  /// @param args Function call arguments
  template <typename ResultType, typename... Args>
  typename std::enable_if_t<std::is_void<ResultType>::value> call(
      std::string const &name, Args &&... args) {
    auto service_names = service_directory_.list_services(name);
    if (service_names.empty()) {
      throw broker_error("No service or group with this name exists.");
    }
    for (const auto &name : service_names) {
      call_<ResultType, Args...>(name, std::forward<Args>(args)...);
    }
  }

  /**
   Get a service.

   @exception broker_error Thrown when a service with required name does not
   exist.

   @tparam Result Type of the result.
   @tparam Args   Type of the arguments.
   @param name  Service name.

   @return The service.
   */
  template <typename Result, typename... Args>
  Service<Result, Args...> get_service(std::string const &name) {
    auto it = services_.find(name);
    if (it == services_.end()) {
      throw broker_error("Service does not exist.");
    }

    try {
      return boost::any_cast<Service<Result, Args...>>(it->second);
    } catch (boost::bad_any_cast &) {
      throw broker_error("Type mismatch.");
    }
  }

  std::vector<std::string> list_services(std::string const &name = "") const {
    return service_directory_.list_services(name);
  }

  /**
   Clear all services
   */
  void clear() {
    service_directory_.clear();
    services_.clear();
  }

 private:
  /**
   Register callback.

   @exception broker_error Thrown when a service with the name and group does
   not exist or when service and callback types do not match.

   @tparam Function Type of the Callback.
   @param name              Service name.
   @param group             Service group.
   @param [in,out] callback The callback.

   @return A boost::signals2::connection.
   */
  template <typename Function>
  boost::signals2::connection register_callback_(std::string const &name,
                                                 Function &&callback) {
    auto it = services_.find(name);
    assert(it != services_.end());

    using callback_traits = typename utils::function_traits<decltype(callback)>;

    try {
      return connect(it->second, std::forward<Function>(callback),
                     typename gens<callback_traits::arity>::type());
    } catch (boost::bad_any_cast &) {
      std::string error =
          "Cannot register callback to service: " + name + ". Type mismatch.";
      throw broker_error(error.c_str());
    }
  }

  // Specialization for void return type returning nothing
  template <typename ResultType, typename... Args>
  typename std::enable_if_t<std::is_void<ResultType>::value> call_(
      std::string const &name, Args &&... args) {
    auto it = services_.find(name);
    assert(it != services_.end());

    try {
      boost::any_cast<Service<ResultType, Args...>>(it->second)(
          std::forward<Args>(args)...);
    } catch (boost::bad_any_cast &) {
      std::string error = "Cannot call: " + name + ". Type mismatch.";
      throw broker_error(error.c_str());
    }
  }

  // Specialization for all calls that are not void returning ReturnType
  template <typename ResultType, typename... Args>
  typename std::enable_if_t<!std::is_void<ResultType>::value, ResultType> call_(
      std::string const &name, Args &&... args) {
    auto it = services_.find(name);
    assert(it != services_.end());

    try {
      return boost::any_cast<Service<ResultType, Args...>>(it->second)(
          std::forward<Args>(args)...);
    } catch (boost::bad_any_cast &) {
      std::string error = "Cannot call: " + name + ". Type mismatch.";
      throw broker_error(error.c_str());
    }
  }

  // Several helper function to generate integer sequence for parameter
  // unpacking
  template <int...>
  struct seq {};
  template <int N, int... S>
  struct gens : gens<N - 1, N - 1, S...> {};
  template <int... S>
  struct gens<0, S...> {
    typedef seq<S...> type;
  };

  /** Connect service to callback.  */
  template <typename Function, int... S>
  boost::signals2::connection connect(boost::any &service, Function &&callback,
                                      seq<S...>) {
    using callback_traits = typename utils::function_traits<decltype(callback)>;

    return boost::any_cast<Service<typename callback_traits::result_type,
                                   typename callback_traits::template arg<S>::type...>>(
               service)
        .service->connect(callback);
  }

  ServiceDirectory service_directory_;

  // name -> Service stored in boost::any.
  std::unordered_map<std::string, boost::any> services_;
};

ServiceBroker &get_broker();
#endif
