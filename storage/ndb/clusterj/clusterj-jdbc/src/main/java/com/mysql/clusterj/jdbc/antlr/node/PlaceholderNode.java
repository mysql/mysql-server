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

public class PlaceholderNode extends Node {

    /** Keep track of which parameter this is */
    private static int sequence = 1;

    private final int id;

    public PlaceholderNode(Token token) {
        super(token);
        this.id = sequence++;
        this.token.setText(String.valueOf(id));
    }

    public PlaceholderNode(PlaceholderNode predicateNode) {
        super(predicateNode);
        this.id = predicateNode.id;
        this.token.setText(String.valueOf(id));
    }

    @Override
    public PlaceholderNode dupNode() {
        return new PlaceholderNode(this);
    }

    public int getId() {
        return id;
    }

    public static void resetId() {
        sequence = 1;
    }

}
