/*
  This class holds all information about triggers of table.

  QQ: Will it be merged into TABLE in future ?
*/
class Table_triggers_list: public Sql_alloc
{
  /* Triggers as SPs grouped by event, action_time */
  sp_head           *bodies[3][2];
  /*
    Copy of TABLE::Field array with field pointers set to old version
    of record, used for OLD values in trigger on UPDATE.
  */
  Field             **old_field;
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

  Table_triggers_list():
    old_field(0)
  {
    bzero((char *)bodies, sizeof(bodies));
  }
  ~Table_triggers_list();

  bool create_trigger(THD *thd, TABLE_LIST *table);
  bool drop_trigger(THD *thd, TABLE_LIST *table);
  bool process_triggers(THD *thd, trg_event_type event,
                        trg_action_time_type time_type)
  {
    int res= 0;

    if (bodies[event][time_type])
    {
      /*
        Similar to function invocation we don't need to surpress sending of
        ok packets here because don't allow execute statements from trigger.

        FIXME: We should juggle with security context here (because trigger
        should be invoked with creator rights).
      */
      res= bodies[event][time_type]->execute_function(thd, 0, 0, 0);
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

  friend class Item_trigger_field;

private:
  bool prepare_old_row_accessors(TABLE *table);
};
