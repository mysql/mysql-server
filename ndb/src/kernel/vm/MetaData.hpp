/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef KERNEL_VM_METADATA_HPP
#define KERNEL_VM_METADATA_HPP

#include <ndb_types.h>
#include <ndb_limits.h>
#include <ErrorReporter.hpp>
#include <signaldata/DictTabInfo.hpp>

class SimulatedBlock;
class Dbdict;
class Dbdih;

/*
 * Common metadata for all blocks on the node.
 *
 * A database node has unique DICT and DIH instances.  Parts of their
 * metadata are described by subclasses of MetaData.  Any block can
 * access the metadata via a MetaData instance.
 */
class MetaData {
public:
  /*
   * Methods return < 0 on error.
   */
  enum Error {
    Locked = -1,
    NotLocked = -2,
    InvalidArgument = -3,
    TableNotFound = -4,
    InvalidTableVersion = -5,
    AttributeNotFound = -6
  };

  /*
   * Common data shared by all metadata instances.  Contains DICT and
   * DIH pointers and counts of shared and exclusive locks.
   */
  class Common {
  public:
    Common(Dbdict& dbdict, Dbdih& dbdih);
  private:
    friend class MetaData;
    Dbdict& m_dbdict;
    Dbdih& m_dbdih;
    unsigned m_lock[2];         // shared: 0 (false), exclusive: 1 (true)
  };

  /*
   * Table metadata.  A base class of Dbdict::TableRecord.  This is
   * actually fragment metadata but until "alter table" there is no
   * difference.
   */
  class Table {
  public:
    /* Table id (array index in DICT and other blocks) */
    Uint32 tableId;

    /* Table version (incremented when tableId is re-used) */
    Uint32 tableVersion;

    /* Table name (may not be unique under "alter table") */
    char tableName[MAX_TAB_NAME_SIZE];

    /* Type of table or index */
    DictTabInfo::TableType tableType;

    /* Is table or index online (this flag is not used in DICT) */
    bool online;

    /* Primary table of index otherwise RNIL */
    Uint32 primaryTableId;

    /* Type of storage (memory/disk, not used) */
    DictTabInfo::StorageType storageType;

    /* Type of fragmentation (small/medium/large) */
    DictTabInfo::FragmentType fragmentType;

    /* Key type of fragmentation (pk/dist key/dist group) */
    DictTabInfo::FragmentKeyType fragmentKeyType;

    /* Global checkpoint identity when table created */
    Uint32 gciTableCreated;

    /* Number of attibutes in table */
    Uint16 noOfAttributes;

    /* Number of null attributes in table (should be computed) */
    Uint16 noOfNullAttr;

    /* Number of primary key attributes (should be computed) */
    Uint16 noOfPrimkey;

    /* Number of distinct character sets (computed) */
    Uint16 noOfCharsets;

    /* Length of primary key in words (should be computed) */
    /* For ordered index this is tree node size in words */
    Uint16 tupKeyLength;

    /* K value for LH**3 algorithm (only 6 allowed currently) */
    Uint8 kValue;

    /* Local key length in words (currently 1) */
    Uint8 localKeyLen;

    /*
     * Parameter for hash algorithm that specifies the load factor in
     * percentage of fill level in buckets. A high value means we are
     * splitting early and that buckets are only lightly used. A high
     * value means that we have fill the buckets more and get more
     * likelihood of overflow buckets.
     */
    Uint8 maxLoadFactor;

    /*
     * Used when shrinking to decide when to merge buckets.  Hysteresis
     * is thus possible. Should be smaller but not much smaller than
     * maxLoadFactor
     */
    Uint8 minLoadFactor;

    /* Is the table logged (i.e. data survives system restart) */
    bool storedTable;

    /* Convenience routines */
    bool isTable() const;
    bool isIndex() const;
    bool isUniqueIndex() const;
    bool isNonUniqueIndex() const;
    bool isHashIndex() const;
    bool isOrderedIndex() const;
  };

  /*
   * Attribute metadata.  A base class of Dbdict::AttributeRecord.
   */
  class Attribute {
  public:
    /* Attribute id within table (counted from 0) */
    Uint16 attributeId;

    /* Attribute number within tuple key (counted from 1) */
    Uint16 tupleKey;

    /* Attribute name (unique within table) */
    char attributeName[MAX_ATTR_NAME_SIZE];

    /* Attribute description (old-style packed descriptor) */
    Uint32 attributeDescriptor;

    /* Extended attributes */
    Uint32 extType;
    Uint32 extPrecision;
    Uint32 extScale;
    Uint32 extLength;

    /* Autoincrement flag, only for ODBC/SQL */
    bool autoIncrement;

    /* Default value as null-terminated string, only for ODBC/SQL */
    char defaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];
  };

  /*
   * Metadata is accessed via a MetaData instance.  The constructor
   * needs a reference to MetaData::Common which can be obtained via
   * the block.  The destructor releases any leftover locks.
   */
  MetaData(Common& common);
  MetaData(SimulatedBlock* block);
  ~MetaData();

  /*
   * Access methods.  Locking can be shared (read) or exclusive (write).
   * Locking can be recursive (a count is kept).  Example (in a block):
   *
   *    MetaData md(this);
   *    MetaData::Table table;
   *    ret = md.lock(false);
   *    ret = md.getTable(table, "SYSTAB_0");
   *    ret = md.unlock();
   */
  int lock(bool exclusive);
  int unlock(bool exclusive);
  int getTable(MetaData::Table& table, Uint32 tableId, Uint32 tableVersion);
  int getTable(MetaData::Table& table, const char* tableName);
  int getAttribute(MetaData::Attribute& attribute, const MetaData::Table& table, Uint32 attributeId);
  int getAttribute(MetaData::Attribute& attribute, const MetaData::Table& table, const char* attributeName);

private:
  Common& m_common;
  unsigned m_lock[2];
};

// MetaData::Table

inline bool
MetaData::Table::isTable() const
{
  return DictTabInfo::isTable(tableType);
}

inline bool
MetaData::Table::isIndex() const
{
  return DictTabInfo::isIndex(tableType);
}

inline bool
MetaData::Table::isUniqueIndex() const
{
  return DictTabInfo::isUniqueIndex(tableType);
}

inline bool
MetaData::Table::isNonUniqueIndex() const
{
  return DictTabInfo::isNonUniqueIndex(tableType);
}

inline bool
MetaData::Table::isHashIndex() const
{
  return DictTabInfo::isHashIndex(tableType);
}

inline bool
MetaData::Table::isOrderedIndex() const
{
  return DictTabInfo::isOrderedIndex(tableType);
}

#endif
