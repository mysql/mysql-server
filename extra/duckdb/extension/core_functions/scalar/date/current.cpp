#include "core_functions/scalar/date_functions.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/transaction/meta_transaction.hpp"
#include "duckdb/planner/expression/bound_cast_expression.hpp"

#include "duckdb/mysql/timestamp_context_state.hpp"

namespace duckdb {

struct UnixTimestampOperator {
	template <typename INPUT_TYPE, typename RESULT_TYPE>
	static RESULT_TYPE Operation(INPUT_TYPE input) {
		const auto end = Timestamp::GetEpochMicroSeconds(input);
		return static_cast<double>(end) / Interval::MICROS_PER_SEC;
	}
};

template<>
double UnixTimestampOperator::Operation(timestamp_tz_t input) {
	dtime_t t0(0);
	const auto end = Timestamp::GetEpochMicroSeconds(input);
	return static_cast<double>(end) / Interval::MICROS_PER_SEC;
}

static timestamp_t GetTransactionTimestamp(ExpressionState &state) {
	return MetaTransaction::Get(state.GetContext()).start_timestamp;
}

static timestamp_t GetQueryTimestamp(ExpressionState &state) {
	Value ts;
	if (state.GetContext().TryGetCurrentSetting("timestamp", ts)) {
		if (BigIntValue::Get(ts) != -1) {
			return timestamp_t(BigIntValue::Get(ts));
		}
	}
	return state.GetContext().registered_state->Get<TimestampContextState>("start_timestamp")->start_timestamp;
}

static void CurrentTimestampFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 0 || input.ColumnCount() == 1);
	auto ts = GetQueryTimestamp(state);
	auto val = Value::TIMESTAMPTZ(timestamp_tz_t(ts));
	result.Reference(val);
}

static void UtcDateFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 0);
	auto val = Value::DATE(Timestamp::GetDate(GetQueryTimestamp(state)));
	result.Reference(val);
}

static void UtcTimeFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 0 || input.ColumnCount() == 1);
	auto val = Value::TIME(Timestamp::GetTime(GetQueryTimestamp(state)));
	result.Reference(val);
}

static void UtcTimeStampFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 0 || input.ColumnCount() == 1);
	auto ts = GetQueryTimestamp(state);
	auto val = Value::TIMESTAMP(ts);
	result.Reference(val);
}

static void UnixTimestampFunctionNoParam(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 0);
	auto val = Value::DOUBLE(GetQueryTimestamp(state).value / Interval::MICROS_PER_SEC);
	result.Reference(val);
}

template<typename T>
static void UnixTimestampFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 1);
	UnaryExecutor::Execute<T, double, UnixTimestampOperator>(input.data[0], result, input.size());
}

ScalarFunction UtcDateFun::GetFunction() {
	ScalarFunction current_date({}, LogicalType::DATE, UtcDateFunction);
	current_date.stability = FunctionStability::CONSISTENT_WITHIN_QUERY;
	return current_date;
}

ScalarFunctionSet UtcTimeFun::GetFunctions() {
	ScalarFunctionSet utc_time;
	utc_time.AddFunction(ScalarFunction({}, LogicalType::TIME, UtcTimeFunction));
	utc_time.AddFunction(ScalarFunction({LogicalTypeId::INTEGER}, LogicalType::TIME, UtcTimeFunction));
	for (auto &func : utc_time.functions) {
		func.stability = FunctionStability::CONSISTENT_WITHIN_QUERY;
	}
	return utc_time;
}

ScalarFunctionSet UtcTimestampFun::GetFunctions() {
	ScalarFunctionSet utc_timestamp;
	utc_timestamp.AddFunction(ScalarFunction({}, LogicalType::TIMESTAMP, UtcTimeStampFunction));
	utc_timestamp.AddFunction(ScalarFunction({LogicalTypeId::INTEGER}, LogicalType::TIMESTAMP, UtcTimeStampFunction));
	for (auto &func : utc_timestamp.functions) {
		func.stability = FunctionStability::CONSISTENT_WITHIN_QUERY;
	}
	return utc_timestamp;
}

ScalarFunctionSet UnixTimestampFun::GetFunctions() {
	ScalarFunctionSet unix_timestamp;
	unix_timestamp.AddFunction(ScalarFunction({}, LogicalType::DOUBLE, UnixTimestampFunctionNoParam));
	unix_timestamp.AddFunction(ScalarFunction({LogicalType::TIMESTAMP_TZ}, LogicalType::DOUBLE, UnixTimestampFunction<timestamp_tz_t>));
	for (auto &func : unix_timestamp.functions) {
		func.stability = FunctionStability::CONSISTENT_WITHIN_QUERY;
	}
	return unix_timestamp;
}

ScalarFunctionSet GetCurrentTimestampFun::GetFunctions() {
	ScalarFunctionSet current_timestamp;
	current_timestamp.AddFunction(ScalarFunction({}, LogicalType::TIMESTAMP_TZ, CurrentTimestampFunction));
	current_timestamp.AddFunction(ScalarFunction({LogicalTypeId::INTEGER}, LogicalType::TIMESTAMP_TZ, CurrentTimestampFunction));
	for (auto &func : current_timestamp.functions) {
		func.stability = FunctionStability::CONSISTENT_WITHIN_QUERY;
	}
	return current_timestamp;
}

} // namespace duckdb
