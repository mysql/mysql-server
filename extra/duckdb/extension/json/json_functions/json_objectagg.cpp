#include "json_common.hpp"
#include "json_functions.hpp"

#include "duckdb/function/aggregate_function.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

namespace {

// Ordered map that preserves insertion order and supports last-key-wins updates
struct OrderedKeyMap {
	vector<pair<string, string>> entries;
	unordered_map<string, idx_t> key_index;

	void InsertOrUpdate(string key, string value) {
		auto it = key_index.find(key);
		if (it != key_index.end()) {
			entries[it->second].second = std::move(value);
		} else {
			key_index[key] = entries.size();
			entries.emplace_back(std::move(key), std::move(value));
		}
	}
};

struct JSONObjectAggState {
	OrderedKeyMap *map;
};

static void JSONEscapeString(string &result, const string_t &str) {
	result += "\"";
	auto data = str.GetData();
	auto size = str.GetSize();
	for (idx_t i = 0; i < size; i++) {
		auto c = data[i];
		switch (c) {
		case '"':
			result += "\\\"";
			break;
		case '\\':
			result += "\\\\";
			break;
		case '\b':
			result += "\\b";
			break;
		case '\f':
			result += "\\f";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\t':
			result += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
				result += buf;
			} else {
				result += c;
			}
			break;
		}
	}
	result += "\"";
}

// Determine if a type needs JSON string quoting
static bool NeedsJSONQuoting(const LogicalType &type) {
	switch (type.id()) {
	case LogicalTypeId::BOOLEAN:
	case LogicalTypeId::TINYINT:
	case LogicalTypeId::SMALLINT:
	case LogicalTypeId::INTEGER:
	case LogicalTypeId::BIGINT:
	case LogicalTypeId::UTINYINT:
	case LogicalTypeId::USMALLINT:
	case LogicalTypeId::UINTEGER:
	case LogicalTypeId::UBIGINT:
	case LogicalTypeId::HUGEINT:
	case LogicalTypeId::UHUGEINT:
	case LogicalTypeId::FLOAT:
	case LogicalTypeId::DOUBLE:
	case LogicalTypeId::DECIMAL:
		return false;
	default:
		if (type.IsJSONType()) {
			return false;
		}
		return true;
	}
}

// Bind data to store ClientContext reference for timezone-aware casts
struct JSONObjectAggBindData : public FunctionData {
	ClientContext &context;

	explicit JSONObjectAggBindData(ClientContext &context_p) : context(context_p) {}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<JSONObjectAggBindData>(context);
	}

	bool Equals(const FunctionData &other_p) const override {
		return true;
	}
};

// Check if a type needs microsecond precision padding (6 decimal places)
static bool NeedsPrecisionPadding(const LogicalType &type) {
	switch (type.id()) {
	case LogicalTypeId::TIME:
	case LogicalTypeId::TIME_TZ:
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_TZ:
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_MS:
		return true;
	default:
		return false;
	}
}

// Ensure the string has exactly 6 decimal places for microsecond precision
static string PadMicrosecondPrecision(const string_t &str) {
	string s = str.GetString();
	if (s == "infinity" || s == "-infinity") {
		return s;
	}
	auto dot_pos = s.rfind('.');
	if (dot_pos == string::npos) {
		s += ".000000";
	} else {
		auto decimals = s.size() - dot_pos - 1;
		if (decimals < 6) {
			s.append(6 - decimals, '0');
		}
	}
	return s;
}

struct JSONObjectAggFunction {
	template <class STATE>
	static void Initialize(STATE &state) {
		state.map = nullptr;
	}

	template <class STATE, class OP>
	static void Combine(const STATE &source, STATE &target, AggregateInputData &) {
		if (!source.map) {
			return;
		}
		if (!target.map) {
			target.map = new OrderedKeyMap();
		}
		for (auto &entry : source.map->entries) {
			target.map->InsertOrUpdate(entry.first, entry.second);
		}
	}

	template <class T, class STATE>
	static void Finalize(STATE &state, T &target, AggregateFinalizeData &finalize_data) {
		if (!state.map || state.map->entries.empty()) {
			finalize_data.ReturnNull();
			return;
		}

		string result = "{";
		bool first = true;
		for (auto &entry : state.map->entries) {
			if (!first) {
				result += ", ";
			}
			first = false;
			result += entry.first;
			result += ": ";
			result += entry.second;
		}
		result += "}";
		target = StringVector::AddString(finalize_data.result, result);
	}

	template <class STATE>
	static void Destroy(STATE &state, AggregateInputData &) {
		if (state.map) {
			delete state.map;
			state.map = nullptr;
		}
	}

	static bool IgnoreNull() {
		return false;
	}
};

