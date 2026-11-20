#include "json_executors.hpp"

namespace duckdb {

static inline bool JSONContains(yyjson_val *haystack, yyjson_val *needle);
static inline bool JSONFuzzyEquals(yyjson_val *haystack, yyjson_val *needle);

static inline bool JSONArrayFuzzyEquals(yyjson_val *haystack, yyjson_val *needle) {
	D_ASSERT(yyjson_get_tag(haystack) == (YYJSON_TYPE_ARR | YYJSON_SUBTYPE_NONE) &&
	         yyjson_get_tag(needle) == (YYJSON_TYPE_ARR | YYJSON_SUBTYPE_NONE));

	size_t needle_idx, needle_max, haystack_idx, haystack_max;
	yyjson_val *needle_child, *haystack_child;
	yyjson_arr_foreach(needle, needle_idx, needle_max, needle_child) {
		bool found = false;
		yyjson_arr_foreach(haystack, haystack_idx, haystack_max, haystack_child) {
			if (JSONFuzzyEquals(haystack_child, needle_child)) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

static inline bool JSONObjectFuzzyEquals(yyjson_val *haystack, yyjson_val *needle) {
	D_ASSERT(yyjson_get_tag(haystack) == (YYJSON_TYPE_OBJ | YYJSON_SUBTYPE_NONE) &&
	         yyjson_get_tag(needle) == (YYJSON_TYPE_OBJ | YYJSON_SUBTYPE_NONE));

	size_t idx, max;
	yyjson_val *key, *needle_child;
	yyjson_obj_foreach(needle, idx, max, key, needle_child) {
		auto haystack_child = yyjson_obj_getn(haystack, unsafe_yyjson_get_str(key), unsafe_yyjson_get_len(key));
		if (!haystack_child || !JSONFuzzyEquals(haystack_child, needle_child)) {
			return false;
		}
	}
	return true;
}

static inline bool JSONFuzzyEquals(yyjson_val *haystack, yyjson_val *needle) {
	D_ASSERT(haystack && needle);

	// Strict equality
	yyjson_type type = unsafe_yyjson_get_type(haystack);
    if (type != unsafe_yyjson_get_type(needle)) {
		if (type == YYJSON_TYPE_ARR && unsafe_yyjson_get_type(needle) != YYJSON_TYPE_ARR) {
			auto lhs_tmp = unsafe_yyjson_get_first(haystack);
			idx_t lhs_len = unsafe_yyjson_get_len(haystack);
			if (lhs_len == 0) {
				return false;
			}
			while (lhs_len > 0) {
				if (JSONFuzzyEquals(lhs_tmp, needle)) break;
				lhs_len--;
				if (lhs_len == 0) return false;
				lhs_tmp = unsafe_yyjson_get_next(lhs_tmp);
			}
			return true;
		}
		return false;
	}

    switch (type) {
        case YYJSON_TYPE_OBJ: {
            idx_t len = unsafe_yyjson_get_len(needle);
            // if (len != unsafe_yyjson_get_len(rhs)) return false;
            if (len > 0) {
                yyjson_obj_iter iter;
                yyjson_obj_iter_init(haystack, &iter);
                auto rhs = unsafe_yyjson_get_first(needle);
                while (len-- > 0) {
                    auto lhs = yyjson_obj_iter_getn(&iter, rhs->uni.str,
                                               unsafe_yyjson_get_len(rhs));
                    if (!lhs) return false;
                    if (!JSONFuzzyEquals(lhs, rhs + 1)) return false;
                    rhs = unsafe_yyjson_get_next(rhs + 1);
                }
            }
            /* yyjson allows duplicate keys, so the check may be inaccurate */
            return true;
        }

        case YYJSON_TYPE_ARR: {
            idx_t lhs_len = unsafe_yyjson_get_len(haystack);
            idx_t rhs_len = unsafe_yyjson_get_len(needle);
			if (lhs_len == 0 && rhs_len > 0) {
				return false;
			}
			if (lhs_len >= 0 && rhs_len == 0) {
				return true;
			}
            // if (len != unsafe_yyjson_get_len(rhs)) return false;
            auto rhs = unsafe_yyjson_get_first(needle);
            while(true) {
                auto lhs_tmp = unsafe_yyjson_get_first(haystack);
                while (lhs_len > 0) {
                    if (JSONFuzzyEquals(lhs_tmp, rhs)) break;
                    lhs_len--;
                    if (lhs_len == 0) return false;
                    lhs_tmp = unsafe_yyjson_get_next(lhs_tmp);
                }
                rhs_len--;
                if (rhs_len == 0) return true;
                rhs = unsafe_yyjson_get_next(rhs);
            }
            return true;
        }

        case YYJSON_TYPE_NUM:
            return unsafe_yyjson_equals(haystack, needle);

        case YYJSON_TYPE_RAW:
        case YYJSON_TYPE_STR:
            return unsafe_yyjson_equals(haystack, needle);

        case YYJSON_TYPE_NULL:
        case YYJSON_TYPE_BOOL:
            return haystack->tag == needle->tag;

        default:
            return false;
    }
}

static inline bool JSONArrayContains(yyjson_val *haystack_array, yyjson_val *needle) {
	D_ASSERT(yyjson_get_tag(haystack_array) == (YYJSON_TYPE_ARR | YYJSON_SUBTYPE_NONE));

	size_t idx, max;
	yyjson_val *child_haystack;
	yyjson_arr_foreach(haystack_array, idx, max, child_haystack) {
		if (JSONContains(child_haystack, needle)) {
			return true;
		}
	}
	return false;
}

static inline bool JSONObjectContains(yyjson_val *haystack_object, yyjson_val *needle) {
	D_ASSERT(yyjson_get_tag(haystack_object) == (YYJSON_TYPE_OBJ | YYJSON_SUBTYPE_NONE));

	size_t idx, max;
	yyjson_val *key, *child_haystack;
	yyjson_obj_foreach(haystack_object, idx, max, key, child_haystack) {
		if (JSONContains(child_haystack, needle)) {
			return true;
		}
	}
	return false;
}

static inline bool JSONContains(yyjson_val *haystack, yyjson_val *needle) {
	if (JSONFuzzyEquals(haystack, needle)) {
		return true;
	}

	// In MySQL, we will not traverse all paths.
	return false;

	switch (yyjson_get_tag(haystack)) {
	case YYJSON_TYPE_ARR | YYJSON_SUBTYPE_NONE:
		return JSONArrayContains(haystack, needle);
	case YYJSON_TYPE_OBJ | YYJSON_SUBTYPE_NONE:
		return JSONObjectContains(haystack, needle);
	default:
		return false;
	}
}

static void JSONContainsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.data.size() == 2);
	auto &lstate = JSONFunctionLocalState::ResetAndGet(state);
	auto alc = lstate.json_allocator->GetYYAlc();

	auto &haystacks = args.data[0];
	auto &needles = args.data[1];

	if (needles.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		if (ConstantVector::IsNull(needles)) {
			result.SetVectorType(VectorType::CONSTANT_VECTOR);
			ConstantVector::SetNull(result, true);
			return;
		}
		auto &needle_str = *ConstantVector::GetData<string_t>(needles);
		auto needle_doc = JSONCommon::ReadDocument(needle_str, JSONCommon::READ_FLAG, alc);
		UnaryExecutor::Execute<string_t, bool>(haystacks, result, args.size(), [&](string_t haystack_str) {
			auto haystack_doc = JSONCommon::ReadDocument(haystack_str, JSONCommon::READ_FLAG, alc);
			return JSONContains(haystack_doc->root, needle_doc->root);
		});
	} else {
		BinaryExecutor::Execute<string_t, string_t, bool>(
		    haystacks, needles, result, args.size(), [&](string_t haystack_str, string_t needle_str) {
			    auto needle_doc = JSONCommon::ReadDocument(needle_str, JSONCommon::READ_FLAG, alc);
			    auto haystack_doc = JSONCommon::ReadDocument(haystack_str, JSONCommon::READ_FLAG, alc);
			    return JSONContains(haystack_doc->root, needle_doc->root);
		    });
	}
}

static void GetContainsFunctionInternal(ScalarFunctionSet &set, const LogicalType &lhs, const LogicalType &rhs) {
	set.AddFunction(ScalarFunction({lhs, rhs}, LogicalType::BOOLEAN, JSONContainsFunction, nullptr, nullptr, nullptr,
	                               JSONFunctionLocalState::Init));
}

ScalarFunctionSet JSONFunctions::GetContainsFunction() {
	ScalarFunctionSet set("json_contains_duckdb");
	GetContainsFunctionInternal(set, LogicalType::VARCHAR, LogicalType::VARCHAR);
	GetContainsFunctionInternal(set, LogicalType::VARCHAR, LogicalType::JSON());
	GetContainsFunctionInternal(set, LogicalType::JSON(), LogicalType::VARCHAR);
	for (auto &func : set.functions) {
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	GetContainsFunctionInternal(set, LogicalType::JSON(), LogicalType::JSON());
	// TODO: implement json_contains that accepts path argument as well
	for (auto &func : set.functions) {
		func.errors = FunctionErrors::CAN_THROW_RUNTIME_ERROR;
	}
	return set;
}

} // namespace duckdb
