/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.Query;
import com.mysql.clusterj.Query.Ordering;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;

import testsuite.clusterj.model.ConversationSummary;

public class Bug17200163Test extends AbstractClusterJModelTest {

    protected int NUMBER_TO_INSERT = 10;
    
    /** The instances for testing. */
    protected List<ConversationSummary> instances = new ArrayList<ConversationSummary>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(ConversationSummary.class);
        tx.commit();
        addTearDownClasses(ConversationSummary.class);
    }

    public void test() {
        insert();
        testQuery(0, 4, 6);
        failOnError();
    }

    protected void insert() {
        // insert instances
        tx = session.currentTransaction();
        tx.begin();
        
        int count = 0;

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            session.makePersistent(instances.get(i));
            ++count;
        }
        tx.commit();
    }

    protected void testQuery(long userId, long startDate, long endDate) {
        QueryBuilder builder = session.getQueryBuilder(); 
        QueryDomainType<ConversationSummary> domain = 
        builder.createQueryDefinition(ConversationSummary.class); 

        Predicate compare = null; 
        PredicateOperand column = domain.get("sourceUserId"); 
        PredicateOperand param = domain.param("filter_sourceUserId"); 
        compare = column.equal(param); 

        PredicateOperand column2 = domain.get("updatedAt"); 
        PredicateOperand param2 = domain.param("filter_updatedAt1"); 
        compare = compare.and(column2.greaterEqual(param2)); 

        PredicateOperand column3 = domain.get("updatedAt"); 
        PredicateOperand param3 = domain.param("filter_updatedAt2"); 
        compare = compare.and(column3.lessEqual(param3)); 

        domain.where(compare); 

        Query<ConversationSummary> query = session.createQuery(domain); 
        query.setParameter("filter_sourceUserId", userId); 
        query.setParameter("filter_updatedAt1", startDate); 
        query.setParameter("filter_updatedAt2", endDate); 
        query.setLimits(0, 2); 
        query.setOrdering(Ordering.DESCENDING, "updatedAt"); 

        List<Long> expected = new ArrayList<Long>();
        expected.add((long)6);
        expected.add((long)5);
        List<Long> actual = new ArrayList<Long>();
        List<ConversationSummary> results = query.getResultList();
        for (ConversationSummary result: results) {
            actual.add(result.getAnswererId());
        }
        errorIfNotEqual("Results of query with ordering and limits for ConversationSummary", expected, actual);
    }

    protected void createInstances(int number) {
        for (int i = 0; i < number; ++i) {
            ConversationSummary instance = session.newInstance(ConversationSummary.class);
            instance.setSourceUserId(0);
            instance.setAnswererId(i);
            instance.setDestUserId(i);
            instance.setLastMessageById(i);
            instance.setQueryHistoryId(i);
            instance.setText("Text " + i);
            instance.setUpdatedAt(i);
            instance.setViewed(0 == (i%2)?true:false);
            instances.add(instance);
        }
    }


}
