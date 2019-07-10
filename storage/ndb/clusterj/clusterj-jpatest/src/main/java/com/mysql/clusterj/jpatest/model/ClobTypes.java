/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.jpatest.model;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Lob;
import javax.persistence.Table;

/** Schema
 *
drop table if exists charsetlatin1;
create table charsetlatin1 (
 id int not null primary key,
 smallcolumn varchar(200),
 mediumcolumn varchar(500),
 largecolumn text(10000)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
 
/** Blob types.
 */
@Entity
@Table(name="charsetlatin1")
public class ClobTypes implements IdBase {

    @Id
    private int id;

    @Lob
    @Column(name="smallcolumn")
    private String small200;

    @Lob
    @Column(name="mediumcolumn")
    private String medium500;

    @Lob
    @Column(name="largecolumn")
    private String large10000;

    public int getId() {
        return id;
    }

    public void setId(int id) {
        this.id = id;
    }

    public String getSmall200() {
        return small200;
    }

    public void setSmall200(String small200) {
        this.small200 = small200;
    }

    public String getMedium500() {
        return medium500;
    }

    public void setMedium500(String medium500) {
        this.medium500 = medium500;
    }

    public String getLarge10000() {
        return large10000;
    }

    public void setLarge10000(String large10000) {
        this.large10000 = large10000;
    }

}
