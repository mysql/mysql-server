import cStringIO
import hashlib
import MySQLdb
import os
import random
import signal
import sys
import threading
import time
import string

CHARS = string.letters + string.digits

def sha1(x):
  return hashlib.sha1(str(x)).hexdigest()

# Should be deterministic given an idx
def get_msg(do_blob, idx):
  random.seed(idx);
  if do_blob:
    blob_length = random.randint(1, 24000)
  else:
    blob_length = random.randint(1, 255)

  if random.randint(1, 2) == 1:
    # blob that cannot be compressed (well, compresses to 85% of original size)
    return ''.join([random.choice(CHARS) for x in xrange(blob_length)])
  else:
    # blob that can be compressed
    return random.choice(CHARS) * blob_length

class PopulateWorker(threading.Thread):
  global LG_TMP_DIR
  def __init__(self, con, start_id, end_id, i):
    threading.Thread.__init__(self)
    self.con = con
    con.autocommit(False)
    self.log = open('/%s/populate-%d.log' % (LG_TMP_DIR, i), 'a')
    self.num = i
    self.start_id = start_id
    self.end_id = end_id
    self.exception = None
    self.start_time = time.time()
    self.start()

  def run(self):
    try:
      self.runme()
      print >> self.log, "ok"
    except Exception, e:
      self.exception = e
      try:
        cursor = self.con.cursor()
        cursor.execute("INSERT INTO errors VALUES('%s')" % e)
      except MySQLdb.Error, e2:
        print >> self.log, "caught while inserting error (%s)" % e2
      print >> self.log, "caught (%s)" % e
    finally:
      self.finish()

  def finish(self):
    print >> self.log, "total time: %.2f s" % (time.time() - self.start_time)
    self.log.close()
    self.con.commit()
    self.con.close()

  def runme(self):
    print >> self.log, "populate thread-%d started" % self.num
    cur = self.con.cursor()
    stmt = None
    for i in xrange(self.start_id, self.end_id):
      msg = get_msg(do_blob, i)
      stmt = """
INSERT INTO t1(id,msg_prefix,msg,msg_length,msg_checksum) VALUES (%d,'%s','%s',%d,'%s')
""" % (i+1, msg[0:255], msg, len(msg), sha1(msg))
      cur.execute(stmt)
      if i % 100 == 0:
        self.con.commit()

def populate_table(con, num_records_before, do_blob, log):
  con.autocommit(False)
  cur = con.cursor()
  stmt = None
  workers = []
  N = num_records_before / 10
  start_id = 0
  for i in xrange(10):
     w = PopulateWorker(MySQLdb.connect(user=user, host=host, port=port, db=db),
                        start_id, start_id + N, i)
     start_id += N
     workers.append(w)

  for i in xrange(start_id, num_records_before):
      msg = get_msg(do_blob, i)
      # print >> log, "length is %d, complen is %d" % (len(msg), len(zlib.compress(msg, 6)))
      stmt = """
INSERT INTO t1(id,msg_prefix,msg,msg_length,msg_checksum) VALUES (%d,'%s','%s',%d,'%s')
""" % (i+1, msg[0:255], msg, len(msg), sha1(msg))
      cur.execute(stmt)

  con.commit()
  for w in workers:
    w.join()
    if w.exception:
      print >>log, "populater thead %d threw an exception" % w.num
      return False
  return True

def get_update(msg, idx):
  return """
UPDATE t1 SET msg_prefix='%s',msg='%s',msg_length=%d,msg_checksum='%s' WHERE id=%d""" % (
msg[0:255], msg, len(msg), sha1(msg), idx)

def get_insert_on_dup(msg, idx):
  return """
INSERT INTO t1 (msg_prefix,msg,msg_length,msg_checksum,id) VALUES ('%s','%s',%d,'%s',%d)
ON DUPLICATE KEY UPDATE
msg_prefix=VALUES(msg_prefix),
msg=VALUES(msg),
msg_length=VALUES(msg_length),
msg_checksum=VALUES(msg_checksum),
id=VALUES(id)""" % (
msg[0:255], msg, len(msg), sha1(msg), idx)

