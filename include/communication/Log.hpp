#ifndef LOG_HPP
#define LOG_HPP

#include <string>

struct Log {
  enum class Severity { Debug, Info, Warning, Error, Severe };
  Severity severity;
  std::string message;
};


#endif
