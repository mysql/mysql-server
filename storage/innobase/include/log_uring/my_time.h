
#include <chrono>
#include <boost/date_time.hpp>

inline double_t to_microseconds(std::chrono::nanoseconds ns) {
  return double_t(ns.count()/1000.0);
}

inline double_t to_milliseconds(std::chrono::nanoseconds ns) {
  return double_t(ns.count()/1000000.0);
}

inline double_t to_seconds(std::chrono::nanoseconds ns) {
  return double_t(ns.count()/1000000000.0);
}