def get_insert(msg, idx):
  return """
INSERT INTO t1 (msg_prefix,msg,msg_length,msg_checksum,id) VALUES ('%s','%s',%d,'%s',%d)""" % (
msg[0:255], msg, len(msg), sha1(msg), idx)

def get_insert_null(msg):
  return """
INSERT INTO t1 (msg_prefix,msg,msg_length,msg_checksum,id) VALUES ('%s','%s',%d,'%s',NULL)""" % (
msg[0:255], msg, len(msg), sha1(msg))

class ChecksumWorker(threading.Thread):
  global LG_TMP_DIR
  def __init__(self, con, checksum):
    threading.Thread.__init__(self)
    self.con = con
    con.autocommit(False)
    self.log = open('/%s/worker-checksum.log' % LG_TMP_DIR, 'a')
    self.checksum = checksum
    print >> self.log, "given checksum=%d" % checksum
    self.start()

  def run(self):
    try:
      self.runme()
      print >> self.log, "ok"
    except Exception, e:
      try:
        cursor = self.con.cursor()
        cursor.execute("INSERT INTO errors VALUES('%s')" % e)
        con.commit()
      except MySQLdb.Error, e2:
        print >> self.log, "caught while inserting error (%s)" % e2

      print >> self.log, "caught (%s)" % e
    finally:
      self.finish()

  def finish(self):
    print >> self.log, "total time: %.2f s" % (time.time() - self.start_time)
    self.log.close()
    self.con.close()

  def runme(self):
    print >> self.log, "checksum thread started"
    self.start_time = time.time()
    cur = self.con.cursor()
    cur.execute("SET SESSION innodb_lra_size=16")
    cur.execute("CHECKSUM TABLE t1")
    checksum = cur.fetchone()[1]
    self.con.commit()
    if checksum != self.checksum:
      print >> self.log, "checksums do not match. given checksum=%d, calculated checksum=%d" % (self.checksum, checksum)
      self.checksum = checksum
    else:
      print >> self.log, "checksums match! (both are %d)" % checksum


