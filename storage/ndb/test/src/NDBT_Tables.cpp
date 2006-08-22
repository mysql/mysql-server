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

#include <NDBT.hpp>
#include <NDBT_Table.hpp>
#include <NDBT_Tables.hpp>

/* ******************************************************* */
//    Define Ndb standard tables 
//
//  USE ONLY UPPERLETTERS IN TAB AND COLUMN NAMES
/* ******************************************************* */

/*
 * These are our "official" test tables
 *
 */
/* T1 */
static
const
NDBT_Attribute T1Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table T1("T1", sizeof(T1Attribs)/sizeof(NDBT_Attribute), T1Attribs);

/* T2 */
static
const
NDBT_Attribute T2Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Bigunsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Bit, 23),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned, 
		 1, false, true), // Nullable 
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned)
};
static
const
NDBT_Table T2("T2", sizeof(T2Attribs)/sizeof(NDBT_Attribute), T2Attribs);

/* T3 */
static
const
NDBT_Attribute T3Attribs[] = {
  NDBT_Attribute("ID", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("PERSNR", NdbDictionary::Column::Char, 10),
  NDBT_Attribute("NAME", NdbDictionary::Column::Char, 25),
  NDBT_Attribute("ADRESS", NdbDictionary::Column::Char, 50),
  NDBT_Attribute("ADRESS2", NdbDictionary::Column::Char, 
		 30, false, true), // Nullable
  NDBT_Attribute("FÖDELSEÅR", NdbDictionary::Column::Unsigned)
};
static
const
NDBT_Table T3("T3", sizeof(T3Attribs)/sizeof(NDBT_Attribute), T3Attribs);

/* T4 */
static
const
NDBT_Attribute T4Attribs[] = {
  NDBT_Attribute("REGNR", NdbDictionary::Column::Char, 6, true), 
  NDBT_Attribute("YEAR", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("OWNER", NdbDictionary::Column::Char, 25),
  NDBT_Attribute("ADRESS", NdbDictionary::Column::Char, 50),
  NDBT_Attribute("ADRESS2", NdbDictionary::Column::Char, 
		 30, false, true), // Nullable
  NDBT_Attribute("OWNERID", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("CHECKDATE", NdbDictionary::Column::Unsigned)
};
static
const
NDBT_Table T4("T4", sizeof(T4Attribs)/sizeof(NDBT_Attribute), T4Attribs);

/* T5 */
static
const
NDBT_Attribute T5Attribs[] = {
  NDBT_Attribute("OWNERID", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("REGNR", NdbDictionary::Column::Char, 6, true),
  NDBT_Attribute("CREATEDDATE", NdbDictionary::Column::Unsigned)
};
static
const
NDBT_Table T5("T5", sizeof(T5Attribs)/sizeof(NDBT_Attribute), T5Attribs);

/* T6 */
static
const
NDBT_Attribute T6Attribs[] = {
  NDBT_Attribute("PK1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("ATTR1", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR4", NdbDictionary::Column::Char, 
		 47, false, true),// Nullable
  NDBT_Attribute("ATTR5", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR6", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR7", NdbDictionary::Column::Char, 
		 48, false, true),// Nullable
  NDBT_Attribute("ATTR8", NdbDictionary::Column::Char, 
		 50, false, true), // Nullable
  NDBT_Attribute("ATTR9", NdbDictionary::Column::Int),
  NDBT_Attribute("ATTR10", NdbDictionary::Column::Float),
  NDBT_Attribute("ATTR11", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR12", NdbDictionary::Column::Char, 49),
  NDBT_Attribute("ATTR13", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR14", NdbDictionary::Column::Char, 50),
  NDBT_Attribute("ATTR15", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR16", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR17", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR18", NdbDictionary::Column::Char, 257),
  NDBT_Attribute("ATTR19", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR20", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table T6("T6", sizeof(T6Attribs)/sizeof(NDBT_Attribute), T6Attribs);

/* T7 */
static
const
NDBT_Attribute T7Attribs[] = {
  NDBT_Attribute("PK1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("PK2", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("PK3", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("PK4", NdbDictionary::Column::Varbinary, 123, true), 
  NDBT_Attribute("ATTR1", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR5", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR6", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR7", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR8", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR9", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR10", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR11", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR12", NdbDictionary::Column::Char, 259),
  NDBT_Attribute("ATTR13", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR14", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR15", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR16", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR17", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR18", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR19", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ATTR20", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table T7("T7", sizeof(T7Attribs)/sizeof(NDBT_Attribute), T7Attribs);

/* T8 */
static
const
NDBT_Attribute T8Attribs[] = {
  NDBT_Attribute("PERSON_ID", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("NAME", NdbDictionary::Column::Varbinary, 255),
  NDBT_Attribute("ADRESS", NdbDictionary::Column::Longvarbinary, 513),
  NDBT_Attribute("POSTADRESS", NdbDictionary::Column::Char, 1173),
  NDBT_Attribute("VALUE", NdbDictionary::Column::Unsigned),
  
};
static
const
NDBT_Table T8("T8", sizeof(T8Attribs)/sizeof(NDBT_Attribute), T8Attribs);

/* T9 */
static
const
NDBT_Attribute T9Attribs[] = {
  NDBT_Attribute("KF_SKAPAD", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("PLATS_ID", NdbDictionary::Column::Char, 2, true),
  NDBT_Attribute("TNR_SKAPAD", NdbDictionary::Column::Char, 12, true),
  NDBT_Attribute("DELG_MOT", NdbDictionary::Column::Char, 1, true),
  NDBT_Attribute("VALUE", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table T9("T9", sizeof(T9Attribs)/sizeof(NDBT_Attribute), T9Attribs);

/* T10 - Long key table */
static
const
NDBT_Attribute T10Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Char, 256, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Char, 257),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table T10("T10", sizeof(T10Attribs)/sizeof(NDBT_Attribute), T10Attribs);


/* T11 - Primary key is not first attribute */
static
const
NDBT_Attribute T11Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Char, 111),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Char, 113)
};

static
const
NDBT_Table T11("T11", sizeof(T11Attribs)/sizeof(NDBT_Attribute), T11Attribs);

/* T12 - 16 primary keys */
static
const
NDBT_Attribute T12Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL6", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL7", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL8", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL9", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL10", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL11", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL12", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL13", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL14", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL15", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL16", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL20", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL30", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL40", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL50", NdbDictionary::Column::Unsigned)
};

static
const
NDBT_Table T12("T12", sizeof(T12Attribs)/sizeof(NDBT_Attribute), T12Attribs);

/* T13 - Long key table */
static
const
NDBT_Attribute T13Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Char, 257, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Char, 259, true),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Char, 113, true),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned, 1, true),
  NDBT_Attribute("KOL6", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table T13("T13", sizeof(T13Attribs)/sizeof(NDBT_Attribute), T13Attribs);

/* T14 - 5 primary keys */
static
const
NDBT_Attribute T14Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL3", NdbDictionary::Column::Char, 4, true), 
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL20", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL30", NdbDictionary::Column::Int),
  NDBT_Attribute("KOL40", NdbDictionary::Column::Float),
  NDBT_Attribute("KOL50", NdbDictionary::Column::Char, 200)
};


static
const
NDBT_Table T14("T14", sizeof(T14Attribs)/sizeof(NDBT_Attribute), T14Attribs);

/*
  C2 DHCP TABLES, MAYBE THESE SHOULD BE MOVED TO THE UTIL_TABLES?
*/
static 
const
NDBT_Attribute I1_Cols[] = {
  NDBT_Attribute("ID", NdbDictionary::Column::Unsigned, true),
  NDBT_Attribute("PORT", NdbDictionary::Column::Char, 16, true),
  NDBT_Attribute("ACCESSNODE", NdbDictionary::Column::Char, 16, true),
  NDBT_Attribute("POP", NdbDictionary::Column::Char, 64, true),
  NDBT_Attribute("VLAN", NdbDictionary::Column::Char, 16),
  NDBT_Attribute("COMMENT", NdbDictionary::Column::Char, 128),
  NDBT_Attribute("SNMPINDEX", NdbDictionary::Column::Int),
  NDBT_Attribute("PORTSTATE", NdbDictionary::Column::Int),
  NDBT_Attribute("UPDATES", NdbDictionary::Column::Unsigned)
};

static
const
char* I1_Indexes[] = {
  "UNIQUE", "ID", "PORT", "ACCESSNODE", "POP", "PORTSTATE", 0,
  0
};

static
NDBT_Table I1("I1", sizeof(I1_Cols)/sizeof(NDBT_Attribute), I1_Cols
	      );// ,I1_Indexes);

static 
const
NDBT_Attribute I2_Cols[] = {
  NDBT_Attribute("ID", NdbDictionary::Column::Unsigned, true),
  NDBT_Attribute("PORT", NdbDictionary::Column::Char, 16, true),
  NDBT_Attribute("ACCESSNODE", NdbDictionary::Column::Char, 16, true),
  NDBT_Attribute("POP", NdbDictionary::Column::Char, 64, true),
  NDBT_Attribute("ACCESSTYPE", NdbDictionary::Column::Int, true),
  NDBT_Attribute("CUSTOMER_ID", NdbDictionary::Column::Int),
  NDBT_Attribute("PROVIDER", NdbDictionary::Column::Int),
  NDBT_Attribute("TEXPIRE", NdbDictionary::Column::Int),
  NDBT_Attribute("NUM_IP", NdbDictionary::Column::Int),
  NDBT_Attribute("LEASED_NUM_IP", NdbDictionary::Column::Int),
  NDBT_Attribute("LOCKED_IP", NdbDictionary::Column::Int),
  NDBT_Attribute("STATIC_DNS", NdbDictionary::Column::Int),
  NDBT_Attribute("SUSPENDED_SERVICES", NdbDictionary::Column::Int),
  NDBT_Attribute("UPDATES", NdbDictionary::Column::Unsigned)
};

const
char* I2_Indexes[] = {
  "ORDERED", "CUSTOMER_ID", 0,
  "ORDERED", "NUM_IP", 0,
  0
};

static
NDBT_Table I2("I2", sizeof(I2_Cols)/sizeof(NDBT_Attribute), I2_Cols
	      );//, I2_Indexes);

static 
const
NDBT_Attribute I3_Cols[] = {
  NDBT_Attribute("ID", NdbDictionary::Column::Unsigned, true),
  NDBT_Attribute("PORT", NdbDictionary::Column::Char, 16), // SI2
  NDBT_Attribute("ACCESSNODE", NdbDictionary::Column::Char, 16), // SI2
  NDBT_Attribute("POP", NdbDictionary::Column::Char, 64), // SI2
  NDBT_Attribute("MAC", NdbDictionary::Column::Char, 12, true), 
  NDBT_Attribute("MAC_EXPIRE", NdbDictionary::Column::Int, 1),
  NDBT_Attribute("IIP", NdbDictionary::Column::Int), // SI1
  NDBT_Attribute("P_EXPIRE", NdbDictionary::Column::Int),
  NDBT_Attribute("HOSTNAME", NdbDictionary::Column::Char, 32),
  NDBT_Attribute("DETECTED", NdbDictionary::Column::Int),
  NDBT_Attribute("STATUS", NdbDictionary::Column::Int),
  NDBT_Attribute("NUM_REQUESTS", NdbDictionary::Column::Int),
  NDBT_Attribute("ACCESSTYPE", NdbDictionary::Column::Int),
  NDBT_Attribute("OS_TYPE", NdbDictionary::Column::Int),
  NDBT_Attribute("GW", NdbDictionary::Column::Int),
  NDBT_Attribute("UPDATES", NdbDictionary::Column::Unsigned)
};

const
char* I3_Indexes[] = {
  "UNIQUE", "ID", 0,
  "ORDERED", "MAC", 0,
  "ORDERED", "GW", 0,
  0
};

static
NDBT_Table I3("I3", sizeof(I3_Cols)/sizeof(NDBT_Attribute), I3_Cols
	      ); // ,I3_Indexes);

/* D1 */
static
const
NDBT_Attribute D1Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 1, false, false, 0, NdbDictionary::Column::StorageTypeDisk),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Char, 233, false, true, 0, NdbDictionary::Column::StorageTypeDisk),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned),
};
static
const
NDBT_Table D1("D1", sizeof(D1Attribs)/sizeof(NDBT_Attribute), D1Attribs);

static
const
NDBT_Attribute D2Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Varbinary, 127, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 1, false, false, 0, NdbDictionary::Column::StorageTypeDisk),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Varbinary, 133),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Char, 199, false, true, 0, NdbDictionary::Column::StorageTypeDisk),
  NDBT_Attribute("KOL6", NdbDictionary::Column::Bit, 21, false, false, 0, NdbDictionary::Column::StorageTypeDisk),
};
static
const
NDBT_Table D2("D2", sizeof(D2Attribs)/sizeof(NDBT_Attribute), D2Attribs);


