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
import org.antlr.runtime.tree.Tree;

import com.mysql.clusterj.jdbc.antlr.MySQL51Parser;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.QueryDomainType;

public class BetweenNode extends PredicateNode {

    public BetweenNode(Token token) {
        super(token);
        setNumberOfParameters(2);
    }

    public BetweenNode(BetweenNode betweenNode) {
        super(betweenNode);
        setNumberOfParameters(2);
    }

    @Override
    public BetweenNode dupNode() {
        return new BetweenNode(this);
    }

    @Override
    public Predicate getPredicate(QueryDomainType<?> queryDomainType) {
        Predicate result = null;
        String propertyName = null;
        String leftParameterName = getParameterName(1);
        String rightParameterName = getParameterName(2);
        if (getChild(0).getType() == MySQL51Parser.NOT) {
                                       // For NOT BETWEEN,
            propertyName = getChild(0) // the BETWEEN NODE's first child is a NOT node
            .getChild(0)               // whose child is a FIELD node
            .getChild(0)               // whose child is a text node
            .getText();                // containing the column name
            result = queryDomainType.not(queryDomainType.get(propertyName)
                    .between(queryDomainType.param(leftParameterName), queryDomainType.param(rightParameterName)));
        } else {
            propertyName = getPropertyName();
            result = queryDomainType.get(propertyName)
                    .between(queryDomainType.param(leftParameterName), queryDomainType.param(rightParameterName));
        }
        Tree lowerBound = getChild(1);
        Tree upperBound = getChild(2);
        if (logger.isDetailEnabled()) logger.detail("propertyName: " + propertyName
                + " lowerBound: " + lowerBound.getText()
                + " upperBound: " + upperBound.getText());
        return result;
    }

}
