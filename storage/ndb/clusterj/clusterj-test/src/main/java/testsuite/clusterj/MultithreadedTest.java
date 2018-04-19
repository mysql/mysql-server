/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Random;
import java.util.Set;
import java.util.TreeSet;

import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.query.QueryDomainType;

import testsuite.clusterj.model.Customer;
import testsuite.clusterj.model.Order;
import testsuite.clusterj.model.OrderLine;

public class MultithreadedTest extends AbstractClusterJModelTest {

    @Override
    protected boolean getDebug() {
        return false;
    }

    private int numberOfThreads = 50;
    private int numberOfNewCustomersPerThread = 5;
    private int numberOfNewOrdersPerNewCustomer = 5;
    private int numberOfUpdatesPerThread = 2;

    private int maximumOrderLinesPerOrder = 5;
    private int maximumQuantityPerOrderLine = 100;
    private int maximumUnitPrice = 100;


    private int numberOfInitialCustomers = 10;
    private int nextCustomerId = numberOfInitialCustomers;
    private int nextOrderId = 0;
    private int nextOrderLineId = 0;

    private int numberOfUpdatedOrderLines = 0;
    private int numberOfDeletedOrders = 0;
    private int numberOfDeletedOrderLines = 0;

    private ThreadGroup threadGroup;

    /** Customers */
    List<Customer> customers = new ArrayList<Customer>();

    /** Orders */
    List<Order> orders = new ArrayList<Order>();

