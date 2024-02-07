/*
 *  Copyright (c) 2013, 2024, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
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

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
DROP TABLE IF EXISTS conversation_summary;
CREATE TABLE conversation_summary (
  source_user_id bigint(11) NOT NULL,
  destination_user_id bigint(11) NOT NULL,
  last_message_user_id bigint(11) NOT NULL,
  text_summary varchar(255) NOT NULL DEFAULT '',
  query_history_id bigint(20) NOT NULL DEFAULT '0',
  answerer_id bigint(11) NOT NULL,
  viewed bit(1) NOT NULL,
  updated_at bigint(20) NOT NULL,
  PRIMARY KEY (source_user_id,destination_user_id,query_history_id),
  KEY IX_updated_at (updated_at)
) ENGINE=ndbcluster;

 */
@PersistenceCapable(table="conversation_summary")
public interface ConversationSummary {

    @PrimaryKey 
    @Column(name = "source_user_id") 
    long getSourceUserId(); 
    void setSourceUserId(long id); 

    @PrimaryKey 
    @Column(name = "destination_user_id") 
    long getDestUserId(); 
    void setDestUserId(long id); 

    @Column(name = "last_message_user_id") 
    long getLastMessageById(); 
    void setLastMessageById(long id); 

    @Column(name = "text_summary") 
    String getText(); 
    void setText(String text); 

    @PrimaryKey 
    @Column(name = "query_history_id") 
    long getQueryHistoryId(); 
    void setQueryHistoryId(long id); 

    @Column(name = "answerer_id") 
    long getAnswererId(); 
    void setAnswererId(long id); 

    @Column(name = "viewed") 
    boolean getViewed(); 
    void setViewed(boolean viewed); 

    @Column(name = "updated_at") 
    @Index(name="IX_updated_at") 
    long getUpdatedAt(); 
    void setUpdatedAt(long updated); 
}
