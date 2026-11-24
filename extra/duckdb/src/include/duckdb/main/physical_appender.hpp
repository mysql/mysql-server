//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/physical_appender.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/parser/column_definition.hpp"
#include "duckdb/main/appender.hpp"
#include "duckdb/main/table_description.hpp"
// #include "duckdb/planner/logical_index.hpp"

namespace duckdb {

class Connection;
class ClientContext;
class ColumnDataCollection;

//! PhysicalAppender bypasses the SQL execution layer and writes directly to
//! the table's transaction-local storage via LocalAppend, preserving the
//! pre-refactor high-performance flush behavior.
class PhysicalAppender : public BaseAppender {
public:
	DUCKDB_API PhysicalAppender(Connection &con, const string &database_name, const string &schema_name,
	                            const string &table_name, AppenderType appender_type,
	                            const idx_t flush_memory_threshold = DConstants::INVALID_INDEX);
	DUCKDB_API PhysicalAppender(Connection &con, const string &schema_name, const string &table_name,
	                            AppenderType appender_type,
	                            const idx_t flush_memory_threshold = DConstants::INVALID_INDEX);
	DUCKDB_API PhysicalAppender(Connection &con, const string &table_name, AppenderType appender_type,
	                            const idx_t flush_memory_threshold = DConstants::INVALID_INDEX);
	DUCKDB_API ~PhysicalAppender() override;

public:
	void AppendDefault() override;
	void AppendDefault(DataChunk &chunk, idx_t col, idx_t row) override;
	void AddColumn(const string &name) override;
	void ClearColumns() override;

private:
	//! A weak pointer to the context of this appender.
	weak_ptr<ClientContext> context;
	//! The table description including the column names.
	unique_ptr<TableDescription> description;
	//! All table default values.
	unordered_map<column_t, Value> default_values;
	//! If not empty, holds all logical column IDs of columns provided by the appender.
	vector<LogicalIndex> column_ids;

protected:
	void FlushInternal(ColumnDataCollection &collection) override;
	Value GetDefaultValue(idx_t column);
};

} // namespace duckdb
