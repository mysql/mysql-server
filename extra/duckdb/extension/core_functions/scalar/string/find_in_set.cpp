#include "core_functions/scalar/string_functions.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/scalar/string_common.hpp"
#include "utf8proc.hpp"

namespace duckdb {

struct FindInSetOperator {
	template <class TA, class TB, class TR>
	static inline TR Operation(TA str, TB strlist) {
		int64_t string_position = 0;

		auto strlist_data = const_uchar_ptr_cast(strlist.GetData());
		auto strlist_size = strlist.GetSize();

		if (strlist_size == 0) {
			return 0;
		}

		unsigned char sep = ',';
		idx_t idx = 1;
		while (strlist_size >= 0) {
			auto pos = FindStrInStr(strlist_data, strlist_size, &sep, 1);
			if (pos > strlist_size) {
				auto tmpstr = string_t(const_char_ptr_cast(strlist_data), strlist_size);
				if (tmpstr == str) {
					return idx;
				}
				break;
			}
			auto tmpstr = string_t(const_char_ptr_cast(strlist_data), pos);
			if (tmpstr == str) {
				return idx;
			}
			strlist_data += (pos + 1);
			strlist_size -= (pos + 1);
			idx += 1;
		}
		return 0;
	}
};

ScalarFunction FindInSetFun::GetFunction() {
	return ScalarFunction({LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::BIGINT,
	                      ScalarFunction::BinaryFunction<string_t, string_t, int64_t, FindInSetOperator>);
}

} // namespace duckdb
