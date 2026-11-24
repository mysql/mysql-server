#include "json_executors.hpp"

namespace duckdb {

static inline bool JSONExists(yyjson_val *val, yyjson_alc *, Vector &, ValidityMask &, idx_t) {
	return val;
}

static void BinaryExistsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::BinaryExecute<bool, false>(args, state, result, JSONExists);
}

static void ManyExistsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	JSONExecutors::ExecuteMany<bool, false>(args, state, result, JSONExists);
}

static void ContainsPathFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);

	auto &lstate = JSONFunctionLocalState::ResetAndGet(state);
	auto alc = lstate.json_allocator->GetYYAlc();

	const auto count = args.size();

	vector<bool> one_or_all_cached(count, true);
	vector<bool> result_is_null(count, false);
	auto result_data = FlatVector::GetData<bool>(result);
	{
		auto &one_or_all_vector = args.data[1];
		if (one_or_all_vector.GetVectorType() == VectorType::CONSTANT_VECTOR) {
			if (ConstantVector::IsNull(one_or_all_vector)) {
				result_is_null[0] = true;
			} else {
				auto one_or_all_data = ConstantVector::GetData<string_t>(one_or_all_vector);
				auto tmp = one_or_all_data->GetString();

				if (strcasecmp(tmp.c_str(), "all") == 0) {
					one_or_all_cached[0] = false;
					result_data[0] = true;
				} else if (strcasecmp(tmp.c_str(), "one") == 0) {
					one_or_all_cached[0] = true;
					result_data[0] = false;
				} else {
					throw ParameterNotAllowedException("The oneOrAll argument to json_contains_path may take these values: 'one' or 'all'.");
				}
			}
		} else {
			result.SetVectorType(VectorType::FLAT_VECTOR);
			UnifiedVectorFormat one_or_all_data;
			one_or_all_vector.ToUnifiedFormat(count, one_or_all_data);
			auto one_or_all = UnifiedVectorFormat::GetData<string_t>(one_or_all_data);
			for (idx_t i = 0; i < count; i++) {
				auto idx = one_or_all_data.sel->get_index(i);
				if (!one_or_all_data.validity.RowIsValid(idx)) {
					result_is_null[i] = true;
					continue;
				}
				auto tmp = one_or_all[idx].GetString();
				if (strcasecmp(tmp.c_str(), "all") == 0) {
					one_or_all_cached[i] = false;
					result_data[i] = true;
				} else if (strcasecmp(tmp.c_str(), "one") == 0) {
					one_or_all_cached[i] = true;
					result_data[i] = false;
				} else {
					throw ParameterNotAllowedException("The oneOrAll argument to json_contains_path may take these values: 'one' or 'all'.");
				}
			}
		}
	}

	vector<vector<const char *>> ptrs(count);
	vector<vector<idx_t>> lens(count);

	for (idx_t col_idx = 2; col_idx < args.ColumnCount(); col_idx++) {
		auto &input = args.data[col_idx];

		// loop over the vector and concat to all results
		if (input.GetVectorType() == VectorType::CONSTANT_VECTOR) {
			// constant vector
			if (ConstantVector::IsNull(input) || result_is_null[0]) {
				result_is_null[0] = true;
				continue;
			}
			// append the constant vector to each of the strings
			auto input_data = ConstantVector::GetData<string_t>(input);
			auto input_ptr = input_data->GetData();
			auto input_len = input_data->GetSize();
			for (idx_t i = 0; i < count; i++) {
				ptrs[i].push_back(input_ptr);
				lens[i].push_back(input_len);
			}
		} else {
			// standard vector
			result.SetVectorType(VectorType::FLAT_VECTOR);
			UnifiedVectorFormat idata;
			input.ToUnifiedFormat(count, idata);

			auto input_data = UnifiedVectorFormat::GetData<string_t>(idata);
			for (idx_t i = 0; i < count; i++) {
				auto idx = idata.sel->get_index(i);
				if (!idata.validity.RowIsValid(idx) || result_is_null[i]) {
					result_is_null[i] = true;
					continue;
				}
				auto input_ptr = input_data[idx].GetData();
				auto input_len = input_data[idx].GetSize();
				ptrs[i].push_back(input_ptr);
				lens[i].push_back(input_len);
			}
		}
	}

	yyjson_val *val;
	auto &json_vector = args.data[0];
	if (json_vector.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		if (ConstantVector::IsNull(json_vector) || result_is_null[0]) {
			result_is_null[0] = true;
		} else {
			auto json = ConstantVector::GetData<string_t>(json_vector);
			auto doc = JSONCommon::ReadDocument(json[0], JSONCommon::READ_FLAG, alc);
			for (idx_t i = 0; i < count; i++) {
				for (idx_t path_i = 0; path_i < ptrs[i].size(); path_i++) {
					try {
						val = JSONCommon::GetUnsafe(doc->root, ptrs[i][path_i], lens[i][path_i]);
					} catch (std::exception &ex) {
						throw InvalidInputException("Invalid JSON path: %s", string(ptrs[i][path_i], lens[i][path_i]));
					}
					bool found = val != nullptr;
					result_data[i] = one_or_all_cached[i] ? result_data[i] || found : result_data[i] && found;
				}
			}
		}
	} else {
		UnifiedVectorFormat input_data;
		json_vector.ToUnifiedFormat(count, input_data);
		auto jsons = UnifiedVectorFormat::GetData<string_t>(input_data);
		for (idx_t i = 0; i < count; i++) {
			auto idx = input_data.sel->get_index(i);
			if (!input_data.validity.RowIsValid(idx)) {
				result_is_null[i] = true;
				continue;
			}

			auto doc = JSONCommon::ReadDocument(jsons[idx], JSONCommon::READ_FLAG, alc);
			for (idx_t path_i = 0; path_i < ptrs[i].size(); path_i++) {
				try {
					val = JSONCommon::GetUnsafe(doc->root, ptrs[i][path_i], lens[i][path_i]);
				} catch (std::exception &ex) {
					throw InvalidInputException("Invalid JSON path: %s", string(ptrs[i][path_i], lens[i][path_i]));
				}
				bool found = val != nullptr;
				result_data[i] = one_or_all_cached[i] ? result_data[i] || found : result_data[i] && found;
			}
		}
	}

	if (result.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		if (result_is_null[0]) {
			auto &result_validity = ConstantVector::Validity(result);
			result_validity.SetAllInvalid(count);
		}
	} else {
		for (idx_t i = 0; i < count; i++) {
			auto &result_validity = FlatVector::Validity(result);
			if (result_is_null[i]) {
				result_validity.SetInvalid(i);
			}
		}
	}
}

