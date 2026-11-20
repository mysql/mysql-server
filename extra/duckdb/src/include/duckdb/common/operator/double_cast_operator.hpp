//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/operator/double_cast_operator.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.h"
#include "fast_float/fast_float.h"
#include "duckdb/common/string_util.hpp"

namespace duckdb {
template <class T>
static bool TryDoubleCast(const char *buf, idx_t len, T &result, bool strict, char decimal_separator = '.') {
	// skip any spaces at the start
	bool has_sign = false;
	while (len > 0 && StringUtil::CharacterIsSpace(*buf)) {
		buf++;
		len--;
	}
	if (len == 0) {
		if (!strict) {
			result = 0;
			return true;
		}
		return false;
	}
	if (*buf == '+') {
		has_sign = true;
		if (strict) {
			// plus is not allowed in strict mode
			return false;
		}
		buf++;
		len--;
	}
	// In MySQL, if there are extra characters at the begin, it will return 0
	if (!strict) {
		// If there is already a positive sign, the next character
		// must be digit, otherwise it returns 0.
		if (has_sign && !StringUtil::CharacterIsDigit(buf[0])) {
			result = 0;
			return true;
		}
		if (!has_sign && (!StringUtil::CharacterIsDigit(buf[0]) && *buf != '-')) {
			result = 0;
			return true;
		}
	}
	if (strict && len >= 2) {
		if (buf[0] == '0' && StringUtil::CharacterIsDigit(buf[1])) {
			// leading zeros are not allowed in strict mode
			return false;
		}
	}
	auto endptr = buf + len;
	auto parse_result = duckdb_fast_float::from_chars(buf, buf + len, result, strict, decimal_separator);
	if (parse_result.ec != std::errc()) {
		return false;
	}
	auto current_end = parse_result.ptr;
	if (!strict) {
		// In MySQL, extra characters at the end are ignored
		return true;
	}
	return current_end == endptr;
}
} // namespace duckdb
