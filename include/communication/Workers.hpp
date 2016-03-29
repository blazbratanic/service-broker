#ifndef WORKERS_HPP
#define WORKERS_HPP

#include <cxml/cxml.hpp>

#include "ServiceBroker.hpp"
#include "WorkerBase.hpp"
#include "WorkerSingleThreaded.hpp"
#include "WorkerMultiThreaded.hpp"
#include "ContextBase.hpp"

using configuration_type = cxml::CXml;

using WorkerBase = WorkerBaseT<ServiceBroker, configuration_type>;

template <typename ArgumentType, typename ResultType>
using WorkerSingleThreaded =
    WorkerSingleThreadedT<ArgumentType, ResultType, ServiceBroker, configuration_type>;

template <typename ArgumentType, typename ResultType, typename ContextType>
using WorkerMultiThreaded =
    WorkerMultiThreadedT<ArgumentType, ResultType, ContextType, ServiceBroker,
                         configuration_type>;

template <typename ArgumentType, typename ResultType>
using ContextBase = ContextBaseT<ArgumentType, ResultType, configuration_type>;

#endif
