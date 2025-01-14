#include "memalloc.h"
#include "spw_table.h"
#include "spw_connection.h"

// MySQL C connector
#include <mysql.h>


namespace Sparrow
{

InitMySQLib	spw_Table::initMySQL_;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// spw_Table
//////////////////////////////////////////////////////////////////////////////////////////////////////
spw_Table::spw_Table() : maxLifetime_(0), coalescingPeriod_(0), aggregationPeriod_(0), defaultWhere_(0), stringOptimization_(0)
{}

spw_Table::spw_Table(const Str& database, const Str& table)
	: maxLifetime_(0), coalescingPeriod_(0), aggregationPeriod_(0), defaultWhere_(0), stringOptimization_(0)
{
	if ( database.length() == 0 )
		throw SparrowException::create(false, SPW_API_FAILED, "Database name cannot be empty");
	databaseName_ = database;
	databaseName_.toLower();

	if ( table.length() == 0 )
		throw SparrowException::create(false, SPW_API_FAILED, "spw_Table name cannot be empty");
	tableName_ = table;
	tableName_.toLower();
}

spw_Table::spw_Table(const Str& database, const Str& table, 
			 const Columns& columns, const Indexes& indexes, const ForeignKeys& foreignKeys,
			 const DnsConfiguration& dns, uint64_t maxLifetime, uint64_t coalescingPeriod,
			 uint32_t aggregationPeriod, uint64_t defaultWhere, uint64_t stringOptimization)
	: columns_(columns), indexes_(indexes), foreignKeys_(foreignKeys), dns_(dns),
	maxLifetime_(maxLifetime), coalescingPeriod_(coalescingPeriod), aggregationPeriod_(aggregationPeriod),
	defaultWhere_(defaultWhere), stringOptimization_(stringOptimization)
{
	if ( database.length() == 0 )
		throw SparrowException::create(false, SPW_API_FAILED, "Database name cannot be empty");
	databaseName_ = database;
	databaseName_.toLower();

	if ( table.length() == 0 )
		throw SparrowException::create(false, SPW_API_FAILED, "spw_Table name cannot be empty");
	tableName_ = table;
	tableName_.toLower();

	if ( columns.length() == 0 ) {
		throw SparrowException::create(false, SPW_API_FAILED, "No column defined.");
	}

	// Check the first column is a NOT NULL timestamp.
	if ( columns[0].getType() != COL_TIMESTAMP 
		|| columns[0].isFlagSet(COL_NULLABLE) ) {
		throw SparrowException::create(false, SPW_API_FAILED, "The first column must be a NOT NULL timestamp.");
	}

	// Checks if indexes reference the table columns.
	for ( uint32_t i=0; i<indexes_.length(); ++i ) {
		const spw_Index&	index = indexes_[i];
		if ( index.getColumnIds().length() == 0 ) {
			throw SparrowException::create(false, SPW_API_FAILED, "Index %u does not reference any column.", i);
		}
		for ( uint32_t j=0; j<index.getColumnIds().length(); ++j ) {
			if ( index.getColumnIds()[j] >= columns.length() ) {
				throw SparrowException::create(false, SPW_API_FAILED, "Index %u must reference columns in the table.", i);
			}
		}
	}

	// Check duplicate indexes.
	for ( uint32_t i=0; i<indexes_.length(); ++i ) {
		for ( uint32_t j=0; j<indexes_.length(); ++j ) {
			if ( i != j  && indexes_[i] == indexes_[j] ) {
				throw SparrowException::create( false, SPW_API_FAILED, "Index %u and %u are duplicates.", i, j );
			}
		}
	}

	// Check foreign keys
	SYSvector<uint32_t>		fcolumns;
	for ( uint32_t i=0; i<foreignKeys_.length(); ++i ) {
		if ( foreignKeys_[i].getColumnId() > columns.length() ) {
			throw SparrowException::create( false, SPW_API_FAILED, "Foreign key %u must reference a column defined in the table.", i );
		}
		for ( uint32_t j=0; j<fcolumns.length(); ++j ) {
			if ( fcolumns[j] == foreignKeys_[i].getColumnId() ) {
				const spw_Column&	fcolumn = columns[foreignKeys_[i].getColumnId()];
				throw SparrowException::create( false, SPW_API_FAILED, "Several foreign key use column \"%s\"", fcolumn.getName() );
			}
		}
		fcolumns.append(  foreignKeys_[i].getColumnId()  );
	}

	// Check DNS flags.
	bool	hasDnsIdentifier = false;
	bool	hasReverseDns = false;
	for ( uint32_t i=0; i<columns.length(); ++i ) {
		const Column&	column = columns[i];
		if ( column.isFlagSet(COL_DNS_IDENTIFIER) ) {
			if ( hasDnsIdentifier ) {
				throw SparrowException::create( false, SPW_API_FAILED, "Only one column can be the DNS identifier." );
			}
			if ( column.getType() != COL_INT ) {
				throw SparrowException::create( false, SPW_API_FAILED, "The DNS identifier column must be an integer." );
			}
			hasDnsIdentifier = true;
		}
		if ( column.isFlagSet(COL_IP_LOOKUP) ) {
			hasReverseDns = true;
			if ( column.getType() != COL_STRING ) {
				throw SparrowException::create( false, SPW_API_FAILED, "Column \"%s\" must be a string since it contains "
					"reverse DNS lookups.", column.getName() );
			}
			uint32_t ip = column.getInfo();
			if ( ip >= columns.length() ) {
				throw SparrowException::create( false, SPW_API_FAILED, "Column \"%s\" references a column out of range: %d.", column.getName(), ip );
			}
			if ( columns[ip].getType() != COL_BLOB || !columns[ip].isFlagSet(COL_IP_ADDRESS) ) {
				throw SparrowException::create( false, SPW_API_FAILED, "Column \"%s\" contains reverse DNS lookups of values "
					"from column \"%s\". This column does not contain IP addresses.", 
					column.getName(), columns[ip].getName() );
			}
			if ( !column.isFlagSet(COL_NULLABLE) ) {
				throw SparrowException::create( false, SPW_API_FAILED, "Column \"%s\" must be nullable since it contains reverse DNS lookups.", column.getName() );
			}
		}
	}
	
	if ( hasReverseDns && dns.entries() > 0 ) {
		if ( !hasDnsIdentifier && (dns.entries() > 1 || !dns.contains(-1) ) ) {
			throw SparrowException::create( false, SPW_API_FAILED, "At least one column contains reverse DNS lookups and there is no DNS identifier column. "
				"In this case, the DNS configuration can contain only the wildcard identifier.");
		}
	}
}

void spw_Table::getColumnDefinition( Str& str, const spw_Column& column ) {
	char	buffer[1024];
	snprintf( buffer, sizeof(buffer), "`%s` %s", column.getName(), getType(column.getType()) );
	if ( column.getType() == COL_BLOB || column.getType() == COL_STRING ) {
		char	b[64];
		snprintf( b, sizeof(b), "(%u)", column.getStringSize() );
		strcat( buffer, b );
	}
	if ( column.isFlagSet(COL_UNSIGNED) ) {
		strcat( buffer, " UNSIGNED" );
	}
	if ( column.isFlagSet(COL_NULLABLE) ) {
		strcat( buffer, " NULL" );
	} else {
		strcat( buffer, " NOT NULL" );
	}
	const char* defaultValue = column.getDefaultValue();
	if (strlen(defaultValue) > 0) {
		strcat(buffer, " DEFAULT '");
		strcat(buffer, defaultValue);
		strcat(buffer, "'");
	}
	if ( column.isFlagSet(COL_AUTO_INC) ) {
		strcat( buffer, " AUTO_INCREMENT" );
	}
	str = Str( buffer, false );
}

int spw_Table::create( Connection* connection )
{
	SPW_relASSERT(connection);
	if ( connection->isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	spw_Connection*		conn = static_cast<spw_Connection*>(connection);

	try {
		conn->initialize( *this );
		return 0;
	} catch ( const SparrowException& e ) {
		spwerror = e;
		return e.getErrcode();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// InitMySQLib
//////////////////////////////////////////////////////////////////////////////////////////////////////

void InitMySQLib::initialize() {
	if ( initialized_ ) return;
	if ( mysql_library_init(0, nullptr, nullptr) ) {
		throw SparrowException::create(false, SPW_API_FAILED, "could not initialize MySQL library");
	}
	initialized_ = true;
}

void InitMySQLib::clear() {
	if ( initialized_ ) {
		mysql_library_end();
	}
	initialized_ = false;
}


}