static void GetExistsFunctionsInternal(ScalarFunctionSet &set, const LogicalType &input_type) {
	set.AddFunction(ScalarFunction({input_type, LogicalType::VARCHAR}, LogicalType::BOOLEAN, BinaryExistsFunction,
	                               JSONReadFunctionData::Bind, nullptr, nullptr, JSONFunctionLocalState::Init));
	set.AddFunction(ScalarFunction({input_type, LogicalType::LIST(LogicalType::VARCHAR)},
	                               LogicalType::LIST(LogicalType::BOOLEAN), ManyExistsFunction,
	                               JSONReadManyFunctionData::Bind, nullptr, nullptr, JSONFunctionLocalState::Init));
}

static void GetContainsPathFunctionsInternal(ScalarFunctionSet &set, const LogicalType &input_type) {
	ScalarFunction contains_path = ScalarFunction(
	    {input_type, LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::BOOLEAN, ContainsPathFunction,
	    nullptr, nullptr, nullptr, JSONFunctionLocalState::Init);
	contains_path.varargs = LogicalType::VARCHAR;
	set.AddFunction(contains_path);
}

ScalarFunctionSet JSONFunctions::GetExistsFunction() {
	ScalarFunctionSet set("json_exists");
	GetExistsFunctionsInternal(set, LogicalType::VARCHAR);
	GetExistsFunctionsInternal(set, LogicalType::JSON());
	for (auto &func : set.functions) {
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	return set;
}

ScalarFunctionSet JSONFunctions::GetContainsPathFunction() {
	ScalarFunctionSet set("json_contains_path");
	GetContainsPathFunctionsInternal(set, LogicalType::VARCHAR);
	GetContainsPathFunctionsInternal(set, LogicalType::JSON());
	for (auto &func : set.functions) {
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	return set;
}

} // namespace duckdb
