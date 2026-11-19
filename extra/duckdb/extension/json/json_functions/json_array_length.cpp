#include "json_executors.hpp"

namespace duckdb {

static inline uint64_t GetArrayLength(yyjson_val *val, yyjson_alc *, Vector &, ValidityMask &, idx_t) {
	return yyjson_arr_size(val);
}

static inline uint64_t GetLength(yyjson_val *val, yyjson_alc *, Vector &, ValidityMask &, idx_t) {
	if (yyjson_is_arr(val)) {
		return yyjson_arr_size(val);
	} else if (yyjson_is_obj(val)) {
		return yyjson_obj_size(val);
	} else {
		return 1;
	}
}
static void UnaryArrayLengthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::UnaryExecute<uint64_t>(args, state, result, GetArrayLength);
}

static void UnaryLengthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::UnaryExecute<uint64_t>(args, state, result, GetLength);
}

static void BinaryArrayLengthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::BinaryExecute<uint64_t>(args, state, result, GetArrayLength);
}

static void BinaryLengthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::BinaryExecute<uint64_t>(args, state, result, GetLength);
}

static void ManyArrayLengthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::ExecuteMany<uint64_t>(args, state, result, GetArrayLength);
}

static void ManyLengthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::ExecuteMany<uint64_t>(args, state, result, GetLength);
}

static void GetArrayLengthFunctionsInternal(ScalarFunctionSet &set, const LogicalType &input_type) {
	set.AddFunction(ScalarFunction({input_type}, LogicalType::UBIGINT, UnaryArrayLengthFunction, nullptr, nullptr,
	                               nullptr, JSONFunctionLocalState::Init));
	set.AddFunction(ScalarFunction({input_type, LogicalType::VARCHAR}, LogicalType::UBIGINT, BinaryArrayLengthFunction,
	                               JSONReadFunctionData::Bind, nullptr, nullptr, JSONFunctionLocalState::Init));
	set.AddFunction(ScalarFunction({input_type, LogicalType::LIST(LogicalType::VARCHAR)},
	                               LogicalType::LIST(LogicalType::UBIGINT), ManyArrayLengthFunction,
	                               JSONReadManyFunctionData::Bind, nullptr, nullptr, JSONFunctionLocalState::Init));
}

ScalarFunctionSet JSONFunctions::GetArrayLengthFunction() {
	ScalarFunctionSet set("json_array_length");
	GetArrayLengthFunctionsInternal(set, LogicalType::VARCHAR);
	GetArrayLengthFunctionsInternal(set, LogicalType::JSON());
	for (auto &func : set.functions) {
		if (func.arguments.size() == 1 && func.arguments[0].IsJSONType()) {
			continue;
		}
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	return set;
}

static void GetLengthFunctionsInternal(ScalarFunctionSet &set, const LogicalType &input_type) {
	set.AddFunction(ScalarFunction({input_type}, LogicalType::UBIGINT, UnaryLengthFunction, nullptr, nullptr,
	                               nullptr, JSONFunctionLocalState::Init));
	set.AddFunction(ScalarFunction({input_type, LogicalType::VARCHAR}, LogicalType::UBIGINT, BinaryLengthFunction,
	                               JSONReadFunctionData::Bind, nullptr, nullptr, JSONFunctionLocalState::Init));
	set.AddFunction(ScalarFunction({input_type, LogicalType::LIST(LogicalType::VARCHAR)},
	                               LogicalType::LIST(LogicalType::UBIGINT), ManyLengthFunction,
	                               JSONReadManyFunctionData::Bind, nullptr, nullptr, JSONFunctionLocalState::Init));
}

ScalarFunctionSet JSONFunctions::GetLengthFunction() {
	ScalarFunctionSet set("json_length");
	GetLengthFunctionsInternal(set, LogicalType::VARCHAR);
	GetLengthFunctionsInternal(set, LogicalType::JSON());
	for (auto &func : set.functions) {
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	return set;
}

} // namespace duckdb