// Define array with pointer to all tables 
static
const
NDBT_Table *test_tables[]=
{ 
  &T1,
  &T2,
  &T3,
  &T4,
  &T5,
  &T6,
  &T7,
  &T8,
  &T9,
  &T10,
  &T11,
  &T12,
  &T13,
  &T14,
  &I1,
  &I2,
  &I3,
  &D1, &D2
};

struct NDBT_IndexList {
  const char * m_table;
  const char ** m_indexes;
};

static
const
NDBT_IndexList indexes[] = {
  "I1", I1_Indexes, 
  "I2", I2_Indexes, 
  "I3", I3_Indexes,
  0, 0
};

static
const
int numTestTables = sizeof(test_tables)/sizeof(NDBT_Table*);


/**
 * Define tables we should not be able to create
 */ 

/* F1 
 *
 * Error: PK and column with same name
 */
static
const
NDBT_Attribute F1Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned)
};

static
const
NDBT_Table F1("F1", sizeof(F1Attribs)/sizeof(NDBT_Attribute), F1Attribs);

/* F2
 *
 * Error: Two columns with same name
 */
static
const
NDBT_Attribute F2Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned)
};

static
const
NDBT_Table F2("F2", sizeof(F2Attribs)/sizeof(NDBT_Attribute), F2Attribs);

