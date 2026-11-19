#include "core_functions/aggregate/distributive_functions.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/null_value.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/algorithm.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/common/serializer/serializer.hpp"
#include "duckdb/common/serializer/deserializer.hpp"

namespace duckdb {

struct MultiStringAggState {
	idx_t size;
	idx_t alloc_size;
	char *dataptr;
};

struct MultiStringAggBindData : public FunctionData {
	explicit MultiStringAggBindData(string sep_p) : sep(std::move(sep_p)) {
	}

	string sep;

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<MultiStringAggBindData>(sep);
	}
	bool Equals(const FunctionData &other_p) const override {
		auto &other = other_p.Cast<MultiStringAggBindData>();
		return sep == other.sep;
	}
};

struct MultiStringAggFunction {
	template <class STATE>
	static void Initialize(STATE &state) {
		state.dataptr = nullptr;
		state.alloc_size = 0;
		state.size = 0;
	}

	template <class T, class STATE>
	static void Finalize(STATE &state, T &target, AggregateFinalizeData &finalize_data) {
		if (!state.dataptr) {
			finalize_data.ReturnNull();
		} else {
			target = string_t(state.dataptr, state.size);
		}
	}

	static inline void PerformOperation(MultiStringAggState &state, ArenaAllocator &allocator, const char *str,
	                                    const char *sep, idx_t str_size, idx_t sep_size) {
		if (!state.dataptr) {
			// first iteration: allocate space for the string and copy it into the state
			state.alloc_size = MaxValue<idx_t>(8, NextPowerOfTwo(str_size));
			state.dataptr = char_ptr_cast(allocator.Allocate(state.alloc_size));
			state.size = str_size;
			memcpy(state.dataptr, str, str_size);
		} else {
			// subsequent iteration: first check if we have space to place the string and separator
			idx_t required_size = state.size + str_size + sep_size;
			if (required_size > state.alloc_size) {
				// no space! allocate extra space
				const auto old_size = state.alloc_size;
				while (state.alloc_size < required_size) {
					state.alloc_size *= 2;
				}
				state.dataptr =
				    char_ptr_cast(allocator.Reallocate(data_ptr_cast(state.dataptr), old_size, state.alloc_size));
			}
			// copy the separator
			memcpy(state.dataptr + state.size, sep, sep_size);
			state.size += sep_size;
			// copy the string
			memcpy(state.dataptr + state.size, str, str_size);
			state.size += str_size;
		}
	}

	static inline void PerformOperation(MultiStringAggState &state, ArenaAllocator &allocator, string_t str,
	                                    optional_ptr<FunctionData> data_p) {
		auto &data = data_p->Cast<MultiStringAggBindData>();
		PerformOperation(state, allocator, str.GetData(), data.sep.c_str(), str.GetSize(), data.sep.size());
	}

	template <class STATE, class OP>
	static void Combine(const STATE &source, STATE &target, AggregateInputData &aggr_input_data) {
		if (!source.dataptr) {
			// source is not set: skip combining
			return;
		}
		PerformOperation(target, aggr_input_data.allocator,
		                 string_t(source.dataptr, UnsafeNumericCast<uint32_t>(source.size)), aggr_input_data.bind_data);
	}
};

unique_ptr<FunctionData> MultiStringAggBind(ClientContext &context, AggregateFunction &function,
                                            vector<unique_ptr<Expression>> &arguments) {
	D_ASSERT(arguments.size() >= 2);
	if (arguments[0]->HasParameter()) {
		throw ParameterNotResolvedException();
	}
	if (!arguments[0]->IsFoldable()) {
		throw BinderException("Separator argument to StringAgg must be a constant");
	}
	auto separator_val = ExpressionExecutor::EvaluateScalar(context, *arguments[0]);
	auto separator_string = separator_val.ToString();
	return make_uniq<MultiStringAggBindData>(std::move(separator_string));
}

