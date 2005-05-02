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

#include <common/ConnArea.hpp>
#include "DictCatalog.hpp"
#include "DictSchema.hpp"

DictCatalog::~DictCatalog()
{
    for (Schemas::iterator i = m_schemas.begin(); i != m_schemas.end(); i++) {
	delete *i;
	*i = 0;
    }
}

DictSchema*
DictCatalog::findSchema(Ctx& ctx, const BaseString& name)
{
    for (Schemas::iterator i = m_schemas.begin(); i != m_schemas.end(); i++) {
	DictSchema* schema = *i;
	ctx_assert(schema != 0);
	if (strcmp(schema->getName().c_str(), name.c_str()) == 0)
	    return schema;
    }
    ctx_assert(strcmp(name.c_str(), "NDB") == 0);
    DictSchema* schema = new DictSchema(m_connArea, "NDB");
    m_schemas.push_back(schema);
    return schema;
}