/* F3
 *
 * Error: Too many primary keys defined, 32 is max
 */
static
const
NDBT_Attribute F3Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL6", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL7", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL8", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL9", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL10", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL11", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL12", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL13", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL14", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL15", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL16", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL17", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL18", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL19", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL20", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL21", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL22", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL23", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL24", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL25", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL26", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL27", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL28", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL29", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL30", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL31", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL32", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL33", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("KOL40", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL50", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL60", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL70", NdbDictionary::Column::Unsigned)
};

static
const
NDBT_Table F3("F3", sizeof(F3Attribs)/sizeof(NDBT_Attribute), F3Attribs);

/* F4
 *
 * Error: Too long key
 */
static
const
NDBT_Attribute F4Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL2", NdbDictionary::Column::Unsigned, 9999999, true), 
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned)
};

static
const
NDBT_Table F4("F4", sizeof(F4Attribs)/sizeof(NDBT_Attribute), F4Attribs);

/* F5
 *
 * Error: Too long attr name
 */
static
const
NDBT_Attribute F5Attribs[] = {
  NDBT_Attribute("KOL1WITHVERRYLONGNAME_ISITTOLONG", NdbDictionary::Column::Unsigned, true),
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned)
};

static
const
NDBT_Table F5("F5", sizeof(F5Attribs)/sizeof(NDBT_Attribute), F5Attribs);

