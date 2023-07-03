/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBT_DATA_SET_HPP
#define NDBT_DATA_SET_HPP

#include "NDBT_Table.hpp"
#include <NdbApi.hpp>

class NDBT_DataSet;

class NDBT_DataSetFactory {
public:
  NDBT_DataSet * createEmpty(const NDBT_Table & table, 
			     const char * columns[]);
  
  NDBT_DataSet * createRandom(const NDBT_DataSet & table, 
			      const char * columns[],
			      int rows);
  
  NDBT_DataSet * createXXX(int noOfDS, const NDBT_DataSet ** datasets);
};

class NDBT_DataSet {
  friend class NDBT_DataSetFactory;
public:
  /**
   * Rows in the data set
   */
  void setRows(int rows);
  void addRows(int rows);
  
  int getNoOfRows() const;

  /**
   * Columns for a row in the data set
   */
  int getNoOfColumns() const;
  int getNoOfPKs() const;
  
  const NDBT_Attribute * getColumn(int index);
  const NDBT_Attribute * getColumn(const char * name);
  
  /**
   * Data status in dataset
   */
  bool hasPK(int row);
  bool hasData(int row);

  /**
   * Do all rows in the dataset have a PK
   */
  bool hasPK();
  
  /**
   * Do all rows in the dataset has data
   */
  bool hasData();
  
  /**
   * Getters for data
   */
  Uint32 getUInt(int row, int index) const;
  Uint32 getUInt(int row, const char * attribute) const;
  
  Int32 getInt(int row, int index) const;
  Int32 getInt(int row, const char * attribute) const;
  
  const char * getString(int row, int index) const;
  const char * getString(int row, const char * attribute) const;

  bool getIsNull(int row, int index) const;
  bool getIsNull(int row, const char * attribute) const;
  
  /**
   * Setters for data
   */
  void set(int row, int index, Int32 value);
  void set(int row, const char * attr, Int32 value);
  
  void set(int row, int index, Uint32 value);
  void set(int row, const char * attr, Uint32 value);
  
  void set(int row, int index, const char * value);
  void set(int row, const char * attr, const char * value);

  /**
   * Comparators
   */

  /**
   * Is this dataset identical to other dataset
   *
   * If either of the datasets have "undefined" rows the answer is false
   */
  bool equal(const NDBT_DataSet & other) const;
  
  /**
   * Do these dataset have identical PK's
   *
   * I.e noOfRows equal
   *
   * and for each row there is a corresponding row in the other ds
   *     with the same pk
   */
  bool equalPK(const NDBT_DataSet & other) const;

private:
  NDBT_Attribute * columns;
  
  Uint32 noOfRows;
  Uint32 noOfPKs;
  
  Uint32 * pks;
  Uint32 * columnSizes;
  
  char * pkData;
  char * data;

  char * pk(int row, int pkIndex);
  char * column(int row, int columnIndex);

  Uint32 * hasPK;
  Uint32 * hasData;
};

#endif
