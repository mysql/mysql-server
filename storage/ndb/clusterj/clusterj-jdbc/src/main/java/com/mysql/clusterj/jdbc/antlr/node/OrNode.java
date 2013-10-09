/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
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

import org.antlr.runtime.Token;

import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.QueryDomainType;

public class OrNode extends BooleanOperatorNode {

    public OrNode(Token token) {
        super(token);
    }

    public OrNode(OrNode orNode) {
        super(orNode);
    }

    @Override
    public OrNode dupNode() {
        return new OrNode(this);
    }

    @Override
    public Predicate getPredicate(QueryDomainType<?> queryDomainType) {
        Predicate result = null;
        PredicateNode leftNode = getLeftPredicateNode();
        PredicateNode rightNode = getRightPredicateNode();
        Predicate leftPredicate = leftNode.getPredicate(queryDomainType);
        Predicate rightPredicate = rightNode.getPredicate(queryDomainType);
        result = leftPredicate.or(rightPredicate);
        setNumberOfParameters(leftNode.getNumberOfParameters() + rightNode.getNumberOfParameters());
        return result;
    }

}