static void MultiStringAggSerialize(Serializer &serializer, const optional_ptr<FunctionData> bind_data_p,
                                    const AggregateFunction &function) {
	auto bind_data = bind_data_p->Cast<MultiStringAggBindData>();
	serializer.WriteProperty(100, "separator", bind_data.sep);
}

unique_ptr<FunctionData> MultiStringAggDeserialize(Deserializer &deserializer, AggregateFunction &bound_function) {
	auto sep = deserializer.ReadProperty<string>(100, "separator");
	return make_uniq<MultiStringAggBindData>(std::move(sep));
}

void ArrayGroupConcatUpdate(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count, data_ptr_t state,
                            idx_t count) {

	vector<UnifiedVectorFormat> unified_data(input_count);
	vector<const string_t *> data;
	vector<const duckdb::SelectionVector *> sel;
	vector<ValidityMask> validity;
	MultiStringAggState *state_agg = (MultiStringAggState *)state;
	MultiStringAggBindData bind_data = aggr_input_data.bind_data->Cast<MultiStringAggBindData>();
	idx_t sep_size = bind_data.sep.size();
	for (idx_t i = 1; i < input_count; i++) {
		inputs[i].ToUnifiedFormat(count, unified_data[i - 1]);
		data.push_back(UnifiedVectorFormat::GetData<string_t>(unified_data[i - 1]));
		sel.push_back(unified_data[i - 1].sel);
		validity.push_back(unified_data[i - 1].validity);
	}

	for (idx_t i = 0; i < count; i++) {
		vector<idx_t> sizes(input_count - 1, 0);
		vector<idx_t> idxes(input_count - 1, 0);
		idx_t str_size = 0;
		bool all_valid = true;
		for (idx_t j = 0; j < input_count - 1; j++) {
			idx_t idx = sel[j]->get_index(i);
			if (!validity[j].RowIsValid(idx)) {
				all_valid = false;
				break;
			}
			str_size += data[j][idx].GetSize();
			idxes[j] = idx;
			sizes[j] = data[j][idx].GetSize();
		}
		if (!all_valid) {
			continue;
		}
		if (!state_agg->dataptr) {
			// first iteration: allocate space for the string and copy it into the state
			state_agg->alloc_size = MaxValue<idx_t>(8, NextPowerOfTwo(str_size));
			state_agg->dataptr = char_ptr_cast(aggr_input_data.allocator.Allocate(state_agg->alloc_size));
			for (idx_t j = 0; j < input_count - 1; j++) {
				memcpy(state_agg->dataptr + state_agg->size, data[j][idxes[j]].GetData(), sizes[j]);
				state_agg->size += sizes[j];
			}
		} else {
			// subsequent iteration: first check if we have space to place the string and separator
			idx_t required_size = state_agg->size + str_size + sep_size;
			if (required_size > state_agg->alloc_size) {
				// no space! allocate extra space
				const auto old_size = state_agg->alloc_size;
				while (state_agg->alloc_size < required_size) {
					state_agg->alloc_size *= 2;
				}
				state_agg->dataptr = char_ptr_cast(aggr_input_data.allocator.Reallocate(
				    data_ptr_cast(state_agg->dataptr), old_size, state_agg->alloc_size));
			}
			// copy the separator
			memcpy(state_agg->dataptr + state_agg->size, bind_data.sep.c_str(), sep_size);
			state_agg->size += sep_size;
			// copy the string
			for (idx_t j = 0; j < input_count - 1; j++) {
				memcpy(state_agg->dataptr + state_agg->size, data[j][idxes[j]].GetData(), sizes[j]);
				state_agg->size += sizes[j];
			}
		}
	}
}

