#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/function/aggregate/distributive_functions.hpp"
#include "duckdb/function/aggregate/distributive_function_utils.hpp"
#include "duckdb/planner/expression/bound_aggregate_expression.hpp"

namespace duckdb {

struct BaseMultiCountFunction {
	template <class STATE>
	static void Initialize(STATE &state) {
		state = 0;
	}

	template <class STATE, class OP>
	static void Combine(const STATE &source, STATE &target, AggregateInputData &) {
		target += source;
	}

	template <class T, class STATE>
	static void Finalize(STATE &state, T &target, AggregateFinalizeData &finalize_data) {
		target = state;
	}
};

struct MultiCountFunction : public BaseMultiCountFunction {
	using STATE = int64_t;

	static void MultiCountScatter(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count,
	                              Vector &states, idx_t count) {

		vector<UnifiedVectorFormat> unified_data(input_count);
		vector<const duckdb::SelectionVector *> sel;
		vector<ValidityMask> validity;
		UnifiedVectorFormat unified_state;
		states.ToUnifiedFormat(count, unified_state);
		auto state_sel = unified_state.sel;
		for (idx_t i = 0; i < input_count; i++) {
			inputs[i].ToUnifiedFormat(count, unified_data[i]);
			sel.push_back(unified_data[i].sel);
			validity.push_back(unified_data[i].validity);
		}

		for (idx_t i = 0; i < count; i++) {
			bool all_valid = true;
			for (idx_t j = 0; j < input_count; j++) {
				idx_t idx = sel[j]->get_index(i);
				if (!validity[j].RowIsValid(idx)) {
					all_valid = false;
					break;
				}
			}
			if (!all_valid) {
				continue;
			}
			(*((reinterpret_cast<STATE **>(unified_state.data))[state_sel->get_index(i)]))++;
		}
	}

	static void MultiCountUpdate(Vector inputs[], AggregateInputData &, idx_t input_count, data_ptr_t state_p,
	                             idx_t count) {

		vector<UnifiedVectorFormat> unified_data(input_count);
		vector<const duckdb::SelectionVector *> sel;
		vector<ValidityMask> validity;
		auto &result = *reinterpret_cast<STATE *>(state_p);
		for (idx_t i = 0; i < input_count; i++) {
			inputs[i].ToUnifiedFormat(count, unified_data[i]);
			sel.push_back(unified_data[i].sel);
			validity.push_back(unified_data[i].validity);
		}

		for (idx_t i = 0; i < count; i++) {
			bool all_valid = true;
			for (idx_t j = 0; j < input_count; j++) {
				idx_t idx = sel[j]->get_index(i);
				if (!validity[j].RowIsValid(idx)) {
					all_valid = false;
					break;
				}
			}
			if (!all_valid) {
				continue;
			}
			result++;
		}
	}
};

AggregateFunction MultiCountFun::GetFunction() {
	AggregateFunction fun({LogicalType(LogicalTypeId::ANY)}, LogicalType::BIGINT, AggregateFunction::StateSize<int64_t>,
	                      AggregateFunction::StateInitialize<int64_t, MultiCountFunction>,
	                      MultiCountFunction::MultiCountScatter,
	                      AggregateFunction::StateCombine<int64_t, MultiCountFunction>,
	                      AggregateFunction::StateFinalize<int64_t, int64_t, MultiCountFunction>,
	                      FunctionNullHandling::SPECIAL_HANDLING, MultiCountFunction::MultiCountUpdate);
	fun.name = "multi_count";
	fun.order_dependent = AggregateOrderDependent::NOT_ORDER_DEPENDENT;
    fun.varargs = LogicalType(LogicalTypeId::ANY);
	return fun;
}

} // namespace duckdb
