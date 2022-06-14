import io
import hashlib
import mysql.connector
import os
import random
import signal
import sys
import threading
import time
import string

CHARS = string.ascii_letters + string.digits

def sha1(x):
  return hashlib.sha1(str(x).encode("utf-8")).hexdigest()

# Should be deterministic given an idx
def get_msg(do_blob, idx):
  random.seed(idx);
  if do_blob:
    blob_length = random.randint(1, 24000)
  else:
    blob_length = random.randint(1, 255)

  if random.randint(1, 2) == 1:
    # blob that cannot be compressed (well, compresses to 85% of original size)
    return ''.join([random.choice(CHARS) for x in range(blob_length)])
  else:
    # blob that can be compressed
    return random.choice(CHARS) * blob_length

class PopulateWorker(threading.Thread):
  global LG_TMP_DIR
  def __init__(self, con, start_id, end_id, i):
    threading.Thread.__init__(self)
    self.con = con
    con.autocommit = False
    self.log = open('%s/populate-%d.log' % (LG_TMP_DIR, i), 'a')
    self.num = i
    self.start_id = start_id
    self.end_id = end_id
    self.exception = None
    self.start_time = time.time()
    self.start()

  def run(self):
    try:
      self.runme()
      print("ok", file=self.log)
    except Exception as e:
      self.exception = e
      try:
        cursor = self.con.cursor()
        cursor.execute("INSERT INTO errors VALUES (%s)", (e,))
      except mysql.connector.Error as e2:
        print("caught while inserting error (%s)" % e2,)
      print("caught (%s)" % e)
    finally:
      self.finish()

  def finish(self):
    print("total time: %.2f s" % (time.time() - self.start_time), file=self.log)
    self.log.close()
    self.con.commit()
    self.con.close()

  def runme(self):
    print("populate thread-%d started" % self.num, file=self.log)
    cur = self.con.cursor()
    stmt = None
    for i in range(self.start_id, self.end_id):
      msg = get_msg(do_blob, i)
      stmt = """
INSERT INTO t1(id,msg_prefix,msg,msg_length,msg_checksum) VALUES (%d,'%s','%s',%d,'%s')
""" % (i+1, msg[0:255], msg, len(msg), sha1(msg))
      try:
        cur.execute(stmt)
        if i % 100 == 0:
          self.con.commit()
      except mysql.connector.Error as e2:
        print("caught in insert stmt: (%s)" % e2)

def populate_table(con, num_records_before, do_blob, log):
  con.autocommit = False
  cur = con.cursor()
  stmt = None
  workers = []
  N = num_records_before // 10
  start_id = 0
  for i in range(10):
    # We use raw data as long strings of digits were interpretted by connector as
    # a float and cast to float value of "inf".
    w = PopulateWorker(mysql.connector.connect(user=user, host=host, port=port, db=db, ssl_disabled=True, raw=True),
                       start_id, start_id + N, i)
    start_id += N
    workers.append(w)

  for i in range(start_id, num_records_before):
      msg = get_msg(do_blob, i)
      stmt = """
INSERT INTO t1(id,msg_prefix,msg,msg_length,msg_checksum) VALUES (%d,'%s','%s',%d,'%s')
""" % (i+1, msg[0:255], msg, len(msg), sha1(msg))
      cur.execute(stmt)

  con.commit()
  for w in workers:
    w.join()
    if w.exception:
      print("populater thead %d threw an exception" % w.num)
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
INSERT IGNORE INTO t1 (msg_prefix,msg,msg_length,msg_checksum,id) VALUES ('%s','%s',%d,'%s',%d)""" % (
msg[0:255], msg, len(msg), sha1(msg), idx)

def get_insert_null(msg):
  return """
