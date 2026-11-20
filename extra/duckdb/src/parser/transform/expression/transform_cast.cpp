#include "duckdb/common/limits.hpp"
#include "duckdb/parser/expression/cast_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/transformer.hpp"
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/common/types/blob.hpp"

namespace duckdb {

unique_ptr<ParsedExpression> Transformer::TransformTypeCast(duckdb_libpgquery::PGTypeCast &root) {
	// get the type to cast to
	auto type_name = root.typeName;
	LogicalType target_type = TransformTypeName(*type_name);

	string cast_type_name;
	if (type_name->names && type_name->names->length > 0) {
		auto name_cell = type_name->names->tail;
		cast_type_name = PGPointerCast<duckdb_libpgquery::PGValue>(name_cell->data.ptr_value)->val.str;
	}
	// Since v1.5.2, TransformTypeName returns UNBOUND, so check the PG type name directly.
	auto type_id = TransformStringToLogicalTypeId(cast_type_name);

	// check for a constant BLOB value, then return ConstantExpression with BLOB
	if (!root.tryCast && type_id == LogicalTypeId::BLOB && root.arg->type == duckdb_libpgquery::T_PGAConst) {
		auto c = PGPointerCast<duckdb_libpgquery::PGAConst>(root.arg);
		if (c->val.type == duckdb_libpgquery::T_PGString) {
			CastParameters parameters;
			if (root.location >= 0) {
				parameters.query_location = NumericCast<idx_t>(root.location);
			}
			auto blob_data = Blob::ToBlob(string(c->val.val.str), parameters);
			auto result = make_uniq<ConstantExpression>(Value::BLOB_RAW(blob_data));
			SetQueryLocation(*result, root.location);
			return std::move(result);
		}
	}

	// transform the expression node
	auto expression = TransformExpression(root.arg);
	bool try_cast = root.tryCast;

	// now create a cast operation
	auto result = make_uniq<CastExpression>(target_type, std::move(expression), try_cast);
	SetQueryLocation(*result, root.location);
	// When casting to CHAR(N)/VARCHAR(N), DuckDB ignores the length modifier.
	// To match MySQL semantics, wrap in LEFT(cast_result, N) to truncate.
	if (type_name->typmods) {
		if (type_id == LogicalTypeId::VARCHAR) {
			std::vector<unique_ptr<ParsedExpression>> children;
			auto &const_val = *Transformer::PGPointerCast<duckdb_libpgquery::PGAConst>(type_name->typmods->head->data.ptr_value);
			children.push_back(std::move(result));
			children.push_back(std::move(make_uniq<ConstantExpression>(Value::BIGINT(const_val.val.val.ival))));
			auto function = make_uniq<FunctionExpression>("left", std::move(children));
			return std::move(function);
		}
	}
	return std::move(result);
}

} // namespace duckdb