    /** Order lines */
    Set<OrderLine> orderlines = new TreeSet<OrderLine>(
            new Comparator<OrderLine>() {
                public int compare(OrderLine o1, OrderLine o2) {
                    return o1.getId() - o2.getId();
                }
            }
        );

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        // first delete all customers, orders, and order lines
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Customer.class);
        session.deletePersistentAll(Order.class);
        session.deletePersistentAll(OrderLine.class);
        tx.commit();
        // start out with some customers
        createCustomerInstances(nextCustomerId);
        // add new customer instances
        tx.begin();
        session.makePersistentAll(customers);
        tx.commit();
        // get rid of them when we're done
        addTearDownClasses(Customer.class);
        addTearDownClasses(Order.class);
        addTearDownClasses(OrderLine.class);
    }

    private void createCustomerInstances(int numberToCreate) {
        for (int i = 0; i < numberToCreate; ++i) {
            Customer customer = session.newInstance(Customer.class);
            customer.setId(i);
            customer.setName("Customer number " + i + " (initial)");
            customer.setMagic(i * 100);
            customers.add(customer);
        }
    }

    /** The test method creates numberOfThreads threads and starts them.
     * Once the threads are started, the main thread waits until all threads complete.
     * The main thread then checks that the proper number of instances are
     * created in the database and verifies that all orders are consistent
     * with their order lines. Inconsistency might be due to thread interaction
     * or improper database updates.
     */
    public void test() {
        List<Thread> threads = new ArrayList<Thread>();
        // create thread group
        threadGroup = new ThreadGroup("Stuff");
        // create uncaught exception handler
        MyUncaughtExceptionHandler uncaughtExceptionHandler = new MyUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(uncaughtExceptionHandler);
        // create all threads
        for (int i = 0; i < numberOfThreads ; ++i) {
            Thread thread = new Thread(threadGroup, new StuffToDo());
            threads.add(thread);
            thread.start();
        }
        // wait until all threads have finished
        for (Thread t: threads) {
            try {
                t.join();
            } catch (InterruptedException e) {
                throw new RuntimeException("Interrupted while joining threads.");
            }
        }
        // if any uncaught exceptions (from threads) signal an error
        for (Throwable thrown: uncaughtExceptionHandler.getUncaughtExceptions()) {
            error("Caught exception: " + thrown.getClass().getName() + ": " + thrown.getMessage());
            StackTraceElement[] elements = thrown.getStackTrace();
            for (StackTraceElement element: elements) {
                error("        at " + element.toString());
            }
        }
        // summarize for the record
        if (getDebug()) {
            System.out.println("Number of threads: " + numberOfThreads + 
                "; number of new customers per thread: " + numberOfNewCustomersPerThread +
                "; number of orders per new customer: " + numberOfNewOrdersPerNewCustomer);
            System.out.println("Created " + nextCustomerId + " customers; " +
                nextOrderId + " orders; and " + nextOrderLineId + " order lines.");
            System.out.println("Deleted " + numberOfDeletedOrders + " orders; and " +
                numberOfDeletedOrderLines + " order lines.");
            System.out.println("Updated " + numberOfUpdatedOrderLines + " order lines.");
        }
        errorIfNotEqual("Failed to create customers.",
                numberOfThreads * numberOfNewCustomersPerThread + numberOfInitialCustomers, nextCustomerId);
        errorIfNotEqual("Failed to create orders. ",
                numberOfThreads * numberOfNewCustomersPerThread * numberOfNewOrdersPerNewCustomer, nextOrderId);
        // double check the orders to make sure they were updated correctly
        Session session = sessionFactory.getSession();
        QueryDomainType<OrderLine> queryOrderType = session.getQueryBuilder().createQueryDefinition(OrderLine.class);
        queryOrderType.where(queryOrderType.get("orderId").equal(queryOrderType.param("orderId")));
        Query<OrderLine> query = session.createQuery(queryOrderType);        
        for (Order order: orders) {
            int orderId = order.getId();
            if (getDebug()) System.out.println("Read order " + orderId + " total " + order.getValue());
            // replace order with its persistent representation
            order = session.find(Order.class, orderId);
            double expectedTotal = order.getValue();
            double actualTotal = 0.0d;
            List<OrderLine> orderLines = new ArrayList<OrderLine>();
            for (OrderLine orderLine: getOrderLines(session, query, orderId)) {
                orderLines.add(orderLine);
                if (getDebug()) System.out.println("order " + orderLine.getOrderId() +
                        " orderline " + orderLine.getId() + " value " + orderLine.getTotalValue());
                actualTotal += orderLine.getTotalValue();
            }
            errorIfNotEqual("For order " + orderId + ", order value does not equal sum of order line values."
                    + " orderLines: " + dump(orderLines),
                    expectedTotal, actualTotal);
        }
        failOnError();
    }

    /** This class implements the logic per thread. For each thread created,
     * the run method is invoked. 
     * Each thread uses its own session and shares the customer, order, and order line
     * collections. Collections are synchronized to avoid threading conflicts.
     * Each thread creates numberOfNewCustomersPerThread customers, each of which
     * contains a random number of orders, each of which contains a random number
     * of order lines.
     * Each thread then updates numberOfUpdatesPerThread orders by changing one
     * order line.
     * Each thread then deletes one order and its associated order lines.
     */
    class StuffToDo implements Runnable {

        private Random myRandom = new Random();

        public void run() {
            // get my own session
            Session session = sessionFactory.getSession();
            QueryDomainType<OrderLine> queryOrderType = session.getQueryBuilder().createQueryDefinition(OrderLine.class);
            queryOrderType.where(queryOrderType.get("orderId").equal(queryOrderType.param("orderId")));
            Query<OrderLine> query = session.createQuery(queryOrderType);
            for (int i = 0; i < numberOfNewCustomersPerThread; ++i) {
                // create a new customer
                createCustomer(session, String.valueOf(Thread.currentThread().getId()));
                for (int j = 0; j < numberOfNewOrdersPerNewCustomer ; ++j) {
                    // create a new order
                    createOrder(session, myRandom);
                }
            }
            // update orders
            for (int j = 0; j < numberOfUpdatesPerThread; ++j) {
                updateOrder(session, myRandom, query);
            }
            // delete an order
            deleteOrder(session, myRandom, query);
        }

    }

    /** Create a new customer.
     * 
     * @param session the session
     * @param threadId the thread id of the creating thread
     */
    private void createCustomer(Session session, String threadId) {
        Customer customer = session.newInstance(Customer.class);
        int id = getNextCustomerId();
        customer.setId(id);
        customer.setName("Customer number " + id + " thread " + threadId);
        customer.setMagic(id * 10000);
        session.makePersistent(customer); // autocommit for this
        addCustomer(customer);
    }

    /** Create a new order. Add a new order with a random number of order lines
     * and a random unit price and quantity.
     * 
     * @param session the session
     * @param random a random number generator
     */
    public void createOrder(Session session, Random random) {
        session.currentTransaction().begin();
        // get an order number
        int orderid = getNextOrderId();
        Order order = session.newInstance(Order.class);
        order.setId(orderid);
        // get a random customer number
        int customerId = random .nextInt(nextCustomerId);
        order.setCustomerId(customerId);
        order.setDescription("Order " + orderid + " for Customer " + customerId);
        Double orderValue = 0.0d;
        // now create some order lines
        int numberOfOrderLines = random.nextInt(maximumOrderLinesPerOrder);
        if (getDebug()) System.out.println("Create Order " + orderid
                + " with numberOfOrderLines: " + numberOfOrderLines);
        for (int i = 0; i < numberOfOrderLines; ++i) {
            int orderLineNumber = getNextOrderLineId();
            OrderLine orderLine = session.newInstance(OrderLine.class);
            orderLine.setId(orderLineNumber);
            orderLine.setOrderId(orderid);
            long quantity = random.nextInt(maximumQuantityPerOrderLine);
            orderLine.setQuantity(quantity);
            float unitPrice = ((float)random.nextInt(maximumUnitPrice)) / 4;
            orderLine.setUnitPrice(unitPrice);
            double orderLineValue = unitPrice * quantity;
            orderValue += orderLineValue;
            if (getDebug()) System.out.println("Create orderline " + orderLineNumber + " for Order " + orderid
                    + " quantity " + quantity + " price " + unitPrice
                    + " order line value " + orderLineValue + " order value " + orderValue);
            orderLine.setTotalValue(orderLineValue);
            addOrderLine(orderLine);
            session.persist(orderLine);
        }
        order.setValue(orderValue);
        session.persist(order);
        session.currentTransaction().commit();
        addOrder(order);
    }

    /** Update an order; change one or more order lines
     * 
     * @param session the session
     * @param random a random number generator
     * @param query 
     */
    public void updateOrder(Session session, Random random, Query<OrderLine> query) {
        session.currentTransaction().begin();
        Order order = null;
        // pick an order to update; prevent anyone else from updating the same order
        order = removeOrderFromOrdersCollection(random);
        if (order == null) {
            return;
        }
        int orderId = order.getId();
        // replace order with its persistent representation
        order = session.find(Order.class, orderId);
        List<OrderLine> orderLines = getOrderLines(session, query, orderId);
        int numberOfOrderLines = orderLines.size();
        OrderLine orderLine = null;
        double orderValue = order.getValue();
        if (numberOfOrderLines > 0) {
            int index = random.nextInt(numberOfOrderLines);
            orderLine = orderLines.get(index);
            orderValue -= orderLine.getTotalValue();
            updateOrderLine(orderLine, random);
            orderValue += orderLine.getTotalValue();
        }
        order.setValue(orderValue);
        session.updatePersistent(orderLine);
        session.updatePersistent(order);
        session.currentTransaction().commit();
        // put order back now that we're done updating it
        addOrder(order);
    }

    /** Update an order line by randomly changing unit price and quantity.
     * 
     * @param orderLine the order line to update
     * @param random a random number generator
     */
    private void updateOrderLine(OrderLine orderLine, Random random) {
        int orderid = orderLine.getOrderId();
        int orderLineNumber = orderLine.getId();
        double previousValue = orderLine.getTotalValue();
        long quantity = random.nextInt(maximumQuantityPerOrderLine );
        orderLine.setQuantity(quantity);
        float unitPrice = ((float)random.nextInt(maximumUnitPrice)) / 4;
        orderLine.setUnitPrice(unitPrice);
        double orderLineValue = unitPrice * quantity;
        orderLine.setTotalValue(orderLineValue);
        if (getDebug()) System.out.println("For order " + orderid + " orderline " + orderLineNumber + 
                " previous order line value "  + previousValue + " new order line value " + orderLineValue);
        synchronized (orderlines) {
            ++numberOfUpdatedOrderLines;
        }
    }

    /** Delete an order from the database.
     * 
     * @param session the session
     * @param random a random number generator
     * @param query the query instance to query for OrderLines by OrderId
     */
    public void deleteOrder(Session session, Random random, Query<OrderLine> query) {
        session.currentTransaction().begin();
        Order order = null;
        // pick an order to delete
        order = removeOrderFromOrdersCollection(random);
        if (order == null) {
            return;
        }
        int orderId = order.getId();
        List<OrderLine> orderLines = getOrderLines(session, query, orderId);
        removeOrderLinesFromOrderLinesCollection(orderLines);
        session.deletePersistentAll(orderLines);
        session.deletePersistent(order);
        session.currentTransaction().commit();
    }

    private List<OrderLine> getOrderLines(Session session, Query<OrderLine> query, int orderId) {
        query.setParameter("orderId", orderId);
        return query.getResultList();
    }

    private Order removeOrderFromOrdersCollection(Random random) {
        synchronized(orders) {
            int numberOfOrders = orders.size();
            if (numberOfOrders < 10) {
                return null;
            }
            int which = random.nextInt(numberOfOrders);
            ++numberOfDeletedOrders;
            return orders.remove(which);
        }
    }

    private void removeOrderLinesFromOrderLinesCollection(Collection<OrderLine> orderLinesToRemove) {
        synchronized(orderlines) {
            orderlines.removeAll(orderLinesToRemove);
            numberOfDeletedOrderLines += orderLinesToRemove.size();
        }
    }

    /** Add a new customer to the list of customers
     * @param customer
     */
    private void addCustomer(Customer customer) {
        synchronized(customers) {
            customers.add(customer);
        }
    }

    /** Get the next customer number (multithread safe)
     * @return
     */
    private int getNextCustomerId() {
        synchronized(customers) {
            int result = nextCustomerId++;
            return result;
        }
    }
    
    /** Get the next order number (multithread safe)
     * @return
     */
    private int getNextOrderId() {
        synchronized(orders) {
            int result = nextOrderId++;
            return result;
        }
    }

    /** Get the next order line number (multithread safe)
     * @return
     */
    private int getNextOrderLineId() {
        synchronized(orderlines) {
            int result = nextOrderLineId++;
            return result;
        }
    }
    
    /** Add an order to the list of orders.
     * 
     * @param order the order
     */
    private void addOrder(Order order) {
        synchronized(orders) {
            orders.add(order);
        }
    }

    /** Add an order line to the list of order lines.
     * 
     * @param orderLine the order line
     */
    private void addOrderLine(OrderLine orderLine) {
        synchronized(orderlines) {
            orderlines.add(orderLine);
        }
    }

}
