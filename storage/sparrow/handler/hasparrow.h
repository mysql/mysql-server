/*
	Sparrow handler.
*/

#ifndef _handler_handler_h_
#define _handler_handler_h_

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif 

#include "../engine/master.h"
#include "../engine/misc.h"

extern "C" char** thd_query(MYSQL_THD thd);

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RecordWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define SPARROW_MAX_BIT_SIZE	64

typedef SYSarray<uint8_t> BitArray;

/* Reads a record from a partition and format the data to the MySQL format using the TableFields. 
	The bit mask indicates which columns to read and send back. The bit mask corresponds to 
	index definitions.
*/
class PartitionReader;
class RecordWrapper {
protected:

	TableFields fields_;
	uint32_t bits_;
	uint32_t bitSize_;
	uint32_t size_;

private:
	void initialize(const TableFields& fields, const ColumnIds* columnIds, const bool tree) _THROW_(SparrowException);

public:

	RecordWrapper() : bits_(0), bitSize_(0), size_(0) {
	}

	RecordWrapper(const TableFields& fields, const ColumnIds* columnIds, const bool tree) _THROW_(SparrowException);

	RecordWrapper(TableFields& fields, const ColumnIds* columnIds, const bool tree, const bool removeFromFields) _THROW_(SparrowException);

	RecordWrapper(const TableFields& fields, const ColumnIds& skippedColumnIds) _THROW_(SparrowException);

	virtual ~RecordWrapper() {
	}

	const TableFields& getFields() const {
		return fields_;
	}

	uint32_t getSize() const {
		return size_;
	}

	uint32_t getBitSize() const {
		return bitSize_;
	}

	void readBits(ByteBuffer& buffer, uint8_t* bits) const {
		ByteBuffer b(bits, getBitSize());
		buffer >> b;
	}

	void readUsingKeyPartMap(PartitionReader& reader, PartitionReader& stringReader, const key_part_map map,
		uint8_t* buffer, const bool keyFormat) const _THROW_(SparrowException);

	void readUsingTableBitmap(TABLE& table, PartitionReader& reader, PartitionReader& stringReader, const bool all,
		uint8_t* buffer, const bool keyFormat) const _THROW_(SparrowException);

	void readKeyValue(PartitionReader& reader, PartitionReader& stringReader, ByteBuffer& buffer, BinBuffer* binBuffer) const _THROW_(SparrowException);

	int compare(ByteBuffer& buffer1, PartitionReader& stringReader1, ByteBuffer& buffer2, PartitionReader& stringReader2,
		BinBuffer* binBuffer) const _THROW_(SparrowException);
};

