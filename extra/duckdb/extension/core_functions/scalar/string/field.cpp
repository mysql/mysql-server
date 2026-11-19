#include "core_functions/scalar/string_functions.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/scalar/string_common.hpp"
#include "duckdb/common/value_operations/value_operations.hpp"
#include "utf8proc.hpp"

#include "duckdb/common/types/decimal.hpp"

namespace duckdb {

template <class T>
static void FieldFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() >= 2);

	auto count = args.size();

	result.SetVectorType(VectorType::CONSTANT_VECTOR);
	for (idx_t i = 0; i < args.ColumnCount(); i++) {
		if (args.data[i].GetVectorType() != VectorType::CONSTANT_VECTOR) {
			result.SetVectorType(VectorType::FLAT_VECTOR);
			break;
		}
	}

	auto result_data = result.GetVectorType() == VectorType::CONSTANT_VECTOR ? ConstantVector::GetData<int32_t>(result)
	                                                                         : FlatVector::GetData<int32_t>(result);

	for (idx_t i = 0; i < count; i++) {
		result_data[i] = 0;
	}

	auto &lhs = args.data[0];
	UnifiedVectorFormat lhs_v;
	lhs.ToUnifiedFormat(count, lhs_v);
	auto lhs_values = UnifiedVectorFormat::GetData<T>(lhs_v);

	for (idx_t col = 1; col < args.ColumnCount(); col++) {
		auto &arg = args.data[col];
		UnifiedVectorFormat arg_v;
		arg.ToUnifiedFormat(count, arg_v);
		auto arg_values = UnifiedVectorFormat::GetData<T>(arg_v);
		for (idx_t i = 0; i < count; i++) {
			auto idx_lhs = lhs_v.sel->get_index(i);
			auto idx_arg = arg_v.sel->get_index(i);
			if (!lhs_v.validity.RowIsValid(idx_lhs)) {
				continue;
			}
			if (result_data[i] != 0) {
				continue;
			}
			if (!arg_v.validity.RowIsValid(idx_arg)) {
				continue;
			}
			if (Equals::Operation(lhs_values[idx_lhs], arg_values[idx_arg])) {
				result_data[i] = col;
			}
		}
	}
}

unique_ptr<FunctionData> BindDecimalField(ClientContext &context, ScalarFunction &bound_function,
                                          vector<unique_ptr<Expression>> &arguments) {

	uint8_t max_scale = 0;
	uint8_t max_width_over_scale = 0;

	for (idx_t i = 0; i < arguments.size(); i++) {
		uint8_t width, scale;
		auto can_convert = arguments[i]->return_type.GetDecimalProperties(width, scale);
		if (!can_convert) {
			throw InternalException("Could not convert type %s to a decimal.", arguments[i]->return_type.ToString());
		}
		max_scale = MaxValue<uint8_t>(scale, max_scale);
		max_width_over_scale = MaxValue<uint8_t>(max_width_over_scale, width - scale);
	}

	if (max_width_over_scale + max_scale > Decimal::MAX_WIDTH_DECIMAL) {
		throw NotImplementedException("max_width > %d is not supported", Decimal::MAX_WIDTH_DECIMAL);
	}

	LogicalType return_type = LogicalType::DECIMAL(max_width_over_scale + max_scale, max_scale);
	switch (return_type.InternalType()) {
	case PhysicalType::INT16:
		bound_function.function = FieldFunction<int16_t>;
		break;
	case PhysicalType::INT32:
		bound_function.function = FieldFunction<int32_t>;
		break;
	case PhysicalType::INT64:
		bound_function.function = FieldFunction<int64_t>;
		break;
	case PhysicalType::INT128:
		bound_function.function = FieldFunction<hugeint_t>;
		break;
	default:
		throw NotImplementedException("Unsupported type for field function");
	}

	for (idx_t i = 0; i < bound_function.arguments.size(); i++) {
		bound_function.arguments[i] = return_type;
	}
	bound_function.varargs = return_type;

	return nullptr;
}

ScalarFunctionSet FieldFun::GetFunctions() {
	ScalarFunctionSet field;
	ScalarFunction field_varchar =
	    ScalarFunction("field", {LogicalTypeId::VARCHAR, LogicalTypeId::VARCHAR}, LogicalType::INTEGER, FieldFunction<string_t>);
	field_varchar.varargs = LogicalType::VARCHAR;
	field_varchar.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
	field.AddFunction(field_varchar);
	ScalarFunction field_int =
	    ScalarFunction("field", {LogicalTypeId::HUGEINT, LogicalTypeId::HUGEINT}, LogicalType::INTEGER, FieldFunction<hugeint_t>);
	field_int.varargs = LogicalType::HUGEINT;
	field_int.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
	field.AddFunction(field_int);
	ScalarFunction field_decimal = ScalarFunction("field", {LogicalTypeId::DECIMAL, LogicalTypeId::DECIMAL},
	                                              LogicalType::INTEGER, nullptr, BindDecimalField);
	field_decimal.varargs = LogicalType::DECIMAL(38, 18);
	field_decimal.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
	field.AddFunction(field_decimal);
	ScalarFunction field_double =
	    ScalarFunction("field", {LogicalTypeId::DOUBLE, LogicalTypeId::DOUBLE}, LogicalType::INTEGER, FieldFunction<double>);
	field_double.varargs = LogicalType::DOUBLE;
	field_double.null_handling = FunctionNullHandling::SPECIAL_HANDLING;
	field.AddFunction(field_double);

	return field;
}

} // namespace duckdb