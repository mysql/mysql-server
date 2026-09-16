#include "c4/language.hpp"

namespace c4 {
namespace detail {

#ifndef __GNUC__
void use_char_pointer(char const volatile* v)
{
    C4_UNUSED(v);
}
#else
// to avoid empty file warning from the linker
C4_MAYBE_UNUSED void foo() {} // NOLINT(misc-use-internal-linkage)
#endif

} // namespace detail
} // namespace c4
