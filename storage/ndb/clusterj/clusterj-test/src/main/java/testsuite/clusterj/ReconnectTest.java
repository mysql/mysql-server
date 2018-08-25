/*
   Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.query.QueryDomainType;

import testsuite.clusterj.model.Customer;
import testsuite.clusterj.model.Order;
import testsuite.clusterj.model.OrderLine;

public class ReconnectTest extends AbstractClusterJModelTest {

    @Override
    protected boolean getDebug() {
        return false;
    }

    private int numberOfThreads = 30;
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

    // member variables for synchronization between threads
    private int numberOfThreadsReady = 0;
    final private Object numberOfThreadsReadySync = new Object();

    private void incrementNumberOfThreadsReady() {
        synchronized (numberOfThreadsReadySync) {
            numberOfThreadsReady++;
            numberOfThreadsReadySync.notify();
        }
    }

    private void waitUntilAtleastNumberOfThreadsReady(int value) {
        synchronized (numberOfThreadsReadySync) {
            while(numberOfThreadsReady < value) {
                logger.warn("Waiting on " + value);
                try {
                    numberOfThreadsReadySync.wait();
                } catch (InterruptedException ie) {
                    ie.printStackTrace();
                }
            }
        }
    }

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
        session.close();
    }

    private void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
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
        // create and start the thread that misbehaves
        Thread misbehaving = new Thread(threadGroup, new Misbehaving());
        misbehaving.start();
        // wait for it to begin
        waitUntilAtleastNumberOfThreadsReady(1);
        // create all normal threads
        for (int i = 0; i < numberOfThreads ; ++i) {
            Thread thread = new Thread(threadGroup, new StuffToDo());
            threads.add(thread);
        }
        // start all threads
        for (Thread thread: threads) {
            thread.start();
        }
        // wait until atleast one StuffToDo thread is ready
        waitUntilAtleastNumberOfThreadsReady(2);
        // tell the SessionFactory to reconnect
        sessionFactory.reconnect(5);
        // wait until all threads have finished
        threads.add(misbehaving);
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
        if (retryCount < 5) error ("Retry count too low: " + retryCount);
        if (getDebug()) {
            System.out.println("Retry count: " + retryCount);
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
        boolean done = false;
        while (!done) {
            if (getDebug()) { System.out.println("verifying..."); }
            try (Session session = sessionFactory.getSession()) {
                QueryDomainType<OrderLine> queryOrderType = session.getQueryBuilder().createQueryDefinition(OrderLine.class);
                queryOrderType.where(queryOrderType.get("orderId").equal(queryOrderType.param("orderId")));
                if (getDebug()) { System.out.println("checking orders: " + orders.size()); }
                for (Order order: orders) {
                    int orderId = order.getId();
                    if (getDebug()) System.out.println("Read order " + orderId + " total " + order.getValue());
                    // replace order with its persistent representation
                    order = session.find(Order.class, orderId);
                    double expectedTotal = order.getValue();
                    double actualTotal = 0.0d;
                    StringBuffer messages = new StringBuffer();
                    List<OrderLine> orderLines = new ArrayList<OrderLine>();
                    for (OrderLine orderLine: getOrderLines(session, queryOrderType, orderId)) {
                        orderLines.add(orderLine);
                        String message = "order " + orderLine.getOrderId() +
                                " orderline " + orderLine.getId() + " value " + orderLine.getTotalValue();
                        if (getDebug()) System.out.println(message);
                        messages.append(message);
                        messages.append('\n');
                        actualTotal += orderLine.getTotalValue();
                    }
                    errorIfNotEqual("For order " + orderId + ", order value does not equal sum of order line values."
                            + " orderLines: \n" + messages.toString(),
                            expectedTotal, actualTotal);
                }
                done = true;
            } catch (Throwable t) {
                if (getDebug()) { System.out.println("summarize for the record caught " + t.getMessage()); }
                sleep(1000);
            }
        }
        failOnError();
    }

    private int retryCount = 0;
    private void incrementRetryCount() {
        synchronized(this) {
            ++retryCount;
        }
    }
    class Misbehaving implements Runnable {
        @Override
        public void run() {
            Session session = sessionFactory.getSession();
            session.currentTransaction().begin();
            boolean done = false;
            QueryDomainType<OrderLine> queryOrderType;
            // increment status to indicate we are running
            incrementNumberOfThreadsReady();
            while (!done) {
                try {
                queryOrderType = session.getQueryBuilder().createQueryDefinition(OrderLine.class);
                queryOrderType.where(queryOrderType.get("orderId").greaterThan(
                        (queryOrderType.param("orderId"))));
                Query<OrderLine> queryOrder = session.createQuery(queryOrderType);
                queryOrder.setParameter("orderId", 0);
                queryOrder.getResultList();
                sleep(100);
                } catch (ClusterJException cje) {
                    // the exception might be any of several exceptions when disconnecting/reconnecting
                    done = true;
                }
            }
        }

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
            // get a session for the queryOrderType
            QueryDomainType<OrderLine> queryOrderType = null;
            boolean done = false;
            while (!done) {
                try (Session session = sessionFactory.getSession()) {
                    queryOrderType = session.getQueryBuilder().createQueryDefinition(OrderLine.class);
                    queryOrderType.where(queryOrderType.get("orderId").equal(queryOrderType.param("orderId")));
                    session.close();
                    done = true;
                } catch (ClusterJUserException cjue) {
                    if (getDebug()) { System.out.println("StuffToDo: query orderId caught " + cjue.getMessage()); }
                    if (cjue.getMessage().contains("SessionFactory is not open")) {
                        sleep(300);
                    }
                }
            }
            // tell that this thread is ready
            incrementNumberOfThreadsReady();
            int i = 0;
            while (i < numberOfNewCustomersPerThread) {
                // create a new customer
                try (Session localSession = sessionFactory.getSession()) {
                    Customer newCustomer = null;
                    List<Order> newOrders = new ArrayList<Order>(numberOfNewOrdersPerNewCustomer);
                    localSession.currentTransaction().begin();
                    newCustomer = createCustomer(localSession, String.valueOf(Thread.currentThread().getId()));
                    int customerId = newCustomer.getId();
                    for (int j = 0; j < numberOfNewOrdersPerNewCustomer ; ++j) {
                            // create a new order for the customer
                            newOrders.add(createOrder(localSession, customerId, myRandom));
                    }
                    ++i;
                    localSession.currentTransaction().commit();
                    // add new customer and orders only if successful
                    addCustomer(newCustomer);
                    addOrders(newOrders);
                } catch (ClusterJUserException cjue) {
                    if (getDebug()) { System.out.println("StuffToDo: create customer caught " + cjue.getMessage()); }
                    if (cjue.getMessage().contains("SessionFactory is not open")) {
                        incrementRetryCount();
                        sleep(300);
                    }
                }
            }
            // update orders
            i = 0;
            while (i < numberOfUpdatesPerThread) {
                try (Session localSession = sessionFactory.getSession()) {
                    // update an order
                    localSession.currentTransaction().begin();
                    Order order = updateOrder(localSession, myRandom, queryOrderType);
                    localSession.currentTransaction().commit();
                    // put the updated order back
                    addOrder(order);
                    ++i;
                } catch (ClusterJUserException cjue) {
                    if (getDebug()) { System.out.println("StuffToDo: update orders caught " + cjue.getMessage()); }
                    if (cjue.getMessage().contains("SessionFactory is not open")) {
                        incrementRetryCount();
                        sleep(300);
                    }
                }
            }
            // delete an order
            done = false;
            while (!done) {
                try (Session localSession = sessionFactory.getSession()) {
                    // delete an order and all of its order lines
                    localSession.currentTransaction().begin();
                    deleteOrder(localSession, myRandom, queryOrderType);
                    localSession.currentTransaction().commit();
                    done = true;
                } catch (ClusterJUserException cjue) {
                    if (getDebug()) { System.out.println("StuffToDo: delete order caught " + cjue.getMessage()); }
                    if (cjue.getMessage().contains("SessionFactory is not open")) {
                        incrementRetryCount();
                        sleep(300);
                    }
                }
            }
        }
    }

    /** Create a new customer.
     * @param session the session
     * @param threadId the thread id of the creating thread
     * @return the new customer
     */
    private Customer createCustomer(Session session, String threadId) {
        Customer customer = session.newInstance(Customer.class);
        int id = getNextCustomerId();
        customer.setId(id);
        customer.setName("Customer number " + id + " thread " + threadId);
        customer.setMagic(id * 10000);
        session.makePersistent(customer);
        return customer;
    }

    /** Create a new order for a specific customer with a random number of order lines
     * and a random unit price and quantity.
     * @param session the session
     * @param customer the customer
     * @param random the random number generator
     * @return the new order
     */
    public Order createOrder(Session session, int customerId, Random random) {
        // get an order number
        int orderid = getNextOrderId();
        Order order = session.newInstance(Order.class);
        order.setId(orderid);
        order.setCustomerId(customerId);
        order.setDescription("Order " + orderid + " for Customer " + customerId);
        Double orderValue = 0.0d;
        // now create some order lines
        int numberOfOrderLines = random.nextInt(maximumOrderLinesPerOrder) + 1;
        if (getDebug()) System.out.println("Create Order " + orderid
                + " with numberOfOrderLines: " + numberOfOrderLines);
        for (int i = 0; i < numberOfOrderLines; ++i) {
            int orderLineNumber = getNextOrderLineId();
            OrderLine orderLine = session.newInstance(OrderLine.class);
            orderLine.setId(orderLineNumber);
            orderLine.setOrderId(orderid);
            long quantity = random.nextInt(maximumQuantityPerOrderLine) + 1;
            orderLine.setQuantity(quantity);
            float unitPrice = (1.0f + (float)random.nextInt(maximumUnitPrice)) / 4;
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
        return order;
    }

    /** Update an order; change one or more order lines
     * @param session the session
     * @param random a random number generator
     * @param query
     */
    public Order updateOrder(Session session, Random random, QueryDomainType<OrderLine> queryOrderType) {
        Order order = null;
        // pick an order to update; prevent anyone else from updating the same order
        order = removeOrderFromOrdersCollection(random);
        if (order == null) { return null; }
        int orderId = order.getId();
        // replace order with its persistent representation
        order = session.find(Order.class, orderId);
        if (order == null) { return null; }
        List<OrderLine> orderLines = getOrderLines(session, queryOrderType, orderId);
        int numberOfOrderLines = orderLines.size();
        OrderLine orderLine = null;
        double orderValue = order.getValue();
        if (getDebug()) { System.out.println("updateOrder previous orderValue: " + orderValue); }
        if (numberOfOrderLines > 0) {
            int index = random.nextInt(numberOfOrderLines);
            orderLine = orderLines.get(index);
            orderValue -= orderLine.getTotalValue();
            updateOrderLine(orderLine, random);
            orderValue += orderLine.getTotalValue();
        }
        if (getDebug()) { System.out.println("updateOrder updated orderValue: " + orderValue); }
        order.setValue(orderValue);
        session.updatePersistent(orderLine);
        session.updatePersistent(order);
        // return order so it can be put back after committing the transaction
        return order;
    }

    /** Update an order line by randomly changing unit price and quantity.
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
     * @param session the session
     * @param random a random number generator
     * @param query the query instance to query for OrderLines by OrderId
     */
    public void deleteOrder(Session session, Random random, QueryDomainType<OrderLine> queryOrderType) {
        Order order = null;
        // pick an order to delete
        order = removeOrderFromOrdersCollection(random);
        if (order == null) { return; }
        int orderId = order.getId();
        // replace order with its current representation
        order = session.find(Order.class, orderId);
        if (order == null) { return; }

        List<OrderLine> orderLines = getOrderLines(session, queryOrderType, orderId);
        removeOrderLinesFromOrderLinesCollection(orderLines);
        session.deletePersistentAll(orderLines);
        session.deletePersistent(order);
    }

    private List<OrderLine> getOrderLines(Session session, QueryDomainType<OrderLine> queryOrderType, int orderId) {
        Query<OrderLine> query = session.createQuery(queryOrderType);
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

    /** Add a new customer to the list of customers (multithread safe)
     * @param customer the customer to add
     */
    private void addCustomer(Customer customer) {
        synchronized(customers) {
            customers.add(customer);
        }
    }

    /** Get the next customer number (multithread safe)
     * @return the next customer id
     */
    private int getNextCustomerId() {
        synchronized(customers) {
            int result = nextCustomerId++;
            return result;
        }
    }

    /** Get the next order number (multithread safe)
     * @return the next order number
     */
    private int getNextOrderId() {
        synchronized(orders) {
            int result = nextOrderId++;
            return result;
        }
    }

    /** Get the next order line number (multithread safe)
     * @return the next order line number
     */
    private int getNextOrderLineId() {
        synchronized(orderlines) {
            int result = nextOrderLineId++;
            return result;
        }
    }

    /** Add an order to the list of orders. (multithread safe)
     * @param order the order
     */
    private void addOrder(Order order) {
        synchronized(orders) {
            orders.add(order);
        }
    }

    /** Add a collection of orders to the list of orders. (multithread safe)
     * @param newOrders the collection of orders
     */
    private void addOrders(Collection<Order> newOrders) {
        synchronized(orders) {
            orders.addAll(newOrders);
        }
    }

    /** Add an order line to the list of order lines. (multithread safe)
     * @param orderLine the order line
     */
    private void addOrderLine(OrderLine orderLine) {
        synchronized(orderlines) {
            orderlines.add(orderLine);
        }
    }

}
