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

#include <NdbApi.hpp>
#include <dictionary/DictSchema.hpp>
#include <codegen/Code_create_table.hpp>

void
Exec_create_table::execute(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Ndb* const ndb = ndbObject();
    NdbDictionary::Dictionary* ndbDictionary = ndb->getDictionary();
    if (ndbDictionary == 0) {
	ctx.pushStatus(ndb, "getDictionary");
	return;
    }
    NdbDictionary::Table ndbTable(code.m_tableName.c_str());
    for (unsigned i = 1; i <= code.m_attrCount; i++) {
	const Code::Attr& attr = code.m_attrList[i];
	NdbDictionary::Column ndbColumn(attr.m_attrName.c_str());
	if (i == code.m_tupleId)
	    ndbColumn.setTupleKey(true);	// XXX setTupleId()
	if (ctx.logLevel() >= 3) {
	    char buf[100];
	    attr.m_sqlType.print(buf, sizeof(buf));
	    ctx_log3(("attr %s type %s", ndbColumn.getName(), buf));
	}
	if (attr.m_tupleKey)
	    ndbColumn.setPrimaryKey(true);
	attr.m_sqlType.getType(ctx, &ndbColumn);
	if (! ctx.ok())
	    return;
	if (attr.m_autoIncrement)
	    ndbColumn.setAutoIncrement(true);
	char defaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];
	defaultValue[0] = 0;
	if (attr.m_defaultValue != 0) {
	    // XXX obviously should evalute it at insert time too
	    attr.m_defaultValue->evaluate(ctx, ctl);
	    if (! ctx.ok())
		return;
	    const SqlField& f = attr.m_defaultValue->getData().sqlField();
	    const SqlType& t = f.sqlSpec().sqlType();
	    // XXX use SqlField cast method instead
	    SQLINTEGER ind = 0;
	    ExtType extType(ExtType::Char);
	    ExtSpec extSpec(extType);
	    ExtField extField(extSpec, (SQLPOINTER)defaultValue, sizeof(defaultValue), &ind);
	    f.copyout(ctx, extField);
	    if (! ctx.ok())
		return;
	    if (ind == SQL_NULL_DATA)		// do not store NULL default
		defaultValue[0] = 0;
	}
	if (defaultValue[0] != 0)
	    ndbColumn.setDefaultValue(defaultValue);
	ndbTable.addColumn(ndbColumn);
    }
    if (code.m_fragmentType != NdbDictionary::Object::FragUndefined)
	ndbTable.setFragmentType(code.m_fragmentType);
    ndbTable.setLogging(code.m_logging);
    dictSchema().deleteTable(ctx, code.m_tableName);
    if (ndbDictionary->createTable(ndbTable) == -1) {
	ctx.pushStatus(ndbDictionary->getNdbError(), "createTable %s", ndbTable.getName());
	return;
    }
    ctx_log1(("table %s created", ndbTable.getName()));
}