INSERT IGNORE INTO t1 (msg_prefix,msg,msg_length,msg_checksum,id) VALUES ('%s','%s',%d,'%s',NULL)""" % (
msg[0:255], msg, len(msg), sha1(msg))

class ChecksumWorker(threading.Thread):
  global LG_TMP_DIR
  def __init__(self, con, checksum):
    threading.Thread.__init__(self)
    self.con = con
    con.autocommit = False
    self.log = open('%s/worker-checksum.log' % LG_TMP_DIR, 'a')
    self.checksum = checksum
    print("given checksum=%d" % checksum, file=self.log)
    self.start()

  def run(self):
    try:
      self.runme()
      print("ok", file=self.log)
    except Exception as e:
      try:
        cursor = self.con.cursor()
        cursor.execute("INSERT INTO errors VALUES (%s)", (repr(e),))
        con.commit()
      except mysql.connector.Error as e2:
        print("caught while inserting error (%s)" % repr(e2))

      print("caught (%s)" % repr(e))
    finally:
      self.finish()

  def finish(self):
    print("total time: %.2f s" % (time.time() - self.start_time), file=self.log)
    self.log.close()
    self.con.close()

  def runme(self):
    print("checksum thread started", file=self.log)
    self.start_time = time.time()
    cur = self.con.cursor()
    cur.execute("CHECKSUM TABLE t1")
    checksum = cur.fetchone()[1]
    self.con.commit()
    if checksum != self.checksum:
      print("checksums do not match. given checksum=%d, calculated checksum=%d" % (self.checksum, checksum))
      self.checksum = checksum
    else:
      print("checksums match! (both are %d)" % checksum, file=self.log)


class Worker(threading.Thread):
  global LG_TMP_DIR

  def __init__(self, num_xactions, xid, con, server_pid, do_blob, max_id, fake_changes, secondary_checks):
    threading.Thread.__init__(self)
    self.do_blob = do_blob
    self.xid = xid
    con.autocommit = False
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
    self.log = open('%s/worker%02d.log' % (LG_TMP_DIR, self.xid), 'a')
    self.secondary_checks = secondary_checks
    self.start()

  def finish(self):
    print("loop_num:%d, total time: %.2f s" % (
        self.loop_num, time.time() - self.start_time + self.time_spent), file=self.log)
    print("num_primary_select=%d,num_secondary_select=%d,num_secondary_only_select=%d" %\
      (self.num_primary_select, self.num_secondary_select, self.num_secondary_only_select), file=self.log)
    print("num_inserts=%d,num_updates=%d,num_deletes=%d,time_spent=%d" %\
      (self.num_inserts, self.num_updates, self.num_deletes, self.time_spent), file=self.log)
    self.log.close()

  def validate_msg(self, msg_prefix, msg, msg_length, msg_checksum, idx):

    prefix_match = msg_prefix == msg[0:255]

    checksum = sha1(msg)
    checksum_match = checksum == msg_checksum

    len_match = len(msg) == msg_length

    if not prefix_match or not checksum_match or not len_match:
      errmsg = "id(%d), length(%s,%d,%d), checksum(%s,%s,%s), prefix(%s,%s,%s)" % (
          idx,
          len_match, len(msg), msg_length,
          checksum_match, checksum, msg_checksum,
          prefix_match, msg_prefix, msg[0:255])
      print(errmsg)

      cursor = self.con.cursor()
      cursor.execute("INSERT INTO errors VALUES (%s)", (errmsg,))
      cursor.execute("COMMIT")
      raise Exception('validate_msg failed')
    else:
      print("Validated for length(%d) and id(%d)" % (msg_length, idx), file=self.log)

  # Check to see if the idx is in the first column of res_array
  def check_exists(self, res_array, idx):
    for res in res_array:
      if res[0] == idx:
        return True
    return False

  def run(self):
    try:
      self.runme()
      print("ok, with do_blob %s" % self.do_blob, file=self.log)
    except Exception as e:
      try:
        cursor = self.con.cursor()
        cursor.execute("INSERT INTO errors VALUES (%s)", (repr(e),))
        cursor.execute("COMMIT")
      except mysql.connector.Error as e2:
        print("caught while inserting errors: (%s)\nThe message to store: %s" % (repr(e2), repr(e)))

      print("caught (%s)" % repr(e))
    finally:
      self.finish()

  def runme(self):
    self.start_time = time.time()
    cur = self.con.cursor(buffered=True)
    print("thread %d started, run from %d to %d" % (
        self.xid, self.loop_num, self.num_xactions), file=self.log)

    while not self.num_xactions or (self.loop_num < self.num_xactions):
      idx = self.rand.randint(0, self.max_id)
      insert_or_update = self.rand.randint(0, 3)
      self.loop_num += 1

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
          self.validate_msg(res[0].decode(), res[1].decode(), int(res[2], 10), res[3].decode(), idx)

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

        cur.execute(stmt)

        # 10% probability of checking to see the key exists in secondary index
        if self.secondary_checks and self.rand.randint(1, 10) == 1:
          cur.execute("SELECT id, msg_prefix FROM t1 WHERE msg_prefix='%s'" % msg[0:255])
          res_array = cur.fetchall()
          if insert_or_update:
            if insert_with_index:
              if not self.check_exists(res_array, idx):
                print("Error: Inserted row doesn't exist in secondary index")
                raise Exception("Error: Inserted row doesn't exist in secondary index")
          else:
            if self.check_exists(res_array, idx):
              print("Error: Deleted row still exists in secondary index")
              raise Exception("Error: Deleted row still exists in secondary index")


        if (self.loop_num % 100) == 0:
          print("Thread %d loop_num %d: result: %s" % (self.xid,
                                                            self.loop_num,
                                                            stmt), file=self.log)

        # 30% commit, 10% rollback, 60% don't end the trx
        r = self.rand.randint(1,10)
        if r < 4:
          try:
            self.con.commit()
          except mysql.connector.Error as e:
            print("Error: error during commit: %s" % e)
        elif r == 4:
          try:
            self.con.rollback()
          except mysql.connector.Error as e:
            print("Error: error during rollback: %s" % e)

      except mysql.connector.Error as e:
        if e.args[0] in [2006, 2013, 2055]:  # server is killed
          print("mysqld down, transaction %d" % self.xid)
          return
        elif e.args[0] in [1213, 1205]:    # deadlock or lock wait timeout, ignore
          return
        else:
          try:
            cursor = self.con.cursor()
            cursor.execute("INSERT INTO errors VALUES (%s)", ("%s -- %s" % (e, stmt),))
          except mysql.connector.Error as e2:
            print("caught while inserting error (%s)" % e2)
          print("mysql error for stmt(%s) %s" % (stmt, e))

    try:
      self.con.commit()
    except Exception as e:
      print("commit error %s" % repr(e))

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
  fake_changes = int(sys.argv[13]) # not used
  checksum = int(sys.argv[14])
  secondary_checks = int(sys.argv[15])

  checksum_worker = None
  workers = []
  server_pid = int(open(pid_file).read())
  log = open('%s/main.log' % LG_TMP_DIR, 'a')

#  print  "kill_db_after = ",kill_db_after," num_records_before = ", \
#num_records_before, " num_workers= ",num_workers, "num_xactions_per_worker =",\
#num_xactions_per_worker, "user = ",user, "host =", host,"port = ",port,\
#" db = ", db, " server_pid = ", server_pid

  if num_records_before:
    print("populate table do_blob is %d" % do_blob, file=log)
    # We use raw data as long strings of digits were interpretted by connector as
    # a float and cast to float value of "inf".
    con = mysql.connector.connect(user=user, host=host, port=port, db=db, ssl_disabled=True, raw=True)
    if not populate_table(con, num_records_before, do_blob, log):
      sys.exit(1)
    con.close()

  if checksum:
    print("start the checksum thread", file=log)
    # We use raw data as long strings of digits were interpretted by connector as
    # a float and cast to float value of "inf".
    checksum_worker = ChecksumWorker(mysql.connector.connect(user=user, host=host, port=port, db=db,
                                                             ssl_disabled=True), checksum, raw=True)
    workers.append(checksum_worker)

  print("start %d threads" % num_workers, file=log)
  for i in range(num_workers):
    # We use raw data as long strings of digits were interpretted by connector as
    # a float and cast to float value of "inf".
    worker = Worker(num_xactions_per_worker, i,
                    mysql.connector.connect(user=user, host=host, port=port, db=db, ssl_disabled=True, raw=True),
                    server_pid, do_blob, max_id, fake_changes, secondary_checks)
    workers.append(worker)

  if kill_db_after:
    print("kill mysqld", file=log)
    time.sleep(kill_db_after)
    if hasattr(signal, 'SIGKILL'):
      os.kill(server_pid, signal.SIGKILL)
    else:
      # On Windows only SIGTERM is available.
      os.kill(server_pid, signal.SIGTERM)

  print("wait for threads", file=log)
  for w in workers:
    w.join()

  if checksum_worker and checksum_worker.checksum != checksum:
    print("checksums do not match. given checksum=%d, calculated checksum=%d" % (checksum, checksum_worker.checksum), file=log)
    sys.exit(1)

  print("all threads done", file=log)

