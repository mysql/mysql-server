#include "json_functions.hpp"
#include "json_common.hpp"
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

struct MemberOfBindData : public FunctionData {
	// If true, the VARCHAR value must be wrapped in JSON double-quotes to become a JSON string.
	// If false, the VARCHAR representation is already a valid JSON literal (number, boolean, or JSON).
	bool wrap_as_json_string;

	explicit MemberOfBindData(bool wrap_as_json_string_p) : wrap_as_json_string(wrap_as_json_string_p) {
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<MemberOfBindData>(wrap_as_json_string);
	}

	bool Equals(const FunctionData &other_p) const override {
		auto &other = other_p.Cast<MemberOfBindData>();
		return wrap_as_json_string == other.wrap_as_json_string;
	}
};

static unique_ptr<FunctionData> MemberOfBind(ClientContext &context, ScalarFunction &bound_function,
                                             vector<unique_ptr<Expression>> &arguments) {
	auto &value_type = arguments[0]->return_type;
	bool wrap_as_json_string = false;

	if (value_type == LogicalType::JSON()) {
		// Already JSON - parse directly
		wrap_as_json_string = false;
	} else if (value_type.id() == LogicalTypeId::VARCHAR) {
		// SQL string -> wrap as JSON string
		wrap_as_json_string = true;
	} else if (value_type.id() == LogicalTypeId::BOOLEAN) {
		// "true"/"false" are valid JSON booleans
		wrap_as_json_string = false;
		bound_function.arguments[0] = LogicalType::VARCHAR;
	} else if (value_type.IsNumeric()) {
		// Numeric types -> their VARCHAR representation is a valid JSON number
		wrap_as_json_string = false;
		bound_function.arguments[0] = LogicalType::VARCHAR;
	} else {
		// DATE, TIMESTAMP, UUID, etc. -> treat as JSON string
		wrap_as_json_string = true;
		bound_function.arguments[0] = LogicalType::VARCHAR;
	}

	return make_uniq<MemberOfBindData>(wrap_as_json_string);
}

static void EscapeJSONString(const string_t &input, string &output) {
	output.clear();
	output.push_back('"');
	auto data = input.GetData();
	auto size = input.GetSize();
	for (idx_t i = 0; i < size; i++) {
		char c = data[i];
		switch (c) {
		case '"':
			output.append("\\\"");
			break;
		case '\\':
			output.append("\\\\");
			break;
		case '\b':
			output.append("\\b");
			break;
		case '\f':
			output.append("\\f");
			break;
		case '\n':
			output.append("\\n");
			break;
		case '\r':
			output.append("\\r");
			break;
		case '\t':
			output.append("\\t");
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				// Control characters
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
				output.append(buf);
			} else {
				output.push_back(c);
			}
			break;
		}
	}
	output.push_back('"');
}

static bool MemberOfEquals(yyjson_val *lhs, yyjson_val *rhs) {
	auto ltype = unsafe_yyjson_get_type(lhs);
	auto rtype = unsafe_yyjson_get_type(rhs);
	if (ltype != rtype) {
		return false;
	}
	if (ltype == YYJSON_TYPE_NUM) {
		// Compare all numeric types by value to handle integer/decimal comparability
		// (e.g., 1 == 1.0). This is slightly more permissive than MySQL which distinguishes
		// DOUBLE from DECIMAL, but yyjson has no DECIMAL subtype.
		double lval = unsafe_yyjson_get_num(lhs);
		double rval = unsafe_yyjson_get_num(rhs);
		return lval == rval;
	}
	return unsafe_yyjson_equals(lhs, rhs);
}

static void MemberOfExecute(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.data.size() == 2);
	auto &lstate = JSONFunctionLocalState::ResetAndGet(state);
	auto alc = lstate.json_allocator->GetYYAlc();

	auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	auto &bind_data = func_expr.bind_info->Cast<MemberOfBindData>();
	bool wrap = bind_data.wrap_as_json_string;

	auto &values = args.data[0];
	auto &arrays = args.data[1];
	auto count = args.size();

	string escaped_buf;

	BinaryExecutor::ExecuteWithNulls<string_t, string_t, bool>(
	    values, arrays, result, count, [&](string_t value_str, string_t array_str, ValidityMask &mask, idx_t idx) {
		    // Parse the array
		    auto array_doc = JSONCommon::ReadDocument(array_str, JSONCommon::READ_FLAG, alc);
		    auto array_root = array_doc->root;
		    if (!array_root || !yyjson_is_arr(array_root)) {
			    mask.SetInvalid(idx);
			    return false;
		    }

		    // Build the JSON text for the value
		    const char *json_text;
		    idx_t json_len;
		    if (wrap) {
			    EscapeJSONString(value_str, escaped_buf);
			    json_text = escaped_buf.c_str();
			    json_len = escaped_buf.size();
		    } else {
			    json_text = value_str.GetData();
			    json_len = value_str.GetSize();
		    }

		    // Parse the value as JSON
		    auto value_doc = JSONCommon::ReadDocument(string_t(json_text, UnsafeNumericCast<uint32_t>(json_len)),
		                                              JSONCommon::READ_FLAG, alc);
		    if (!value_doc->root) {
			    mask.SetInvalid(idx);
			    return false;
		    }

		    // Iterate array elements and compare
		    size_t arr_idx, arr_max;
		    yyjson_val *element;
		    yyjson_arr_foreach(array_root, arr_idx, arr_max, element) {
			    if (MemberOfEquals(element, value_doc->root)) {
				    return true;
			    }
		    }
		    return false;
	    });
}

static void AddMemberOfFunction(ScalarFunctionSet &set, const LogicalType &lhs, const LogicalType &rhs) {
	set.AddFunction(ScalarFunction({lhs, rhs}, LogicalType::BOOLEAN, MemberOfExecute, MemberOfBind, nullptr, nullptr,
	                               JSONFunctionLocalState::Init));
}

ScalarFunctionSet JSONFunctions::GetMemberOfFunction() {
	ScalarFunctionSet set("member_of");
	AddMemberOfFunction(set, LogicalType::ANY, LogicalType::VARCHAR);
	AddMemberOfFunction(set, LogicalType::ANY, LogicalType::JSON());
	for (auto &func : set.functions) {
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	return set;
}

} // namespace duckdb