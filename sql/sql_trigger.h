/*
  This class holds all information about triggers of table.

  QQ: Will it be merged into TABLE in future ?
*/
class Table_triggers_list: public Sql_alloc
{
  /* Triggers as SPs grouped by event, action_time */
  sp_head           *bodies[3][2];
  /*
    Copy of TABLE::Field array with field pointers set to TABLE::record[1]
    buffer instead of TABLE::record[0] (used for OLD values in on UPDATE
    trigger and DELETE trigger when it is called for REPLACE).
  */
  Field             **record1_field;
  /*
    During execution of trigger new_field and old_field should point to the
    array of fields representing new or old version of row correspondingly
    (so it can point to TABLE::field or to Tale_triggers_list::record1_field)
  */
  Field             **new_field;
  Field             **old_field;
  /* TABLE instance for which this triggers list object was created */
  TABLE *table;
  /*
    Names of triggers.
    Should correspond to order of triggers on definitions_list,
    used in CREATE/DROP TRIGGER for looking up trigger by name.
  */
  List<LEX_STRING>  names_list;

public:
  /*
    Field responsible for storing triggers definitions in file.
    It have to be public because we are using it directly from parser.
  */
  List<LEX_STRING>  definitions_list;

  Table_triggers_list(TABLE *table_arg):
    record1_field(0), table(table_arg)
  {
    bzero((char *)bodies, sizeof(bodies));
  }
  ~Table_triggers_list();

  bool create_trigger(THD *thd, TABLE_LIST *table);
  bool drop_trigger(THD *thd, TABLE_LIST *table);
  bool process_triggers(THD *thd, trg_event_type event,
                        trg_action_time_type time_type,
                        bool old_row_is_record1)
  {
    int res= 0;

    if (bodies[event][time_type])
    {
      bool save_in_sub_stmt= thd->transaction.in_sub_stmt;
#ifndef EMBEDDED_LIBRARY
      /* Surpress OK packets in case if we will execute statements */
      my_bool nsok= thd->net.no_send_ok;
      thd->net.no_send_ok= TRUE;
#endif

      if (old_row_is_record1)
      {
        old_field= record1_field;
        new_field= table->field;
      }
      else
      {
        new_field= record1_field;
        old_field= table->field;
      }

      /*
        FIXME: We should juggle with security context here (because trigger
        should be invoked with creator rights).
      */
      /*
	We disable binlogging, as in SP/functions, even though currently
        triggers can't do updates. When triggers can do updates, someone
        should add such a trigger to rpl_sp.test to verify that the update
        does NOT go into binlog.
      */
      tmp_disable_binlog(thd);
      thd->transaction.in_sub_stmt= TRUE;

      res= bodies[event][time_type]->execute_function(thd, 0, 0, 0);

      thd->transaction.in_sub_stmt= save_in_sub_stmt;
      reenable_binlog(thd);

#ifndef EMBEDDED_LIBRARY
      thd->net.no_send_ok= nsok;
#endif
    }

    return res;
  }

  static bool check_n_load(THD *thd, const char *db, const char *table_name,
                           TABLE *table);

  bool has_delete_triggers()
  {
    return (bodies[TRG_EVENT_DELETE][TRG_ACTION_BEFORE] ||
            bodies[TRG_EVENT_DELETE][TRG_ACTION_AFTER]);
  }

  bool has_before_update_triggers()
  {
    return test(bodies[TRG_EVENT_UPDATE][TRG_ACTION_BEFORE]);
  }

  friend class Item_trigger_field;

private:
  bool prepare_record1_accessors(TABLE *table);
};