typedef SYSvector<RecordWrapper, 0> RecordWrappers;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SerialRecordWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SerialRecordWrapper : public RecordWrapper {
private:

	const uint32_t serial_;		// Column alteration serial number
	const uint32_t index_;
	const bool tree_;

public:

	SerialRecordWrapper(const uint32_t serial, const uint32_t index, const bool tree) : serial_(serial), index_(index), tree_(tree) {
	}

	SerialRecordWrapper(const uint32_t serial, const uint32_t index, const bool tree,
		TableFields& fields, const ColumnIds* columnIds) _THROW_(SparrowException)
		: RecordWrapper(fields, columnIds, tree, true),  serial_(serial), index_(index), tree_(tree) {
	}

	~SerialRecordWrapper() {
		fields_.clearAndDestroy();
	}

	uint32_t hash() const {
		uint32_t result = 31 + index_;
		result = 31 * result + serial_;
		result = 31 * result + (tree_ ? 1231 : 1237);
		return result;
	}

	bool operator == (const SerialRecordWrapper& right) const {
		return serial_ == right.serial_ && index_ == right.index_ && tree_ == right.tree_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartSerialRecordWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PartSerialRecordWrapper : public RecordWrapper {
private:

	const uint32_t serial_;		// Partition serial number

public:

	PartSerialRecordWrapper(const uint32_t serial) : serial_(serial) {
	}

	PartSerialRecordWrapper(const uint32_t serial, const TableFields& fields) _THROW_(SparrowException)
		: RecordWrapper(fields, NULL, false), serial_(serial) {
	}

	~PartSerialRecordWrapper() {
		fields_.clearAndDestroy();
	}

	uint32_t hash() const {
		return serial_;
	}

	bool operator == (const PartSerialRecordWrapper& right) const {
		return serial_ == right.serial_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnInfo
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ColumnInfo {
private:

	uint32_t id_;
	uint32_t bitOffset_;
	uint32_t nbits_;
	uint32_t offset_;
	uint32_t size_;

public:

	ColumnInfo()
		: id_(0), bitOffset_(0), nbits_(0), offset_(0), size_(0) {
	}

	ColumnInfo(const uint32_t id, const uint32_t bitOffset, const uint32_t nbits, const uint32_t offset, const uint32_t size)
		: id_(id), bitOffset_(bitOffset), nbits_(nbits), offset_(offset), size_(size) {
	}

	uint32_t getId() const {
		return id_;
	}

	uint32_t getBitOffset() const {
		return bitOffset_;
	}

	uint32_t getNBits() const {
		return nbits_;
	}

	uint32_t getOffset() const {
		return offset_;
	}

	uint32_t getSize() const {
		return size_;
	}
};

typedef SYSvector<ColumnInfo> ColumnInfos;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DataFileReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DataFileReader {
protected:
	
	const TableFields& fields_;
	ColumnInfos infos_;
	const RecordWrapper recordWrapper_;

public:

	DataFileReader(const TableFields& fields, const ColumnIds& columnIds, const ColumnIds& skippedColumns);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BitMapGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class BitmapGuard {
private:

	TABLE* table_;
	my_bitmap_map* savedMap_;
	
public:
	
	BitmapGuard(TABLE* table) : table_(table), savedMap_(tmp_use_all_columns(table, table->read_set)) {
	}

	~BitmapGuard() {
		tmp_restore_column_map(table_->read_set, savedMap_);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowHandler
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TableShare;
class SparrowHandler : public handler {
	friend class TableShare;

private:

	TableShare* share_;
	THR_LOCK_DATA lockData_;

	// Context.
	Context context_;

	static bool	initialized_;

private:

	static Column createColumn(Field& field) _THROW_(SparrowException);

	static Column createColumn(Create_field& field) _THROW_(SparrowException);

	static Column createColumn(const char* name, const enum_field_types fieldType, const uint decimals, const uint32_t fieldFlags,
		const CHARSET_INFO* charset, const Str& defaultValue) _THROW_(SparrowException);

public:

	// Static methods for handlerton.

	static int initialize(void* p);

	static int deinitialize(void* p);

	static int start_slave_threads();
	static int stop_slave_threads();

	static handler* create(handlerton* hton, TABLE_SHARE* table, bool partitioned, MEM_ROOT* mem_root);

	static int closeConnection(handlerton* hton, THD* thd);

	static void dropDatabase(handlerton* hton, char* path);
	
	static int panic(handlerton* hton, enum ha_panic_function flag);

	static bool showStatus(handlerton* hton, THD* thd, stat_print_fn* stat_print, enum ha_stat_type stat_type);

	SparrowHandler(handlerton* hton, TABLE_SHARE* table);

	~SparrowHandler();

	// --------------------------------------------------------
	// Meta data routines to CREATE, DROP, RENAME table are often used at ALTER TABLE (update_create_info used from ALTER TABLE and SHOW ..).

	// Note: dd::Table* arguments are to be used only if we decide to support atomic DDL

	int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info, dd::Table *table_def) override;

	int delete_table(const char *name, const dd::Table *table_def) override;

	int rename_table(const char *from, const char *to, const dd::Table *from_table_def, dd::Table *to_table_def) override;

	void update_create_info(HA_CREATE_INFO* create_info) override;

	// --------------------------------------------------------
	// Open and close handler object to ensure all underlying files and objects allocated and deallocated for query handling is handled properly.
	int open(const char *name, int mode, uint test_if_locked, const dd::Table *table_def) override;

	int close() override;

	// --------------------------------------------------------
	// This module contains methods that are used to understand start/end of statements, transaction boundaries, and aid for proper concurrency control.
	THR_LOCK_DATA** store_lock(THD* thd, THR_LOCK_DATA** to,  enum thr_lock_type lockType) override;

	int external_lock(THD* thd, int lockType) override;

	// --------------------------------------------------------
	// This part of the handler interface is used to change the records after INSERT, DELETE, UPDATE, REPLACE method calls but also other
	//  special meta-data operations as ALTER TABLE, LOAD DATA, TRUNCATE.

	int write_row(uchar* buf) override;

	int update_row(const uchar* old_data, uchar* new_data) override;

	int delete_row(const uchar* buf) override;

	int delete_all_rows() override;

	void start_bulk_insert(ha_rows rows) override;

	int end_bulk_insert() override;

	// --------------------------------------------------------
	// This module is used for the most basic access method for any table handler. This is to fetch all data through a full table scan. No indexes are needed to implement this part.

	int rnd_init(bool scan) override;

	int rnd_next(uchar* buf) override;

	int rnd_pos(uchar* buf, uchar* pos) override;

	int rnd_end() override;

	void position(const uchar* record) override;

	// --------------------------------------------------------
	// This part of the handler interface is used to perform access through indexes. The interface is defined as a scan interface but the handler
	//	can also use key lookup if the index is a unique index or a primary key index.

	int index_init(uint idx, bool sorted) override;

	int index_next(uchar* buf) override;

	int index_prev(uchar* buf) override;

	int index_first(uchar* buf) override;

	int index_last(uchar* buf) override;

	int index_read_map(uchar* buf, const uchar* key,
		key_part_map keyPartMap, enum ha_rkey_function findFlag) override;

	int index_read_idx_map(uchar* buf, uint index, const uchar* key,
		key_part_map keyPartMap, enum ha_rkey_function findFlag) override;

	int index_read_last_map(uchar* buf, const uchar* key, key_part_map keyPartMap) override;

	int index_end() override;

	// --------------------------------------------------------
	// This calls are used to inform the handler of specifics of the ongoing scans and other actions. Most of these are used for optimisation purposes.

	// TBI. See mysql\include\my_base.h:734 and mysql\storage\example\ha_example.cc:515
	//	Myisam may also be a good example.
	int info(uint) override;

	// TBI. See mysql\include\my_base.h:184 and mysql\storage\example\ha_example.cc:2716
	int extra(enum ha_extra_function operation) override;

	int reset() override;

	// --------------------------------------------------------
	// Optimizer support

	double scan_time() override;

	double read_time(uint index, uint ranges, ha_rows rows) override;

	ha_rows records_in_range(uint inx, key_range* minKey, key_range* maxKey) override;

	// TBI. See mysql\sql\handler.h:5325
	ha_rows estimate_rows_upper_bound() override;

	int records(ha_rows *num_rows) override;

	// --------------------------------------------------------
	// This module contains various methods that returns text messages for table types, index type and error messages.

	const char* table_type() const override;

	bool get_error_message(int error, String* buf) override;


	// --------------------------------------------------------
	// This module contains a number of methods defining limitations and characteristics of the handler (see also documentation regarding the
	//	individual flags).

	// See sql\handler.h:4309 and sql\handler.h:209 for list of flags
	handler::Table_flags table_flags() const override;

	ulong index_flags(uint inx, uint part, bool all_parts) const override;

	uint max_supported_record_length() const override;

	uint max_supported_keys() const override;

	uint max_supported_key_parts() const override;

	uint max_supported_key_length() const override;

	uint max_supported_key_part_length(HA_CREATE_INFO *create_info) const override;

	enum ha_key_alg get_default_index_algorithm() const override;

	bool is_index_algorithm_supported(enum ha_key_alg key_alg) const override;

	// Has been replaced with methods get_default_index_algorithm(), is_index_algorithm_supported(). See mysql\sql\handler.h:4309
	// Default imlpementation is fine. 
	//const char* index_type(uint inx) override;

	// --------------------------------------------------------
	//	This module is used to handle the support of auto increments.

	void get_auto_increment(ulonglong offset, ulonglong increment, ulonglong nb_desired_values, ulonglong* first_value,
		ulonglong* nb_reserved_values) override;
	
	// Not sure what it does. Only implemented in storage engine InnoDB. 
	void release_auto_increment() override  { return; }

	// Seems to have be removed
	//int reset_auto_increment(ulonglong value) override;

	// --------------------------------------------------------
	// Methods for in-place ALTER TABLE support
	//	See mysql\sql\handler.h:6020

	[[deprecated("Part of old, deprecated in-place ALTER API.")]]
	bool check_if_incompatible_data(HA_CREATE_INFO* info, uint table_changes) override;

	// Called by our check_if_incompatible_data() to analyze the raw alterations, Alter_inplace_info::Alter_info
	int check_alterations(Alter_info* info);

	// TODO: Review implementation of these methods: old implementation in new method. 
	enum_alter_inplace_result check_if_supported_inplace_alter(TABLE *altered_table, Alter_inplace_info *ha_alter_info) override;

	// Note: dd::Table* arguments are to be used only if we decide to support atomic DDL.
	bool prepare_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, const dd::Table *old_table_def, dd::Table *new_table_def) override;

	bool inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, const dd::Table *old_table_def, dd::Table *new_table_def) override;

	bool commit_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit, const dd::Table *old_table_def, dd::Table *new_table_def) override;

	void notify_table_changed(Alter_inplace_info *ha_alter_info) override;



	// --------------------------------------------------------
	// Administrative DDL (Data Definition Langage)
	//	Methods that handle the strucure of the data (tables, index)

	int analyze(THD* thd, HA_CHECK_OPT* checkOpt) override;

	int check(THD* thd, HA_CHECK_OPT* checkOpt) override;

	int optimize(THD* thd, HA_CHECK_OPT* checkOpt) override;

	int repair(THD* thd, HA_CHECK_OPT* checkOpt) override;

	// Have those foreign key methods been replaced by something else ?
	//char* get_foreign_key_create_info() override;

	//void free_foreign_key_create_info(char* str) override;

	//int get_foreign_key_list(THD* thd, List<FOREIGN_KEY_INFO>* f_key_list) override;

	// Not part of the handler interface anymore, but still used internally by inplace_alter_table()
	int add_index(TABLE* altered_table, KEY* key_info_buffer, uint* indexes, uint nb);

	// Not part of the handler interface anymore, but still used internally by inplace_alter_table()
	int drop_index(TABLE* table, KEY** key_info, uint nb);

	[[deprecated("Not implemented in 5.6.36: Admin commands not supported currently (almost purely MyISAM routines). This means that the following methods are not implemented. See sql/ha_partition.h")]]
	//int backup(THD* thd, HA_CHECK_OPT* checkOpt) override;

	[[deprecated("Not implemented in 5.6.36: Admin commands not supported currently (almost purely MyISAM routines). This means that the following methods are not implemented. See sql/ha_partition.h")]]
	//int restore(THD* thd, HA_CHECK_OPT* checkOpt) override;
	

	// Overrides handler::clone() but the latter is not declared virtual
	handler* clone(const char* name, MEM_ROOT* mem_root) override;

	void column_bitmaps_signal() override;

	void unlock_row() override;

	bool is_crashed() const override;

	bool auto_repair() const override;

	uint getActiveIndex() {
	  return active_index;
	}

	void setActiveIndex(const uint idx) {
		active_index= idx;
	}

	TABLE& getTable() {
		return *table;
	}

	const TABLE& getTable() const {
		return *table;
	}

	ha_statistics& getStats() {
		return stats;
	}

	// Static methods for information schema

	static int initializeISTables(void* p);

	static int fillISTables(THD* thd, Table_ref* tables, Item* cond);

	static int deinitializeISTables(void* p);

	static int initializeISColumns(void* p);

	static int fillISColumns(THD* thd, Table_ref* tables, Item* cond);

	static int deinitializeISColumns(void* p);

	static int initializeISIndexes(void* p);

	static int fillISIndexes(THD* thd, Table_ref* tables, Item* cond);

	static int deinitializeISIndexes(void* p);

	static int initializeISAlterations(void* p);

	static int fillISAlterations(THD* thd, Table_ref* tables, Item* cond);

	static int deinitializeISAlterations(void* p);

	static int initializeISPartitions(void* p);

	static int fillISPartitions(THD* thd, Table_ref* tables, Item* cond);

	static int deinitializeISPartitions(void* p);

	// Misc
	static uint alterTableFlags(uint flags);


private:

	static Str stripComments(const char* sql);

	static ForeignKeys getForeignKeys(const char* sql, const Str& databaseName,
		const Str& tableName, TABLE* table);

	static const char* moveTo(const char* s, const char* keyword);

	static const char* getIdentifier(const char* s, Str& identifier, bool& hasDot);

	static Str print_KEY(const KEY& index);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TableShare
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TableShare : public RefCounted {
private:

	static SYSpHash<TableShare> hash_;
	static Lock lock_;

	THR_LOCK tableLock_;
	const Str databaseName_;
	const Str tableName_;
	const bool key_;

	// Master file.
	MasterGuard master_;

	// Current column alteration serial.
	uint32_t columnAlterSerial_;

	// Table fields.
	TableFields mappedFields_;		// Fields for columns as currently seen by MySQL (not including columns that have been deleted)
	TableFields fields_;			// Fields for all columns, including deleted columns

	// Prepared record readers.
	RecordWrappers recordWrappers_;

public:

	TableShare(const Str& databaseName, const Str& tableName, TABLE* table) _THROW_(SparrowException);

	~TableShare();

	const Str& getDatabaseName() const {
		return databaseName_;
	}

	const Str& getTableName() const {
		return tableName_;
	}

	Master& getMaster() {
		return *master_;
	}

	uint32_t getColumnAlterSerial() const {
		return columnAlterSerial_;
	}

	const Master& getMaster() const {
		return *master_;
	}

	const TableFields& getMappedFields() const {
		return mappedFields_;
	}

	const TableFields& getFields() const {
		return fields_;
	}

	SerialRecordWrapper* createSerialRecordWrapper(TABLE& table, const uint32_t serial, const uint32_t index, const bool tree) const;

	PartSerialRecordWrapper* createPartSerialRecordWrapper(TABLE& table, const uint32_t alterSerial, const uint32_t partSerial, const ColumnIds& skippedColumnIds) const;

	const RecordWrapper& getRecordWrapper(const uint32_t index, const bool tree) const {
		return recordWrappers_[index == DATA_FILE ? 0 : 1 + index * 2 + (tree ? 1 : 0)];
	}
	
	bool operator == (const TableShare& right) const {
		return databaseName_ == right.databaseName_ && tableName_ == right.tableName_;
	}

	static TableShare* acquire(const Str& databaseName, const Str& tableName, TABLE* table, THR_LOCK_DATA* lockData) _THROW_(SparrowException);

	static void release(TableShare* share);

	uint32_t hash() const {
		uint32_t result = 1;
		result = 31 + databaseName_.hash();
		result = 31 * result + tableName_.hash();
		return result;
	}
};

}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

#endif /* #ifndef _handler_handler_h_ */
