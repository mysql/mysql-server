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

import org.antlr.runtime.Token;

public class BinaryOperatorNode extends PredicateNode {

    public BinaryOperatorNode(Token token) {
        super(token);
        // binary operators all have exactly one parameter
        // change this once we support non-parameter operations
        setNumberOfParameters(1);
    }

    public BinaryOperatorNode(PredicateNode binaryOperatorNode) {
        super(binaryOperatorNode);
        // binary operators all have exactly one parameter
        // change this once we support non-parameter operations
        setNumberOfParameters(1);
    }

    @Override
    public BinaryOperatorNode dupNode() {
        return new BinaryOperatorNode(this);
    }

    public int getParameterId() {
        return ((PlaceholderNode)getChild(1)).getId();
    }

    protected String getParameterName() {
        return getParameterName(1);
    }

}