class Worker(threading.Thread):
  global LG_TMP_DIR

  def __init__(self, num_xactions, xid, con, server_pid, do_blob, max_id, fake_changes, secondary_checks):
    threading.Thread.__init__(self)
    self.do_blob = do_blob
    self.xid = xid
    con.autocommit(False)
    self.con = con
    self.num_xactions = num_xactions
    cur = self.con.cursor()
    self.rand = random.Random()
    self.rand.seed(xid * server_pid)
    self.loop_num = 0
    self.max_id = max_id

    self.num_primary_select = 0
    self.num_secondary_select = 0
    self.num_secondary_only_select = 0
    self.num_inserts = 0
    self.num_deletes = 0
    self.num_updates = 0
    self.time_spent = 0
    self.log = open('/%s/worker%02d.log' % (LG_TMP_DIR, self.xid), 'a')
    if fake_changes:
        cur.execute("SET innodb_fake_changes=1")
    self.secondary_checks = secondary_checks
    self.start()

  def finish(self):
    print >> self.log, "loop_num:%d, total time: %.2f s" % (
        self.loop_num, time.time() - self.start_time + self.time_spent)
    print >> self.log, "num_primary_select=%d,num_secondary_select=%d,num_secondary_only_select=%d" %\
      (self.num_primary_select, self.num_secondary_select, self.num_secondary_only_select)
    print >> self.log, "num_inserts=%d,num_updates=%d,num_deletes=%d,time_spent=%d" %\
      (self.num_inserts, self.num_updates, self.num_deletes, self.time_spent)
    self.log.close()

  def validate_msg(self, msg_prefix, msg, msg_length, msg_checksum, idx):

    prefix_match = msg_prefix == msg[0:255]

    checksum = sha1(msg)
    checksum_match = checksum == msg_checksum

    len_match = len(msg) == msg_length

    if not prefix_match or not checksum_match or not len_match:
      errmsg = "id(%d), length(%s,%d,%d), checksum(%s,%s,%s) prefix(%s,%s,%s)" % (
          idx,
          len_match, len(msg), msg_length,
          checksum_match, checksum, msg_checksum,
          prefix_match, msg_prefix, msg[0:255])
      print >> self.log, errmsg

      cursor = self.con.cursor()
      cursor.execute("INSERT INTO errors VALUES('%s')" % errmsg)
      cursor.execute("COMMIT")
      raise Exception('validate_msg failed')
    else:
      print >> self.log, "Validated for length(%d) and id(%d)" % (msg_length, idx)

  # Check to see if the idx is in the first column of res_array
  def check_exists(self, res_array, idx):
    for res in res_array:
      if res[0] == idx:
        return True
    return False

  def run(self):
    try:
      self.runme()
      print >> self.log, "ok, with do_blob %s" % self.do_blob
    except Exception, e:

      try:
        cursor = self.con.cursor()
        cursor.execute("INSERT INTO errors VALUES('%s')" % e)
        cursor.execute("COMMIT")
      except MySQLdb.Error, e2:
        print >> self.log, "caught while inserting error (%s)" % e2

      print >> self.log, "caught (%s)" % e
    finally:
      self.finish()

  def runme(self):
    self.start_time = time.time()
    cur = self.con.cursor()
    print >> self.log, "thread %d started, run from %d to %d" % (
        self.xid, self.loop_num, self.num_xactions)

    while not self.num_xactions or (self.loop_num < self.num_xactions):
      idx = self.rand.randint(0, self.max_id)
      insert_or_update = self.rand.randint(0, 3)
      self.loop_num += 1

      # Randomly toggle innodb_prefix_index_cluster_optimization 5% of the time
      if self.rand.randint(0, 20) == 0:
        cur.execute("SET GLOBAL innodb_prefix_index_cluster_optimization=1-@@innodb_prefix_index_cluster_optimization")

      try:
        stmt = None

        msg = get_msg(self.do_blob, idx)

        # Query primary key 70%, secondary key lookup 20%, secondary key only 10%
        r = self.rand.randint(1, 10)
        if r <= 7:
          cur.execute("SELECT msg_prefix,msg,msg_length,msg_checksum FROM t1 WHERE id=%d" % idx)
          res = cur.fetchone()
          self.num_primary_select += 1
        elif r <= 9:
          cur.execute("SELECT msg_prefix,msg,msg_length,msg_checksum FROM t1 WHERE msg_prefix='%s'" % msg[0:255])
          res = cur.fetchone()
          self.num_secondary_select += 1
        # Query only the secondary index
        else:
          cur.execute("SELECT id, msg_prefix FROM t1 WHERE msg_prefix='%s'" % msg[0:255])
          res = cur.fetchall()
          self.num_secondary_only_select += 1
          # have to continue to next iteration since we arn't fetching other data
          continue
        if res:
          self.validate_msg(res[0], res[1], res[2], res[3], idx)

        insert_with_index = False
        if insert_or_update:
          if res:
            if self.rand.randint(0, 1):
              stmt = get_update(msg, idx)
            else:
              stmt = get_insert_on_dup(msg, idx)
              insert_with_index = True
            self.num_updates += 1
          else:
            r = self.rand.randint(0, 2)
            if r == 0:
              stmt = get_insert(msg, idx)
              insert_with_index = True
            elif r == 1:
              stmt = get_insert_on_dup(msg, idx)
              insert_with_index = True
            else:
              stmt = get_insert_null(msg)
            self.num_inserts += 1
        else:
          stmt = "DELETE FROM t1 WHERE id=%d" % idx
          self.num_deletes += 1

        query_result = cur.execute(stmt)

        # 10% probability of checking to see the key exists in secondary index
        if self.secondary_checks and self.rand.randint(1, 10) == 1:
          cur.execute("SELECT id, msg_prefix FROM t1 WHERE msg_prefix='%s'" % msg[0:255])
          res_array = cur.fetchall()
          if insert_or_update:
            if insert_with_index:
              if not self.check_exists(res_array, idx):
                print >> self.log, "Error: Inserted row doesn't exist in secondary index"
                raise Exception("Error: Inserted row doesn't exist in secondary index")
          else:
            if self.check_exists(res_array, idx):
              print >> self.log, "Error: Deleted row still exists in secondary index"
              raise Exception("Error: Deleted row still exists in secondary index")


        if (self.loop_num % 100) == 0:
          print >> self.log, "Thread %d loop_num %d: result %d: %s" % (self.xid,
                                                            self.loop_num, query_result,
                                                            stmt)

        # 30% commit, 10% rollback, 60% don't end the trx
        r = self.rand.randint(1,10)
        if r < 4:
          self.con.commit()
        elif r == 4:
          self.con.rollback()

      except MySQLdb.Error, e:
        if e.args[0] == 2006:  # server is killed
          print >> self.log, "mysqld down, transaction %d" % self.xid
          return
        else:
          print >> self.log, "mysql error for stmt(%s) %s" % (stmt, e)

    try:
      self.con.commit()
    except Exception, e:
      print >> self.log, "commit error %s" % e