/* F6
 *
 * Error: Zero length of pk attribute
 */
static
const
NDBT_Attribute F6Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Char, 0, true, false), 
  NDBT_Attribute("KOL2", NdbDictionary::Column::Char, 256),
};

static
const
NDBT_Table F6("F6", sizeof(F6Attribs)/sizeof(NDBT_Attribute), F6Attribs);

/* F7
 *
 * Error: Table without primary key
 */
static
const
NDBT_Attribute F7Attribs[] = {
  NDBT_Attribute("KOL3", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Unsigned)
};

NDBT_Table F7("F7", sizeof(F7Attribs)/sizeof(NDBT_Attribute), F7Attribs);

/* F8
 *
 * Error: Table without nullable primary key
 */
static
const
NDBT_Attribute F8Attribs[] = {
  NDBT_Attribute("KOL3", NdbDictionary::Column::Int, 1, true, true),
  NDBT_Attribute("KOL4", NdbDictionary::Column::Int),
  NDBT_Attribute("KOL5", NdbDictionary::Column::Int)
};

NDBT_Table F8("F8", sizeof(F8Attribs)/sizeof(NDBT_Attribute), F8Attribs);


/* F15 - 2-node crash in v20x */
static
const
NDBT_Attribute F15Attribs[] = {
  NDBT_Attribute("KOL1", NdbDictionary::Column::Char, 40, true)
};
static
const
NDBT_Table F15("F15", sizeof(F15Attribs)/sizeof(NDBT_Attribute), F15Attribs);