void ArrayGroupConcatScatterUpdate(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count,
                                   Vector &states, idx_t count) {
	vector<UnifiedVectorFormat> unified_data(input_count - 1);
	vector<const string_t *> data;
	vector<const duckdb::SelectionVector *> sel;
	vector<ValidityMask> validity;
	UnifiedVectorFormat unified_state;
	states.ToUnifiedFormat(count, unified_state);
	auto state_sel = unified_state.sel;
	auto state_validity = unified_state.validity;
	auto state_data = (MultiStringAggState **)unified_state.data;
	MultiStringAggBindData bind_data = aggr_input_data.bind_data->Cast<MultiStringAggBindData>();
	idx_t sep_size = bind_data.sep.size();
	for (idx_t i = 1; i < input_count; i++) {
		inputs[i].ToUnifiedFormat(count, unified_data[i - 1]);
		data.push_back(UnifiedVectorFormat::GetData<string_t>(unified_data[i - 1]));
		sel.push_back(unified_data[i - 1].sel);
		validity.push_back(unified_data[i - 1].validity);
	}

	for (idx_t i = 0; i < count; i++) {
		vector<idx_t> sizes(input_count - 1, 0);
		vector<idx_t> idxes(input_count - 1, 0);
		idx_t str_size = 0;
		bool all_valid = true;
		for (idx_t j = 0; j < input_count - 1; j++) {
			idx_t idx = sel[j]->get_index(i);
			if (!validity[j].RowIsValid(idx)) {
				all_valid = false;
				break;
			}
			str_size += data[j][idx].GetSize();
			idxes[j] = idx;
			sizes[j] = data[j][idx].GetSize();
		}
		if (!all_valid) {
			continue;
		}
		MultiStringAggState *state_agg = state_data[state_sel->get_index(i)];
		if (!state_agg->dataptr) {
			// first iteration: allocate space for the string and copy it into the state
			state_agg->alloc_size = MaxValue<idx_t>(8, NextPowerOfTwo(str_size));
			state_agg->dataptr = char_ptr_cast(aggr_input_data.allocator.Allocate(state_agg->alloc_size));
			for (idx_t j = 0; j < input_count - 1; j++) {
				memcpy(state_agg->dataptr + state_agg->size, data[j][idxes[j]].GetData(), sizes[j]);
				state_agg->size += sizes[j];
			}
		} else {
			// subsequent iteration: first check if we have space to place the string and separator
			idx_t required_size = state_agg->size + str_size + sep_size;
			if (required_size > state_agg->alloc_size) {
				// no space! allocate extra space
				const auto old_size = state_agg->alloc_size;
				while (state_agg->alloc_size < required_size) {
					state_agg->alloc_size *= 2;
				}
				state_agg->dataptr = char_ptr_cast(aggr_input_data.allocator.Reallocate(
				    data_ptr_cast(state_agg->dataptr), old_size, state_agg->alloc_size));
			}
			// copy the separator
			memcpy(state_agg->dataptr + state_agg->size, bind_data.sep.c_str(), sep_size);
			state_agg->size += sep_size;
			// copy the string
			for (idx_t j = 0; j < input_count - 1; j++) {
				memcpy(state_agg->dataptr + state_agg->size, data[j][idxes[j]].GetData(), sizes[j]);
				state_agg->size += sizes[j];
			}
		}
	}
}

AggregateFunction MultiStringAggFun::GetFunction() {
	AggregateFunction multi_string_agg_param(
	    {LogicalType::VARCHAR, LogicalType::ANY_PARAMS(LogicalType::VARCHAR)}, LogicalType::VARCHAR,
	    AggregateFunction::StateSize<MultiStringAggState>,
	    AggregateFunction::StateInitialize<MultiStringAggState, MultiStringAggFunction>, ArrayGroupConcatScatterUpdate,
	    AggregateFunction::StateCombine<MultiStringAggState, MultiStringAggFunction>,
	    AggregateFunction::StateFinalize<MultiStringAggState, string_t, MultiStringAggFunction>, ArrayGroupConcatUpdate,
	    MultiStringAggBind);
	multi_string_agg_param.serialize = MultiStringAggSerialize;
	multi_string_agg_param.deserialize = MultiStringAggDeserialize;
	multi_string_agg_param.varargs = LogicalType::ANY_PARAMS(LogicalType::VARCHAR);
	return multi_string_agg_param;
}

} // namespace duckdb