if  __name__ == '__main__':
  global LG_TMP_DIR

  pid_file = sys.argv[1]
  kill_db_after = int(sys.argv[2])
  num_records_before = int(sys.argv[3])
  num_workers = int(sys.argv[4])
  num_xactions_per_worker = int(sys.argv[5])
  user = sys.argv[6]
  host = sys.argv[7]
  port = int(sys.argv[8])
  db = sys.argv[9]
  do_blob = int(sys.argv[10])
  max_id = int(sys.argv[11])
  LG_TMP_DIR = sys.argv[12]
  fake_changes = int(sys.argv[13])
  checksum = int(sys.argv[14])
  secondary_checks = int(sys.argv[15])

  checksum_worker = None
  workers = []
  server_pid = int(open(pid_file).read())
  log = open('/%s/main.log' % LG_TMP_DIR, 'a')

#  print  "kill_db_after = ",kill_db_after," num_records_before = ", \
#num_records_before, " num_workers= ",num_workers, "num_xactions_per_worker =",\
#num_xactions_per_worker, "user = ",user, "host =", host,"port = ",port,\
#" db = ", db, " server_pid = ", server_pid

  if num_records_before:
    print >> log, "populate table do_blob is %d" % do_blob
    con = MySQLdb.connect(user=user, host=host, port=port, db=db)
    if not populate_table(con, num_records_before, do_blob, log):
      sys.exit(1)
    con.close()

  if checksum:
    print >> log, "start the checksum thread"
    checksum_worker = ChecksumWorker(MySQLdb.connect(user=user, host=host, port=port, db=db), checksum)
    workers.append(checksum_worker)

  print >> log, "start %d threads" % num_workers
  for i in xrange(num_workers):
    worker = Worker(num_xactions_per_worker, i,
                    MySQLdb.connect(user=user, host=host, port=port, db=db),
                    server_pid, do_blob, max_id, fake_changes, secondary_checks)
    workers.append(worker)

  if kill_db_after:
    print >> log, "kill mysqld"
    time.sleep(kill_db_after)
    os.kill(server_pid, signal.SIGKILL)

  print >> log, "wait for threads"
  for w in workers:
    w.join()

  if checksum_worker and checksum_worker.checksum != checksum:
    print >> log, "checksums do not match. given checksum=%d, calculated checksum=%d" % (checksum, checksum_worker.checksum)
    sys.exit(1)

  print >> log, "all threads done"