static void ProcessRows(Vector &keys, Vector &values, idx_t count, ClientContext &context,
                        const std::function<JSONObjectAggState &(idx_t)> &get_state) {
	// Determine if values need quoting based on the RUNTIME vector type
	bool quote_values = NeedsJSONQuoting(values.GetType());
	bool pad_value_precision = NeedsPrecisionPadding(values.GetType());

	// Convert keys to VARCHAR using context-aware cast (for timestamptz timezone support)
	Vector key_varchar(LogicalType::VARCHAR, count);
	VectorOperations::Cast(context, keys, key_varchar, count);

	// Convert values to VARCHAR using context-aware cast (for timestamptz timezone support)
	Vector val_varchar(LogicalType::VARCHAR, count);
	VectorOperations::Cast(context, values, val_varchar, count);

	UnifiedVectorFormat key_data, val_data;
	key_varchar.ToUnifiedFormat(count, key_data);
	val_varchar.ToUnifiedFormat(count, val_data);

	auto key_strings = UnifiedVectorFormat::GetData<string_t>(key_data);
	auto val_strings = UnifiedVectorFormat::GetData<string_t>(val_data);

	for (idx_t i = 0; i < count; i++) {
		auto kidx = key_data.sel->get_index(i);
		auto vidx = val_data.sel->get_index(i);

		if (!key_data.validity.RowIsValid(kidx)) {
			throw InvalidInputException("json_objectagg does not allow NULL as key");
		}

		auto &state = get_state(i);
		if (!state.map) {
			state.map = new OrderedKeyMap();
		}

		// JSON-escape the key
		string json_key;
		JSONEscapeString(json_key, key_strings[kidx]);

		if (!val_data.validity.RowIsValid(vidx)) {
			state.map->InsertOrUpdate(std::move(json_key), "null");
		} else if (quote_values) {
			string json_val;
			if (pad_value_precision) {
				string padded = PadMicrosecondPrecision(val_strings[vidx]);
				JSONEscapeString(json_val, string_t(padded));
			} else {
				JSONEscapeString(json_val, val_strings[vidx]);
			}
			state.map->InsertOrUpdate(std::move(json_key), std::move(json_val));
		} else {
			state.map->InsertOrUpdate(std::move(json_key), val_strings[vidx].GetString());
		}
	}
}

static void JSONObjectAggScatter(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count,
                                 Vector &states, idx_t count) {
	auto &bind_data = aggr_input_data.bind_data->Cast<JSONObjectAggBindData>();
	UnifiedVectorFormat state_data;
	states.ToUnifiedFormat(count, state_data);

	ProcessRows(inputs[0], inputs[1], count, bind_data.context, [&](idx_t i) -> JSONObjectAggState & {
		auto sidx = state_data.sel->get_index(i);
		return *reinterpret_cast<JSONObjectAggState **>(state_data.data)[sidx];
	});
}

static void JSONObjectAggSimpleUpdate(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count,
                                      data_ptr_t state_p, idx_t count) {
	auto &bind_data = aggr_input_data.bind_data->Cast<JSONObjectAggBindData>();
	auto &state = *reinterpret_cast<JSONObjectAggState *>(state_p);
	ProcessRows(inputs[0], inputs[1], count, bind_data.context, [&](idx_t) -> JSONObjectAggState & {
		return state;
	});
}

unique_ptr<FunctionData> JSONObjectAggBind(ClientContext &context, AggregateFunction &function,
                                           vector<unique_ptr<Expression>> &arguments) {
	// Store ClientContext for timezone-aware casts at runtime
	return make_uniq<JSONObjectAggBindData>(context);
}

} // namespace

AggregateFunctionSet JSONFunctions::GetObjectAggFunction() {
	AggregateFunctionSet json_objectagg("json_objectagg");
	AggregateFunction fun(
	    {LogicalType::ANY, LogicalType::ANY}, LogicalType::JSON(),
	    AggregateFunction::StateSize<JSONObjectAggState>,
	    AggregateFunction::StateInitialize<JSONObjectAggState, JSONObjectAggFunction, AggregateDestructorType::LEGACY>,
	    JSONObjectAggScatter,
	    AggregateFunction::StateCombine<JSONObjectAggState, JSONObjectAggFunction>,
	    AggregateFunction::StateFinalize<JSONObjectAggState, string_t, JSONObjectAggFunction>,
	    FunctionNullHandling::SPECIAL_HANDLING,
	    JSONObjectAggSimpleUpdate,
	    JSONObjectAggBind,
	    AggregateFunction::StateDestroy<JSONObjectAggState, JSONObjectAggFunction>);
	json_objectagg.AddFunction(fun);
	return json_objectagg;
}

} // namespace duckdb