// Define array with pointer to tables that we should not  be able to create
static
const
NDBT_Table *fail_tables[]=
{ 
  &F1,
  &F2,
  &F3,
  &F4,
  &F5,
  &F6,
  &F7,
  &F8,
  &F15
};

static
const
int numFailTables = sizeof(fail_tables)/sizeof(NDBT_Table*);


/**
 * Define util tables that we may create
 */ 


/* GL 
 * General ledger table for bank application
 */
static 
const
NDBT_Attribute GL_Attribs[] = {
  NDBT_Attribute("TIME", NdbDictionary::Column::Bigunsigned, 1, true), 
  NDBT_Attribute("ACCOUNT_TYPE", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("BALANCE", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("DEPOSIT_COUNT", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("DEPOSIT_SUM", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("WITHDRAWAL_COUNT", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("WITHDRAWAL_SUM", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("PURGED", NdbDictionary::Column::Unsigned)
};

static
NDBT_Table GL("GL", sizeof(GL_Attribs)/sizeof(NDBT_Attribute), GL_Attribs);

/* ACCOUNT
 * Account table for bank application
 */
static 
const
NDBT_Attribute ACCOUNT_Attribs[] = {
  NDBT_Attribute("ACCOUNT_ID", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("OWNER", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("BALANCE", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("ACCOUNT_TYPE", NdbDictionary::Column::Unsigned),
};

static
NDBT_Table ACCOUNT("ACCOUNT", sizeof(ACCOUNT_Attribs)/sizeof(NDBT_Attribute), ACCOUNT_Attribs);

/* TRANSACTION
 * Transaction table for bank application
 */
static 
const
NDBT_Attribute TRANSACTION_Attribs[] = {
  NDBT_Attribute("TRANSACTION_ID", NdbDictionary::Column::Bigunsigned, 1, true),
  NDBT_Attribute("ACCOUNT", NdbDictionary::Column::Unsigned, 1, true), 
  NDBT_Attribute("ACCOUNT_TYPE", NdbDictionary::Column::Unsigned), 
  NDBT_Attribute("OTHER_ACCOUNT", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("TRANSACTION_TYPE", NdbDictionary::Column::Unsigned),
  NDBT_Attribute("TIME", NdbDictionary::Column::Bigunsigned),
  NDBT_Attribute("AMOUNT", NdbDictionary::Column::Unsigned),
};

static
NDBT_Table TRANSACTION("TRANSACTION", sizeof(TRANSACTION_Attribs)/sizeof(NDBT_Attribute), TRANSACTION_Attribs);

/* SYSTEM_VALUES
 * System values table for bank application
 */
static 
const
NDBT_Attribute SYSTEM_VALUES_Attribs[] = {
  NDBT_Attribute("SYSTEM_VALUES_ID", NdbDictionary::Column::Unsigned, 1, true),
  NDBT_Attribute("VALUE", NdbDictionary::Column::Bigunsigned)
};

static
NDBT_Table SYSTEM_VALUES("SYSTEM_VALUES", sizeof(SYSTEM_VALUES_Attribs)/sizeof(NDBT_Attribute), SYSTEM_VALUES_Attribs);

/* ACCOUNT_TYPES
 * Account types table for bank application
 */
static 
const
NDBT_Attribute ACCOUNT_TYPES_Attribs[] = {
  NDBT_Attribute("ACCOUNT_TYPE_ID", NdbDictionary::Column::Unsigned, 1, true),
  NDBT_Attribute("DESCRIPTION", NdbDictionary::Column::Char, 64, false, false, &my_charset_latin1_bin)
};

static
NDBT_Table ACCOUNT_TYPES("ACCOUNT_TYPE", sizeof(ACCOUNT_TYPES_Attribs)/sizeof(NDBT_Attribute), ACCOUNT_TYPES_Attribs);


// Define array with pointer to util tables 
static
const
NDBT_Table *util_tables[]=
{ 
  &GL,
  &ACCOUNT,
  &TRANSACTION,
  &SYSTEM_VALUES,
  &ACCOUNT_TYPES
};

static
const
int numUtilTables = sizeof(util_tables)/sizeof(NDBT_Table*);


const
NdbDictionary::Table*
NDBT_Tables::getTable(const char* _nam){
  // Search tables list to find a table
  NDBT_Table* tab = NULL;
  int i;
  for (i=0; i<numTestTables; i++){
    if (strcmp(test_tables[i]->getName(), _nam) == 0){
      return test_tables[i];
    }
  }
  for (i=0; i<numFailTables; i++){
    if (strcmp(fail_tables[i]->getName(), _nam) == 0){
      return fail_tables[i];
    }
  }
  for (i=0; i<numUtilTables; i++){
    if (strcmp(util_tables[i]->getName(), _nam) == 0){
      return util_tables[i];
    }
  }
  // TPK_no tables
  // Dynamcially create table vith primary key size
  // set to no
  // Useful for testing key sizes 1 - max
  int pkSizeOfTable;
  if(sscanf(_nam, "TPK_%d", &pkSizeOfTable) == 1){   
    return tableWithPkSize(_nam, pkSizeOfTable);
  }
  return tab;
}

const NdbDictionary::Table*
NDBT_Tables::tableWithPkSize(const char* _nam, Uint32 pkSize){
  NdbDictionary::Table* tab = new NdbDictionary::Table(_nam);

  // Add one PK of the desired length
  tab->addColumn(NDBT_Attribute("PK1",
				NdbDictionary::Column::Char,
				pkSize,
				true));
  
  // Add 4 attributes
  tab->addColumn(NDBT_Attribute("ATTR1",
				NdbDictionary::Column::Char,
				21));
  
  tab->addColumn(NDBT_Attribute("ATTR2",
				NdbDictionary::Column::Char,
				124));
  
  tab->addColumn(NDBT_Attribute("ATTR3",
				NdbDictionary::Column::Unsigned));
  
  tab->addColumn(NDBT_Attribute("ATTR4",
				NdbDictionary::Column::Unsigned));
  
  return tab;
}

const NdbDictionary::Table* 
NDBT_Tables::getTable(int _num){
  // Get table at pos _num
  assert(_num < numTestTables);
  return test_tables[_num];
}

int
NDBT_Tables::getNumTables(){
  return numTestTables;
}

const char**
NDBT_Tables::getIndexes(const char* table)
{
  Uint32 i = 0;
  for (i = 0; indexes[i].m_table != 0; i++) {
    if (strcmp(indexes[i].m_table, table) == 0)
      return indexes[i].m_indexes;
  }
  return 0;
}

int
NDBT_Tables::createAllTables(Ndb* pNdb, bool _temp, bool existsOk){
  
  for (int i=0; i < NDBT_Tables::getNumTables(); i++){
    pNdb->getDictionary()->dropTable(NDBT_Tables::getTable(i)->getName());
    int ret= createTable(pNdb, 
			 NDBT_Tables::getTable(i)->getName(), _temp, existsOk);
    if(ret){
      return ret;
    }
  }
  return NDBT_OK;
}

int
NDBT_Tables::createAllTables(Ndb* pNdb){
  return createAllTables(pNdb, false);
}

int
NDBT_Tables::create_default_tablespace(Ndb* pNdb)
{
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  int res;
  NdbDictionary::LogfileGroup lg = pDict->getLogfileGroup("DEFAULT-LG");
  if (strcmp(lg.getName(), "DEFAULT-LG") != 0)
  {
    lg.setName("DEFAULT-LG");
    lg.setUndoBufferSize(8*1024*1024);
    res = pDict->createLogfileGroup(lg);
    if(res != 0){
      g_err << "Failed to create logfilegroup:"
	    << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  {
    NdbDictionary::Undofile uf = pDict->getUndofile(0, "undofile01.dat");
    if (strcmp(uf.getPath(), "undofile01.dat") != 0)
    {
      uf.setPath("undofile01.dat");
      uf.setSize(32*1024*1024);
      uf.setLogfileGroup("DEFAULT-LG");
      
      res = pDict->createUndofile(uf, true);
      if(res != 0){
	g_err << "Failed to create undofile:"
	      << endl << pDict->getNdbError() << endl;
	return NDBT_FAILED;
      }
    }
  }
  {
    NdbDictionary::Undofile uf = pDict->getUndofile(0, "undofile02.dat");
    if (strcmp(uf.getPath(), "undofile02.dat") != 0)
    {
      uf.setPath("undofile02.dat");
      uf.setSize(32*1024*1024);
      uf.setLogfileGroup("DEFAULT-LG");
      
      res = pDict->createUndofile(uf, true);
      if(res != 0){
	g_err << "Failed to create undofile:"
	      << endl << pDict->getNdbError() << endl;
	return NDBT_FAILED;
      }
    }
  }
  NdbDictionary::Tablespace ts = pDict->getTablespace("DEFAULT-TS");
  if (strcmp(ts.getName(), "DEFAULT-TS") != 0)
  {
    ts.setName("DEFAULT-TS");
    ts.setExtentSize(1024*1024);
    ts.setDefaultLogfileGroup("DEFAULT-LG");
    
    res = pDict->createTablespace(ts);
    if(res != 0){
      g_err << "Failed to create tablespace:"
	    << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  
  {
    NdbDictionary::Datafile df = pDict->getDatafile(0, "datafile01.dat");
    if (strcmp(df.getPath(), "datafile01.dat") != 0)
    {
      df.setPath("datafile01.dat");
      df.setSize(64*1024*1024);
      df.setTablespace("DEFAULT-TS");
      
      res = pDict->createDatafile(df, true);
      if(res != 0){
	g_err << "Failed to create datafile:"
	      << endl << pDict->getNdbError() << endl;
	return NDBT_FAILED;
      }
    }
  }

  {
    NdbDictionary::Datafile df = pDict->getDatafile(0, "datafile02.dat");
    if (strcmp(df.getPath(), "datafile02.dat") != 0)
    {
      df.setPath("datafile02.dat");
      df.setSize(64*1024*1024);
      df.setTablespace("DEFAULT-TS");
      
      res = pDict->createDatafile(df, true);
      if(res != 0){
	g_err << "Failed to create datafile:"
	      << endl << pDict->getNdbError() << endl;
	return NDBT_FAILED;
      }
    }
  }
  
  return NDBT_OK;
}

int
NDBT_Tables::createTable(Ndb* pNdb, const char* _name, bool _temp, 
			 bool existsOk, NDBT_CreateTableHook f, void* arg)
{
  const NdbDictionary::Table* tab = NDBT_Tables::getTable(_name);
  if (tab == NULL){
    ndbout << "Could not create table " << _name 
	   << ", it doesn't exist in list of tables "\
      "that NDBT_Tables can create!" << endl;
    return NDBT_WRONGARGS;
  }
  
  int r = 0;
  do {
    NdbDictionary::Table tmpTab(* tab);
    tmpTab.setStoredTable(_temp ? 0 : 1);
    if(f != 0 && f(pNdb, tmpTab, 0, arg))
    {
      ndbout << "Failed to create table" << endl;
      return NDBT_FAILED;
    }   
loop:   
    r = pNdb->getDictionary()->createTable(tmpTab);
    if(r == -1){
      if(pNdb->getDictionary()->getNdbError().code == 723)
      {
	if (create_default_tablespace(pNdb) == 0)
	{
	  goto loop;
	}
      }
      if(!existsOk){
	ndbout << "Error0: " << pNdb->getDictionary()->getNdbError() << endl;
	
	break;
      }
      if(pNdb->getDictionary()->getNdbError().code != 721){
	ndbout << "Error: " << pNdb->getDictionary()->getNdbError() << endl;
	break;
      }
      
      r = 0;
    }
    
    Uint32 i = 0;
    for(i = 0; indexes[i].m_table != 0; i++){
      if(strcmp(indexes[i].m_table, _name) != 0)
	continue;
      Uint32 j = 0;
      while(indexes[i].m_indexes[j] != 0){
	NdbDictionary::Index tmpIndx;
	BaseString name;
	name.assfmt("%s$NDBT_IDX%d", _name, j);
	tmpIndx.setName(name.c_str());
	tmpIndx.setTable(_name);
	bool logging = !_temp;
	if(strcmp(indexes[i].m_indexes[j], "ORDERED") == 0){
	  logging = false;
	  tmpIndx.setType(NdbDictionary::Index::OrderedIndex);
	} else if(strcmp(indexes[i].m_indexes[j], "UNIQUE") == 0){
	  tmpIndx.setType(NdbDictionary::Index::UniqueHashIndex);
	} else {
	  ndbout << "Unknown index type";
	  abort();
	}
	tmpIndx.setLogging(logging);
	
	j++;
	while(indexes[i].m_indexes[j] != 0){
	  tmpIndx.addIndexColumn(indexes[i].m_indexes[j]);
	  j++;
	}
	j++;
	if(pNdb->getDictionary()->createIndex(tmpIndx) != 0){
	  ndbout << pNdb->getDictionary()->getNdbError() << endl;
	  return NDBT_FAILED;
	}
      }
    }
    if(f != 0 && f(pNdb, tmpTab, 1, arg))
    {
      ndbout << "Failed to create table" << endl;
      return NDBT_FAILED;
    }      
  } while(false);
  
  return r;
}

int
NDBT_Tables::dropAllTables(Ndb* pNdb){

  for (int i=0; i < NDBT_Tables::getNumTables(); i++){

    const NdbDictionary::Table* tab = NDBT_Tables::getTable(i);
    if (tab == NULL){
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    if(pNdb->getDictionary()->dropTable(tab->getName()) == -1){
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}


int
NDBT_Tables::print(const char * _name){
  
  const NDBT_Table * tab = (const NDBT_Table*)NDBT_Tables::getTable(_name);
  if (tab == NULL){
    ndbout << "Could not print table " << _name 
	   << ", it doesn't exist in list of tables "
	   << "that NDBT_Tables can create!" << endl;
    return NDBT_WRONGARGS;
  }
  ndbout << (* tab) << endl;
  return NDBT_OK;
}

int
NDBT_Tables::printAll(){

  for (int i=0; i < getNumTables(); i++){
    
    const NdbDictionary::Table* tab = getTable(i);
    if (tab == NULL){
      abort();
    }
    ndbout << (* (NDBT_Table*)tab) << endl;
  }
  
  return NDBT_OK;
}
