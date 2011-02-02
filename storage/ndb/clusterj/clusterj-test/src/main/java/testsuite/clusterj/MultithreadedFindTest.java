/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

import com.mysql.clusterj.Session;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

public class MultithreadedFindTest extends AbstractClusterJModelTest {

    @Override
    protected boolean getDebug() {
        return false;
    }

    private int numberOfThreads = 6;
    private int numberOfIterations = 5000;
    private ThreadGroup threadGroup;

    @Override
    public void localSetUp() {
        createSessionFactory();
    }

    /** The test method creates numberOfThreads threads and starts them.
     * Once the threads are started, the main thread waits until all threads complete.
     */
    public void test() {
        List<Thread> threads = new ArrayList<Thread>();
        List<Finder> finders = new ArrayList<Finder>();
        // create thread group
        threadGroup = new ThreadGroup("Finder");
        // create uncaught exception handler
        MyUncaughtExceptionHandler uncaughtExceptionHandler = new MyUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(uncaughtExceptionHandler);
        // create all threads
        for (int i = 0; i < numberOfThreads ; ++i) {
            Finder finder = new Finder();
            Thread thread = new Thread(threadGroup, finder);
            threads.add(thread);
            finders.add(finder);
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
        for (Throwable thrown: uncaughtExceptionHandler.getUncaughtExceptions()) {
            error("Caught exception: " + thrown.getClass().getName() + ": " + thrown.getMessage());
            StackTraceElement[] elements = thrown.getStackTrace();
            for (StackTraceElement element: elements) {
                error("        at " + element.toString());
            }
        }
        failOnError();
    }

    /** This class implements the logic per thread. For each thread created,
     * the run method is invoked. 
     * Each thread uses its own session.
     */
    class Finder implements Runnable {

        private Random myRandom = new Random();
        private Session session;
        private long time;

        public long getTime() {
            return time;
        }

        public void run() {
            // get my own session
            if (getDebug()) System.out.println("Getting session.");
            session = sessionFactory.getSession();
            long start = System.nanoTime();
            if (getDebug()) System.out.println("Finding " + numberOfIterations + " subscribers.");
            for(int i = 0; i < numberOfIterations; i++ ) {
               int r = (int) (myRandom.nextInt(4000));
               find(r);
            }
            long stop = System.nanoTime();
            time = stop - start;
            if (getDebug()) System.out.println("Elapsed time for " + numberOfIterations + " find operations: " + (time/1000000) + "(ms.)");
        }

        public void find(int id)
        {
            Subscriber subscriber = session.find(Subscriber.class, String.valueOf(id));
            if (subscriber == null) {
                //System.out.print(".");
            } else {
                String aImsi = subscriber.getImsi();
                //System.out.print("-" + aImsi);
            }
        }

    }

    @PersistenceCapable(table="subscriber")
    public interface Subscriber {

        @PrimaryKey
        String getImsi();
        void setImsi(String imsi);

        @Index(name="guti_UNIQUE")
        String getGuti();
        void setGuti(String guti);
        
        @Index(name="mme_s1ap_id_UNIQUE")
        int getMme_s1ap_id();
        void setMme_s1ap_id(int mme_s1ap_id);
        
        int getEnb_s1ap_id();
        void setEnb_s1ap_id(int enb_s1ap_id);

        int getMme_teid();
        void setMme_teid(int mme_teid);

        String getSgw_teid();
        void setSgw_teid(String sgw_teid);

        String getPgw_teid();
        void setPgw_teid(String pgw_teid);

        @Index(name="imei_UNIQUE")
        String getImei();
        void setImei(String imei);

        @Index(name="msisdn_UNIQUE")
        String getMsisdn();
        void setMsisdn(String msisdn);

        String getEcm_state();
        void setEcm_state(String ecm_state);

        String getEmm_state();
        void setEmm_state(String emm_state);

        String getEps_cgi();
        void setEps_cgi(String eps_cgi);

        String getGlobal_enb_id();
        void setGlobal_enb_id(String global_enb_id);

        String getBearer_id();
        void setBearer_id(String bearer_id);

        String getSgw_ip_addr();
        void setSgw_ip_addr(String sgw_ip_addr);
  }

}
