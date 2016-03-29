#include "ServiceBroker.hpp"

#ifdef MSVC
__declspec(dllexport) void __cdecl placeholder() {}
#else 
void placeholder() {}
#endif

ServiceBroker& get_broker() {
  static ServiceBroker broker;
  return broker;
}
