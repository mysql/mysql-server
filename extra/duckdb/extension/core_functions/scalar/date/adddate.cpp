#include "duckdb/common/operator/add.hpp"
#include "duckdb/common/operator/subtract.hpp"
#include "core_functions/scalar/date_functions.hpp"

namespace duckdb {

    ScalarFunctionSet AdddateFun::GetFunctions() {
        ScalarFunctionSet set;
        set.AddFunction(ScalarFunction({LogicalTypeId::DATE, LogicalTypeId::INTERVAL}, LogicalTypeId::TIMESTAMP, 
            ScalarFunction::BinaryFunction<date_t, interval_t, timestamp_t, AddOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::DATE, LogicalTypeId::INTEGER}, LogicalTypeId::DATE,
            ScalarFunction::BinaryFunction<date_t, int32_t, date_t, AddOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP, LogicalTypeId::INTERVAL}, LogicalTypeId::TIMESTAMP,
            ScalarFunction::BinaryFunction<timestamp_t, interval_t, timestamp_t, AddOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP, LogicalTypeId::INTEGER}, LogicalTypeId::TIMESTAMP,
            ScalarFunction::BinaryFunction<timestamp_t, int32_t, timestamp_t, AddOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP_TZ, LogicalTypeId::INTERVAL}, LogicalTypeId::TIMESTAMP_TZ,
            ScalarFunction::BinaryFunction<timestamp_t, interval_t, timestamp_t, AddOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP_TZ, LogicalTypeId::INTEGER}, LogicalTypeId::TIMESTAMP_TZ,
            ScalarFunction::BinaryFunction<timestamp_t, int32_t, timestamp_t, AddOperator>));
        for (auto function : set.functions) {
            function.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
        }
        return set;
    }

    ScalarFunctionSet SubdateFun::GetFunctions() {
        ScalarFunctionSet set;
        set.AddFunction(ScalarFunction({LogicalTypeId::DATE, LogicalTypeId::INTERVAL}, LogicalTypeId::TIMESTAMP, 
            ScalarFunction::BinaryFunction<date_t, interval_t, timestamp_t, SubtractOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::DATE, LogicalTypeId::INTEGER}, LogicalTypeId::DATE,
            ScalarFunction::BinaryFunction<date_t, int32_t, date_t, SubtractOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP, LogicalTypeId::INTERVAL}, LogicalTypeId::TIMESTAMP,
            ScalarFunction::BinaryFunction<timestamp_t, interval_t, timestamp_t, SubtractOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP, LogicalTypeId::INTEGER}, LogicalTypeId::TIMESTAMP,
            ScalarFunction::BinaryFunction<timestamp_t, int32_t, timestamp_t, SubtractOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP_TZ, LogicalTypeId::INTERVAL}, LogicalTypeId::TIMESTAMP_TZ,
            ScalarFunction::BinaryFunction<timestamp_t, interval_t, timestamp_t, SubtractOperator>));
        set.AddFunction(ScalarFunction({LogicalTypeId::TIMESTAMP_TZ, LogicalTypeId::INTEGER}, LogicalTypeId::TIMESTAMP_TZ,
            ScalarFunction::BinaryFunction<timestamp_t, int32_t, timestamp_t, SubtractOperator>));
        for (auto function : set.functions) {
            function.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
        }
        return set;
    }

} // namespace duckdb