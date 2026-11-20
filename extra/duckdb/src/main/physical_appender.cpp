#include "duckdb/main/physical_appender.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/column/column_data_collection.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/planner/binder.hpp"
#include "duckdb/planner/expression_binder/constant_binder.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/storage/data_table.hpp"
#include "duckdb/transaction/meta_transaction.hpp"

namespace duckdb {

//===--------------------------------------------------------------------===//
// Physical Appender
//===--------------------------------------------------------------------===//
PhysicalAppender::PhysicalAppender(Connection &con, const string &database_name, const string &schema_name,
                                   const string &table_name, AppenderType appender_type,
                                   const idx_t flush_memory_threshold_p)
    : BaseAppender(Allocator::DefaultAllocator(), appender_type), context(con.context) {

	flush_memory_threshold = (flush_memory_threshold_p == DConstants::INVALID_INDEX)
	                             ? optional_idx::Invalid()
	                             : optional_idx(flush_memory_threshold_p);

	description = con.TableInfo(database_name, schema_name, table_name);
	if (!description) {
		throw CatalogException(
		    StringUtil::Format("Table \"%s.%s.%s\" could not be found", database_name, schema_name, table_name));
	}
	if (description->readonly) {
		throw InvalidInputException("Cannot append to a readonly database.");
	}

	vector<optional_ptr<const ParsedExpression>> defaults;
	for (auto &column : description->columns) {
		if (column.Generated()) {
			continue;
		}
		types.push_back(column.Type());
		defaults.push_back(column.HasDefaultValue() ? &column.DefaultValue() : nullptr);
	}
	auto &context_ref = *con.context;
	auto binder = Binder::CreateBinder(context_ref);
	context_ref.RunFunctionInTransaction([&]() {
		for (idx_t i = 0; i < types.size(); i++) {
			auto &type = types[i];
			auto &expr = defaults[i];

			if (!expr) {
				default_values[i] = Value(type);
				continue;
			}

			auto default_copy = expr->Copy();
			D_ASSERT(!default_copy->HasParameter());

			ConstantBinder default_binder(*binder, context_ref, "DEFAULT value");
			default_binder.target_type = type;
			auto bound_default = default_binder.Bind(default_copy);

			if (!bound_default->IsFoldable()) {
				continue;
			}

			Value result_value;
			auto eval_success = ExpressionExecutor::TryEvaluateScalar(context_ref, *bound_default, result_value);
			if (eval_success) {
				default_values[i] = result_value;
			}
		}
	});

	InitializeChunk();
	collection = make_uniq<ColumnDataCollection>(allocator, GetActiveTypes());
}

PhysicalAppender::PhysicalAppender(Connection &con, const string &schema_name, const string &table_name,
                                   AppenderType appender_type, const idx_t flush_memory_threshold_p)
    : PhysicalAppender(con, INVALID_CATALOG, schema_name, table_name, AppenderType::PHYSICAL,
                       flush_memory_threshold_p) {
}

PhysicalAppender::PhysicalAppender(Connection &con, const string &table_name, AppenderType appender_type,
                                   const idx_t flush_memory_threshold_p)
    : PhysicalAppender(con, INVALID_CATALOG, DEFAULT_SCHEMA, table_name, AppenderType::PHYSICAL,
                       flush_memory_threshold_p) {
}

PhysicalAppender::~PhysicalAppender() {
	Destructor();
}

void PhysicalAppender::FlushInternal(ColumnDataCollection &collection) {
	auto context_ref = context.lock();
	if (!context_ref) {
		throw InvalidInputException("Appender: Attempting to flush data to a closed connection");
	}
	context_ref->RunFunctionInTransaction([&]() {
		auto &table_entry = Catalog::GetEntry<TableCatalogEntry>(*context_ref, description->database,
		                                                          description->schema, description->table);
		// Verify that the table columns and types match up
		if (description->PhysicalColumnCount() != table_entry.GetColumns().PhysicalColumnCount()) {
			throw InvalidInputException("Failed to append: table entry has different number of columns!");
		}
		idx_t table_entry_col_idx = 0;
		for (idx_t i = 0; i < description->columns.size(); i++) {
			auto &column = description->columns[i];
			if (column.Generated()) {
				continue;
			}
			if (column.Type() != table_entry.GetColumns().GetColumn(PhysicalIndex(table_entry_col_idx)).Type()) {
				throw InvalidInputException("Failed to append: table entry has different column types!");
			}
			table_entry_col_idx++;
		}
		auto binder = Binder::CreateBinder(*context_ref);
		auto bound_constraints = binder->BindConstraints(table_entry);
		MetaTransaction::Get(*context_ref)
		    .ModifyDatabase(table_entry.ParentCatalog().GetAttached(), DatabaseModificationType::INSERT_DATA);
		auto col_ids_ptr = column_ids.empty() ? nullptr : &column_ids;
		table_entry.GetStorage().LocalAppend(table_entry, *context_ref, collection, bound_constraints, col_ids_ptr);
	});
}

void PhysicalAppender::AppendDefault() {
	auto value = GetDefaultValue(column);
	Append(value);
}

void PhysicalAppender::AppendDefault(DataChunk &chunk, idx_t col, idx_t row) {
	auto value = GetDefaultValue(col);
	Append(chunk, value, col, row);
}

Value PhysicalAppender::GetDefaultValue(idx_t column) {
	auto index = column;

	if (!column_ids.empty()) {
		if (column >= column_ids.size()) {
			throw InvalidInputException("Column index out of bounds");
		}
		index = column_ids[column].index;
	}

	auto it = default_values.find(index);
	if (it == default_values.end()) {
		auto &name = description->columns[index].Name();
		throw NotImplementedException(
		    "AppendDefault is not supported for column \"%s\": not a foldable default expressions.", name);
	}
	return it->second;
}

void PhysicalAppender::AddColumn(const string &name) {
	Flush();

	auto exists = false;
	for (idx_t col_idx = 0; col_idx < description->columns.size(); col_idx++) {
		auto &col_def = description->columns[col_idx];
		if (col_def.Name() != name) {
			continue;
		}

		if (col_def.Generated()) {
			throw InvalidInputException("cannot add a generated column to the appender");
		}

		for (const auto &column_id : column_ids) {
			if (column_id == col_def.Logical()) {
				throw InvalidInputException("cannot add the same column twice");
			}
		}

		active_types.push_back(col_def.Type());
		column_ids.push_back(col_def.Logical());
		exists = true;
		break;
	}
	if (!exists) {
		throw InvalidInputException("the column must exist in the table");
	}

	InitializeChunk();
	collection = make_uniq<ColumnDataCollection>(allocator, GetActiveTypes());
}

void PhysicalAppender::ClearColumns() {
	Flush();
	column_ids.clear();
	active_types.clear();

	InitializeChunk();
	collection = make_uniq<ColumnDataCollection>(allocator, GetActiveTypes());
}

} // namespace duckdb
