/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.jdbc.antlr.node;

import org.antlr.runtime.tree.CommonTree;
import org.antlr.runtime.Token;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.jdbc.SQLExecutor;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.QueryDomainType;

public class Node extends CommonTree {

    /** My number of parameters */
    protected int numberOfParameters = -1;

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(SQLExecutor.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(SQLExecutor.class);

    public Node(Token token) {
        super(token);
    }

    public Node(Node node) {
        super(node);
    }

    @Override
    public Node dupNode() {
        return new Node(this);
    }

    public Predicate getPredicate(QueryDomainType<?> queryDomainType) {
        // default behavior is no predicate is possible from the tree
        return null;
    }

    public int getNumberOfParameters() {
        if (numberOfParameters == -1) {
            throw new ClusterJFatalInternalException(local.message("ERR_Number_Of_Parameters_Not_Initialized"));
        }
        return numberOfParameters;
    }

    protected void setNumberOfParameters(int numberOfParameters) {
        this.numberOfParameters = numberOfParameters;
    }

}
