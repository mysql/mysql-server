#ifndef _spw_test_column_subset_h
#define _spw_test_column_subset_h

#include "common.h"

using namespace Sparrow;

//-----------------------------------------------------------------------------
// Test API insert on a subset of columns 

class testDefinition {
public:
	uint	id_;
	bool	nulls_;
	std::vector<uint>	columns_;
public:
	testDefinition(uint id, bool nulls, uint nbCols, uint* columns) : id_(id), nulls_(nulls) {
		for ( uint i=0; i<nbCols; ++i ) {
			columns_.push_back( *columns++ );
		}
	}
};

inline std::ostream& operator<<(std::ostream& out, const testDefinition& test)
{
	out << test.id_ << ", " << (test.nulls_ ? "Nulls" : "Not nulls") << ", " << test.columns_.size() << " columns: {";
	bool	first = true;
	for (const auto col : test.columns_) {
		if (!first) out << ", ";
		out << col;
		first = false;
	}
	out << '}';
	return out;
}


//-----------------------------------------------------------------------------
// Test API insert on a subset of columns on a table with ~2000 columns

class testDefinitionM {
public:
	uint	id_;
	bool	nulls_;
	uint	nbColumns_;
	uint	nbValues_;
	std::vector<uint>	columns_;		// Indexes of populated columns (first column has index 0)

public:
	testDefinitionM(uint id, bool nulls, uint nbCols, uint nbValues) : id_(id), nulls_(nulls), nbColumns_(nbCols), nbValues_(nbValues) {
		uint	step = nbColumns_/nbValues_;
		for ( uint i=0, col=1; i<nbValues_; ++i, col+=step ) {
			columns_.push_back(col);
		}
	}
};

inline std::ostream& operator<<(std::ostream& out, const testDefinitionM& test)
{
	return out << test.id_ << ", " << (test.nulls_ ? "Nulls" : "Not nulls") << ", set " << test.nbValues_ << "/" 
		<< test.nbColumns_ << " columns";
}

//-----------------------------------------------------------------------------

class TestColumnSubset : public Test {
public:
	TestColumnSubset(const SQLparams& sql_params) : Test(sql_params) {;}

	void run();

private:
	Table* createTable(const char* table_name, bool nullable);
	Table* createTableMassive(const char* table_name, bool nullable, uint nbColumns);
	void getColumnNames(ColumnNames* columns, Table* table, const testDefinition& params);
	void getColumnNamesM(ColumnNames* columns, Table* table, const testDefinitionM& params);
	void testSelColumns(Table* table, testDefinition& params, int test_number);
	void insertSelectColumns(const char* tableName, bool nullable);
	void insertSelectColumnsMassive(const char* tableName, bool nullable, uint nbCols);
	void testSelColumnsMassive(Table* table, testDefinitionM& params, uint nbRows);
};

#endif	// _spw_test_column_subset_h