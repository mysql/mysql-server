#ifndef C4_VERSION_HPP_
#define C4_VERSION_HPP_

/** @file version.hpp */

#define C4CORE_VERSION "0.6.0"
#define C4CORE_VERSION_MAJOR 0
#define C4CORE_VERSION_MINOR 6
#define C4CORE_VERSION_PATCH 0

#include <c4/export.hpp>

namespace c4 {

C4CORE_EXPORT const char* version();
C4CORE_EXPORT int version_major();
C4CORE_EXPORT int version_minor();
C4CORE_EXPORT int version_patch();

} // namespace c4

#endif /* C4_VERSION_HPP_ */
