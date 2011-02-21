/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.jdbc.antlr.node;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.jdbc.antlr.MySQL51Parser;

import org.antlr.runtime.Token;

public class BinaryOperatorNode extends PredicateNode {

    public BinaryOperatorNode(Token token) {
        super(token);
    }

    public BinaryOperatorNode(BinaryOperatorNode binaryOperatorNode) {
        super(binaryOperatorNode);
    }

    @Override
    public BinaryOperatorNode dupNode() {
        return new BinaryOperatorNode(this);
    }

    public int getParameterId() {
        return ((PlaceholderNode)getChild(1)).getId();
    }

    protected String getParameterName() {
        if (getChild(1).getType() == MySQL51Parser.VALUE_PLACEHOLDER) {
            return getChild(1).getText();
        } else {
            throw new ClusterJFatalInternalException(local.message("ERR_RHS_Not_A_Parameter"));
        }
    }

    protected String getPropertyName() {
        if (getChild(0).getType() == MySQL51Parser.FIELD) {
            return getChild(0).getChild(0).getText();
        } else {
            throw new ClusterJFatalInternalException(local.message("ERR_LHS_Not_A_Field"));
        }
    }

}
