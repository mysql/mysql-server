#include "c4/version.hpp"

namespace c4 {

const char* version()
{
  return C4CORE_VERSION;
}

int version_major()
{
  return C4CORE_VERSION_MAJOR;
}

int version_minor()
{
  return C4CORE_VERSION_MINOR;
}

int version_patch()
{
  return C4CORE_VERSION_PATCH;
}

} // namespace c4
