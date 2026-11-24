//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/enums/set_scope.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"

namespace duckdb {

    enum class SqlModeType : uint8_t {
        MODE_REAL_AS_FLOAT = 0,
        MODE_PIPES_AS_CONCAT,
        MODE_ANSI_QUOTES,
        MODE_IGNORE_SPACE,
        MODE_NOT_USED,
        MODE_ONLY_FULL_GROUP_BY,
        MODE_NO_UNSIGNED_SUBTRACTION,
        MODE_NO_DIR_IN_CREATE,
        MODE_ANSI,
        MODE_NO_AUTO_VALUE_ON_ZERO,
        MODE_NO_BACKSLASH_ESCAPES,
        MODE_STRICT_TRANS_TABLES,
        MODE_STRICT_ALL_TABLES,
        MODE_NO_ZERO_IN_DATE,
        MODE_NO_ZERO_DATE,
        MODE_INVALID_DATES,
        MODE_ERROR_FOR_DIVISION_BY_ZERO,
        MODE_TRADITIONAL,
        MODE_HIGH_NOT_PRECEDENCE,
        MODE_NO_ENGINE_SUBSTITUTION,
        MODE_PAD_CHAR_TO_FULL_LENGTH,
        MODE_TIME_TRUNCATE_FRACTIONAL,
        MODE_LAST
    };

}