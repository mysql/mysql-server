#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/planner/expression/bound_cast_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/expression/bound_parameter_expression.hpp"
#include "duckdb/planner/expression_binder.hpp"
#include "duckdb/catalog/catalog_entry/collate_catalog_entry.hpp"
#include "duckdb/common/string_util.hpp"

#include "duckdb/function/scalar/string_functions.hpp"

#include "duckdb/common/types/decimal.hpp"
#include "duckdb/planner/collation_binding.hpp"

#include "duckdb/main/client_config.hpp"
#include "duckdb/main/settings.hpp"

namespace duckdb {

bool ExpressionBinder::PushCollation(ClientContext &context, unique_ptr<Expression> &source,
                                     const LogicalType &sql_type, CollationType type) {
	auto &collation_binding = CollationBinding::Get(context);
	return collation_binding.PushCollation(context, source, sql_type, type);
}

void ExpressionBinder::TestCollation(ClientContext &context, const string &collation) {
	auto expr = make_uniq_base<Expression, BoundConstantExpression>(Value(""));
	PushCollation(context, expr, LogicalType::VARCHAR_COLLATION(collation));
}

static bool SwitchVarcharComparison(const LogicalType &type) {
	switch (type.id()) {
	case LogicalTypeId::BOOLEAN:
	case LogicalTypeId::TINYINT:
	case LogicalTypeId::SMALLINT:
	case LogicalTypeId::INTEGER:
	case LogicalTypeId::BIGINT:
	case LogicalTypeId::HUGEINT:
	case LogicalTypeId::FLOAT:
	case LogicalTypeId::DOUBLE:
	case LogicalTypeId::DECIMAL:
	case LogicalTypeId::UTINYINT:
	case LogicalTypeId::USMALLINT:
	case LogicalTypeId::UINTEGER:
	case LogicalTypeId::UBIGINT:
	case LogicalTypeId::UHUGEINT:
	case LogicalTypeId::DATE:
	case LogicalTypeId::TIME:
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_MS:
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_NS:
	case LogicalTypeId::INTERVAL:
	case LogicalTypeId::TIMESTAMP_TZ:
	case LogicalTypeId::TIME_TZ:
	case LogicalTypeId::INTEGER_LITERAL:
		return true;
	default:
		return false;
	}
}

bool BoundComparisonExpression::TryBindComparison(ClientContext &context, const LogicalType &left_type,
                                                  const LogicalType &right_type, LogicalType &result_type,
                                                  ExpressionType comparison_type) {
	LogicalType res;
	bool is_equality;
	switch (comparison_type) {
	case ExpressionType::COMPARE_EQUAL:
	case ExpressionType::COMPARE_NOTEQUAL:
	case ExpressionType::COMPARE_IN:
	case ExpressionType::COMPARE_NOT_IN:
	case ExpressionType::COMPARE_DISTINCT_FROM:
	case ExpressionType::COMPARE_NOT_DISTINCT_FROM:
		is_equality = true;
		break;
	default:
		is_equality = false;
		break;
	}
	if (((left_type.IsNumeric() || left_type.id() == LogicalTypeId::INTEGER_LITERAL) && right_type.IsTemporal()) ||
	    (left_type.IsTemporal() && (right_type.IsNumeric() || right_type.id() == LogicalTypeId::INTEGER_LITERAL))) {
		result_type = LogicalType::DOUBLE;
		return true;
	}
	//
	if ((left_type.id() == LogicalTypeId::VARCHAR && right_type.id() == LogicalTypeId::BLOB_LITERAL)) {
		result_type = left_type;
		return true;
	} else if (left_type.id() == LogicalTypeId::BLOB_LITERAL && right_type.id() == LogicalTypeId::VARCHAR) {
		result_type = right_type;
		return true;
	} else if ((left_type.id() == LogicalTypeId::VARCHAR && right_type.id() == LogicalTypeId::BLOB) ||
	           (right_type.id() == LogicalTypeId::VARCHAR && left_type.id() == LogicalTypeId::BLOB)) {
		result_type = LogicalType::BLOB;
		return true;
	}
	if ((left_type.id() == LogicalTypeId::BIT &&
	     (right_type.IsNumeric() || right_type.id() == LogicalTypeId::INTEGER_LITERAL ||
	      right_type.id() == LogicalTypeId::BLOB || right_type.id() == LogicalTypeId::BLOB_LITERAL ||
	      right_type.id() == LogicalTypeId::BOOLEAN)) ||
	    (right_type.id() == LogicalTypeId::BIT &&
	     (left_type.IsNumeric() || left_type.id() == LogicalTypeId::INTEGER_LITERAL ||
	      left_type.id() == LogicalTypeId::BLOB || left_type.id() == LogicalTypeId::BLOB_LITERAL ||
	      left_type.id() == LogicalTypeId::BOOLEAN))) {
		result_type = LogicalType::HUGEINT;
		return true;
	}
	if (is_equality) {
		res = LogicalType::ForceMaxLogicalType(left_type, right_type);
	} else {
		if (!LogicalType::TryGetMaxLogicalType(context, left_type, right_type, res)) {
			return false;
		}
	}
	switch (res.id()) {
	case LogicalTypeId::DECIMAL: {
		// result is a decimal: we need the maximum width and the maximum scale over width
		vector<LogicalType> argument_types = {left_type, right_type};
		uint8_t max_width = 0, max_scale = 0, max_width_over_scale = 0;
		for (idx_t i = 0; i < argument_types.size(); i++) {
			uint8_t width, scale;
			auto can_convert = argument_types[i].GetDecimalProperties(width, scale);
			if (!can_convert) {
				result_type = res;
				return true;
			}
			max_width = MaxValue<uint8_t>(width, max_width);
			max_scale = MaxValue<uint8_t>(scale, max_scale);
			max_width_over_scale = MaxValue<uint8_t>(width - scale, max_width_over_scale);
		}
		max_width = MaxValue<uint8_t>(max_scale + max_width_over_scale, max_width);
		if (max_width > Decimal::MAX_WIDTH_DECIMAL) {
			// target width does not fit in decimal: truncate the scale (if possible) to try and make it fit
			max_width = Decimal::MAX_WIDTH_DECIMAL;
		}
		res = LogicalType::DECIMAL(max_width, max_scale);
		break;
	}
	case LogicalTypeId::VARCHAR:
		// for comparison with strings, we prefer to bind to the numeric types
		if (left_type.id() != LogicalTypeId::VARCHAR && SwitchVarcharComparison(left_type)) {
			res = LogicalType::NormalizeType(left_type);
		} else if (right_type.id() != LogicalTypeId::VARCHAR && SwitchVarcharComparison(right_type)) {
			res = LogicalType::NormalizeType(right_type);
		} else {
			// else: check if collations are compatible
			auto res_collation = StringType::GetCollation(res);
			if (strcasecmp(res_collation.c_str(), "posix") == 0 || strcasecmp(res_collation.c_str(), "binary") == 0) {
				break;
			}
			auto left_collation = StringType::GetCollation(left_type);
			auto right_collation = StringType::GetCollation(right_type);
			if (!left_collation.empty() && !right_collation.empty() && left_collation != right_collation &&
			    !ClientConfig::GetConfig(context).force_no_collation) {
				throw BinderException("Cannot combine types with different collation!");
			}
		}
		break;
	default:
		break;
	}
	result_type = res;
	return true;
}

LogicalType BoundComparisonExpression::BindComparison(ClientContext &context, const LogicalType &left_type,
                                                      const LogicalType &right_type, ExpressionType comparison_type) {
	LogicalType result_type;
	if (!BoundComparisonExpression::TryBindComparison(context, left_type, right_type, result_type, comparison_type)) {
		throw BinderException("Cannot mix values of type %s and %s - an explicit cast is required",
		                      left_type.ToString(), right_type.ToString());
	}
	return result_type;
}

LogicalType ExpressionBinder::GetExpressionReturnType(const Expression &expr) {
	if (expr.GetExpressionClass() == ExpressionClass::BOUND_CONSTANT) {
		if (expr.return_type == LogicalTypeId::VARCHAR && StringType::GetCollation(expr.return_type).empty()) {
			return LogicalTypeId::STRING_LITERAL;
		}
		if (expr.return_type.IsIntegral()) {
			auto &constant = expr.Cast<BoundConstantExpression>();
			if (!constant.value.IsNull()) {
				return LogicalType::INTEGER_LITERAL(constant.value);
			}
		}
	}
	if (expr.return_type == LogicalTypeId::BLOB && expr.IsFoldable()) {
		return LogicalTypeId::BLOB_LITERAL;
	}
	return expr.return_type;
}

BindResult ExpressionBinder::BindExpression(ComparisonExpression &expr, idx_t depth) {
	// first try to bind the children of the case expression
	ErrorData error;
	BindChild(expr.left, depth, error);
	BindChild(expr.right, depth, error);
	if (error.HasError()) {
		return BindResult(std::move(error));
	}

	// the children have been successfully resolved
	auto &left = BoundExpression::GetExpression(*expr.left);
	auto &right = BoundExpression::GetExpression(*expr.right);
	auto left_sql_type = ExpressionBinder::GetExpressionReturnType(*left);
	auto right_sql_type = ExpressionBinder::GetExpressionReturnType(*right);
	// cast the input types to the same type
	// now obtain the result type of the input types
	LogicalType input_type;
	if (!BoundComparisonExpression::TryBindComparison(context, left_sql_type, right_sql_type, input_type,
	                                                  expr.GetExpressionType())) {
		return BindResult(BinderException(expr,
		                                  "Cannot compare values of type %s and type %s - an explicit cast is required",
		                                  left_sql_type.ToString(), right_sql_type.ToString()));
	}

	// Aone 65335969
	if ((left_sql_type.id() == LogicalTypeId::DATE && right_sql_type.id() == LogicalTypeId::STRING_LITERAL) ||
		(left_sql_type.id() == LogicalTypeId::STRING_LITERAL && right_sql_type.id() == LogicalTypeId::DATE)) {
		input_type = LogicalTypeId::TIMESTAMP;
	}

	// Aone 66336888. The rules for comparing time and timestamp(tz) in MySQL is ambiguity, so we do not allow comparisons.
	if ((left_sql_type.id() == LogicalTypeId::TIME && 
			(right_sql_type.id() == LogicalTypeId::TIMESTAMP || right_sql_type.id() == LogicalTypeId::TIMESTAMP_TZ)) ||
		((right_sql_type.id() == LogicalTypeId::TIME) &&
			(left_sql_type.id() == LogicalTypeId::TIMESTAMP || left_sql_type.id() == LogicalTypeId::TIMESTAMP_TZ))) {
		return BindResult(BinderException(expr,
											"Cannot compare values of type %s and type %s - an explicit cast is required",
											left_sql_type.ToString(), right_sql_type.ToString()));
	}

	// When comparing varchar and json, we add double quotes.
	if (left_sql_type.id() == LogicalTypeId::VARCHAR && left_sql_type.GetAlias() == "JSON" &&
	    (right_sql_type.id() == LogicalTypeId::STRING_LITERAL || right_sql_type.id() == LogicalTypeId::VARCHAR)) {
		vector<unique_ptr<Expression>> children;
		LogicalType return_type = right->return_type;
		children.push_back(std::move(make_uniq<BoundConstantExpression>(Value(std::string("\"")))));
		children.push_back(std::move(right));
		children.push_back(std::move(make_uniq<BoundConstantExpression>(Value(std::string("\"")))));
		auto concat_func = ConcatFun::GetFunction();
		auto concat_bind_info = concat_func.bind(context, concat_func, children);
		auto result = make_uniq<BoundFunctionExpression>(return_type, std::move(concat_func), std::move(children),
		                                                 std::move(concat_bind_info));
		right = std::move(result);
	}

	if (right_sql_type.id() == LogicalTypeId::VARCHAR && right_sql_type.GetAlias() == "JSON" &&
	    (left_sql_type.id() == LogicalTypeId::STRING_LITERAL || left_sql_type.id() == LogicalTypeId::VARCHAR)) {
		vector<unique_ptr<Expression>> children;
		LogicalType return_type = left->return_type;
		children.push_back(std::move(make_uniq<BoundConstantExpression>(Value(std::string("\"")))));
		children.push_back(std::move(left));
		children.push_back(std::move(make_uniq<BoundConstantExpression>(Value(std::string("\"")))));
		auto concat_func = ConcatFun::GetFunction();
		auto concat_bind_info = concat_func.bind(context, concat_func, children);
		auto result = make_uniq<BoundFunctionExpression>(return_type, std::move(concat_func), std::move(children),
		                                                 std::move(concat_bind_info));
		left = std::move(result);
	}

	// add casts (if necessary)
	// In MySQL, Comparing BLOB_LITERAL and BIT requires first converting to BIT type, and then to HUGEINT type.
	// BLOB_LITERAL -> HUGEINT: BLOB_LITERAL -> BIT ->HUGEINT
	if (left_sql_type.id() == LogicalTypeId::BLOB_LITERAL && input_type.id() == LogicalTypeId::HUGEINT) {
		left = BoundCastExpression::AddCastToType(context, std::move(left), LogicalType(LogicalTypeId::BIT),
		                                          input_type.id() == LogicalTypeId::ENUM);
	}
	left = BoundCastExpression::AddCastToType(context, std::move(left), input_type,
	                                          input_type.id() == LogicalTypeId::ENUM);
	if (right_sql_type.id() == LogicalTypeId::BLOB_LITERAL && input_type.id() == LogicalTypeId::HUGEINT) {
		right = BoundCastExpression::AddCastToType(context, std::move(right), LogicalType(LogicalTypeId::BIT),
		                                          input_type.id() == LogicalTypeId::ENUM);
	}
	right = BoundCastExpression::AddCastToType(context, std::move(right), input_type,
	                                           input_type.id() == LogicalTypeId::ENUM);

	PushCollation(context, left, input_type);
	PushCollation(context, right, input_type);

	// now create the bound comparison expression
	return BindResult(
	    make_uniq<BoundComparisonExpression>(expr.GetExpressionType(), std::move(left), std::move(right)));
}

} // namespace duckdb
