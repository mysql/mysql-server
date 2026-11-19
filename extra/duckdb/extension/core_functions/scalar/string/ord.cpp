#include "core_functions/scalar/string_functions.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "utf8proc.hpp"

#include <string.h>

namespace duckdb {

struct OrdOperator {
	template <class TA, class TR>
	static inline TR Operation(const TA &input) {
		auto str = reinterpret_cast<const utf8proc_uint8_t *>(input.GetData());
		auto len = input.GetSize();
		utf8proc_int32_t codepoint;
		(void)utf8proc_iterate(str, UnsafeNumericCast<utf8proc_ssize_t>(len), &codepoint);
        int width = Utf8Proc::CodepointLength(codepoint);
        if (width == -1) {
            return str[0];
        }
        uint32_t ret = 0;
        for (int i = 0; i < width; i++) {
            ret <<= 8;
            ret += str[i];
        }
		return ret;
	}
};

ScalarFunction OrdFun::GetFunction() {
	return ScalarFunction({LogicalType::VARCHAR}, LogicalType::UINTEGER,
	                      ScalarFunction::UnaryFunction<string_t, uint32_t, OrdOperator>);
}

} // namespace duckdb
