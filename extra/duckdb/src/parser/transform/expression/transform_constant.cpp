#include "duckdb/common/enum_util.hpp"
#include "duckdb/common/limits.hpp"
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/common/types/decimal.hpp"
#include "duckdb/function/scalar/string_common.hpp"
#include "duckdb/parser/expression/cast_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/transformer.hpp"

namespace duckdb {

unique_ptr<ConstantExpression> Transformer::TransformValue(duckdb_libpgquery::PGValue val) {
	switch (val.type) {
	case duckdb_libpgquery::T_PGInteger:
		D_ASSERT(val.val.ival <= NumericLimits<int32_t>::Maximum());
		return make_uniq<ConstantExpression>(Value::INTEGER((int32_t)val.val.ival));
	case duckdb_libpgquery::T_PGBitString: {
		D_ASSERT(val.val.str[0] == 'x' || val.val.str[0] == 'b');
		if (val.val.str[0] == 'x') {
			auto unhex_input = make_uniq<ConstantExpression>(Value(string(val.val.str + 1)));
			string unhex_string;
			char *c = val.val.str + 1;
			while (*c) {
				if (!(*(c + 1)))
					throw ParserException("Invalid bit string");
				uint8_t major = StringUtil::GetHexValue(*c);
				uint8_t minor = StringUtil::GetHexValue(*(c + 1));
				unhex_string += (static_cast<char>((major << 4) | minor));
				c += 2;
			}
			return make_uniq<ConstantExpression>(
			    Value::BLOB(reinterpret_cast<const_data_ptr_t>(unhex_string.c_str()), unhex_string.size()));
		} else {
			string unbin_string;
			idx_t size = 0;
			char *c = val.val.str + 1;
			while (*c) {
				size += 1;
				c++;
			}
			c = val.val.str + 1;
			idx_t remainder = size % 8;
			uint8_t byte = 0;
			if (remainder) {
				for (idx_t i = remainder; i > 0; --i, ++c) {
					byte |= StringUtil::GetBinaryValue(*c) << (i - 1);
				}
				unbin_string += byte;
			}
			while (*c) {
				uint8_t byte = 0;
				for (idx_t j = 8; j > 0; --j, ++c) {
					byte |= StringUtil::GetBinaryValue(*c) << (j - 1);
				}
				unbin_string += byte;
			}
			return make_uniq<ConstantExpression>(
			    Value::BLOB(reinterpret_cast<const_data_ptr_t>(unbin_string.c_str()), unbin_string.size()));
		}
	}
	case duckdb_libpgquery::T_PGString: {
		std::string unescape_string;
		char *c = val.val.str;
		char tmp;
		while (tmp = *c) {
			if (*c++ == '\\') {
				switch(tmp = *c++) {
					case 'n':
						unescape_string.push_back('\n');
						break;
					case 't':
						unescape_string.push_back('\t');
						break;
					case 'r':
						unescape_string.push_back('\r');
						break;
					case 'b':
						unescape_string.push_back('\b');
						break;
					case '0':
						unescape_string.push_back('\0');
						break;
					case 'Z':
						unescape_string.push_back('\032');
						break;
					case '_':
					case '%':
						unescape_string.push_back('\\');
						unescape_string.push_back(tmp);
						break;
					case '\\':
						unescape_string.push_back('\\');
						break;
					default:
						unescape_string.push_back(tmp);
						while (!IsCharacter(*c)) {
							unescape_string.push_back(*c++);
						}
				}
			} else {
				unescape_string.push_back(tmp);
			}
		}
		return make_uniq<ConstantExpression>(Value(unescape_string));
	}
	case duckdb_libpgquery::T_PGFloat: {
		string_t str_val(val.val.str);
		bool try_cast_as_integer = true;
		bool try_cast_as_decimal = true;
		optional_idx decimal_position = optional_idx::Invalid();
		idx_t num_underscores = 0;
		idx_t num_integer_underscores = 0;
		for (idx_t i = 0; i < str_val.GetSize(); i++) {
			if (val.val.str[i] == '.') {
				// decimal point: cast as either decimal or double
				try_cast_as_integer = false;
				decimal_position = i;
			}
			if (val.val.str[i] == 'e' || val.val.str[i] == 'E') {
				// found exponent, cast as double
				try_cast_as_integer = false;
				try_cast_as_decimal = false;
			}
			if (val.val.str[i] == '_') {
				num_underscores++;
				if (!decimal_position.IsValid()) {
					num_integer_underscores++;
				}
			}
		}
		if (try_cast_as_integer) {
			int64_t bigint_value;
			// try to cast as bigint first
			if (TryCast::Operation<string_t, int64_t>(str_val, bigint_value)) {
				// successfully cast to bigint: bigint value
				return make_uniq<ConstantExpression>(Value::BIGINT(bigint_value));
			}
			hugeint_t hugeint_value;
			// if that is not successful; try to cast as hugeint
			if (TryCast::Operation<string_t, hugeint_t>(str_val, hugeint_value)) {
				// successfully cast to bigint: bigint value
				return make_uniq<ConstantExpression>(Value::HUGEINT(hugeint_value));
			}
			uhugeint_t uhugeint_value;
			// if that is not successful; try to cast as uhugeint
			if (TryCast::Operation<string_t, uhugeint_t>(str_val, uhugeint_value)) {
				// successfully cast to bigint: bigint value
				return make_uniq<ConstantExpression>(Value::UHUGEINT(uhugeint_value));
			}
		}
		idx_t decimal_offset = val.val.str[0] == '-' ? 3 : 2;
		if (try_cast_as_decimal && decimal_position.IsValid() &&
		    str_val.GetSize() - num_underscores < Decimal::MAX_WIDTH_DECIMAL + decimal_offset) {
			// figure out the width/scale based on the decimal position
			auto width = NumericCast<uint8_t>(str_val.GetSize() - 1 - num_underscores);
			auto scale = NumericCast<uint8_t>(width - decimal_position.GetIndex() + num_integer_underscores);
			if (val.val.str[0] == '-') {
				width--;
			}
			if (width <= Decimal::MAX_WIDTH_DECIMAL) {
				// we can cast the value as a decimal
				Value val = Value(str_val);
				val = val.DefaultCastAs(LogicalType::DECIMAL(width, scale));
				return make_uniq<ConstantExpression>(std::move(val));
			}
		}
		// if there is a decimal or the value is too big to cast as either hugeint or bigint
		double dbl_value = Cast::Operation<string_t, double>(str_val);
		return make_uniq<ConstantExpression>(Value::DOUBLE(dbl_value));
	}
	case duckdb_libpgquery::T_PGNull:
		return make_uniq<ConstantExpression>(Value(LogicalType::SQLNULL));
	default:
		throw NotImplementedException("Value not implemented!");
	}
}

unique_ptr<ParsedExpression> Transformer::TransformConstant(duckdb_libpgquery::PGAConst &c) {
	auto constant = TransformValue(c.val);
	SetQueryLocation(*constant, c.location);
	return std::move(constant);
}

} // namespace duckdb
