#include "mysql_priv.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "parse_file.h"
#include "sql_acl.h"


static const LEX_STRING triggers_file_type= {(char *)"TRIGGERS", 8};
static const char * const triggers_file_ext= ".TRG";

/*
  Table of .TRG file field descriptors.
  We have here only one field now because in nearest future .TRG
  files will be merged into .FRM files (so we don't need something
  like md5 or created fields).
*/
static File_option triggers_file_parameters[]=
{
  {{(char*)"triggers", 8}, offsetof(Table_triggers_list, definitions_list),
   FILE_OPTIONS_STRLIST},
  {{NULL, 0}, 0, FILE_OPTIONS_STRING}
};


/*
  Create or drop trigger for table.

  SYNOPSIS
    mysql_create_or_drop_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one table for which trigger is created.
      create - whenever we create (true) or drop (false) trigger

  NOTE
    This function is mainly responsible for opening and locking of table and
    invalidation of all its instances in table cache after trigger creation.
    Real work on trigger creation/dropping is done inside Table_triggers_list
    methods.

  RETURN VALUE
    FALSE Success
    TRUE  error
*/
bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create)
{
  TABLE *table;
  bool result= 0;

  DBUG_ENTER("mysql_create_or_drop_trigger");

  /*
    QQ: This function could be merged in mysql_alter_table() function
    But do we want this ?
  */

  if (open_and_lock_tables(thd, tables))
    DBUG_RETURN(TRUE);

  /*
    TODO: We should check if user has TRIGGER privilege for table here.
    Now we just require SUPER privilege for creating/dropping because
    we don't have proper privilege checking for triggers in place yet.
  */
  if (check_global_access(thd, SUPER_ACL))
    DBUG_RETURN(TRUE);

  table= tables->table;

  /*
    We do not allow creation of triggers on views or temporary tables.
    We have to do this check here and not in
    Table_triggers_list::create_trigger() because we want to avoid messing
    with table cash for views and temporary tables.
  */
  if (tables->view || table->tmp_table != NO_TMP_TABLE)
  {
    my_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE, MYF(0), tables->alias);
    DBUG_RETURN(TRUE);
  }

  if (!table->triggers)
  {
    if (!create)
    {
      my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
      DBUG_RETURN(TRUE);
    }

    if (!(table->triggers= new (&table->mem_root) Table_triggers_list()))
      DBUG_RETURN(TRUE);
  }

  /*
    We don't want perform our operations while global read lock is held
    so we have to wait until its end and then prevent it from occuring
    again until we are done. (Acquiring LOCK_open is not enough because
    global read lock is held without helding LOCK_open).
  */
  if (wait_if_global_read_lock(thd, 0, 0))
    DBUG_RETURN(TRUE);

  VOID(pthread_mutex_lock(&LOCK_open));
  result= (create ?
           table->triggers->create_trigger(thd, tables):
           table->triggers->drop_trigger(thd, tables));

  /* It is sensible to invalidate table in any case */
  close_cached_table(thd, table);
  VOID(pthread_mutex_unlock(&LOCK_open));
  start_waiting_global_read_lock(thd);

  if (!result)
    send_ok(thd);

  DBUG_RETURN(result);
}


/*
  Create trigger for table.

  SYNOPSIS
    create_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one open table for which trigger is
               created.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::create_trigger(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  char dir_buff[FN_REFLEN], file_buff[FN_REFLEN];
  LEX_STRING dir, file;
  MEM_ROOT *old_global_root;
  LEX_STRING *trg_def, *name;
  List_iterator_fast<LEX_STRING> it(names_list);

  /* We don't allow creation of several triggers of the same type yet */
  if (bodies[lex->trg_chistics.event][lex->trg_chistics.action_time])
  {
    my_error(ER_TRG_ALREADY_EXISTS, MYF(0));
    return 1;
  }

  /* Let us check if trigger with the same name exists */
  while ((name= it++))
  {
    if (my_strcasecmp(system_charset_info, lex->name_and_length.str,
                      name->str) == 0)
    {
      my_error(ER_TRG_ALREADY_EXISTS, MYF(0));
      return 1;
    }
  }

  /*
    Here we are creating file with triggers and save all triggers in it.
    sql_create_definition_file() files handles renaming and backup of older
    versions
  */
  strxnmov(dir_buff, FN_REFLEN, mysql_data_home, "/", tables->db, "/", NullS);
  dir.length= unpack_filename(dir_buff, dir_buff);
  dir.str= dir_buff;
  file.length=  strxnmov(file_buff, FN_REFLEN, tables->real_name,
                         triggers_file_ext, NullS) - file_buff;
  file.str= file_buff;

  old_global_root= my_pthread_getspecific_ptr(MEM_ROOT*, THR_MALLOC);
  my_pthread_setspecific_ptr(THR_MALLOC, &table->mem_root);

  /*
    Soon we will invalidate table object and thus Table_triggers_list object
    so don't care about place to which trg_def->ptr points and other
    invariants (e.g. we don't bother to update names_list)

    QQ: Hmm... probably we should not care about setting up active thread
        mem_root too.
  */
  if (!(trg_def= (LEX_STRING *)alloc_root(&table->mem_root,
                                          sizeof(LEX_STRING))) ||
      definitions_list.push_back(trg_def))
  {
    my_pthread_setspecific_ptr(THR_MALLOC, old_global_root);
    return 1;
  }

  trg_def->str= thd->query;
  trg_def->length= thd->query_length;

  my_pthread_setspecific_ptr(THR_MALLOC, old_global_root);

  return sql_create_definition_file(&dir, &file, &triggers_file_type,
                                    (gptr)this, triggers_file_parameters, 3);
}


/*
  Drop trigger for table.

  SYNOPSIS
    drop_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one open table for which trigger is
               dropped.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::drop_trigger(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  LEX_STRING *name;
  List_iterator_fast<LEX_STRING> it_name(names_list);
  List_iterator<LEX_STRING>      it_def(definitions_list);

  while ((name= it_name++))
  {
    it_def++;

    if (my_strcasecmp(system_charset_info, lex->name_and_length.str,
                      name->str) == 0)
    {
      /*
        Again we don't care much about other things required for
        clean trigger removing since table will be reopened anyway.
      */
      it_def.remove();

      if (definitions_list.is_empty())
      {
        char path[FN_REFLEN];

        /*
          TODO: Probably instead of removing .TRG file we should move
          to archive directory but this should be done as part of
          parse_file.cc functionality (because we will need it
          elsewhere).
        */
        strxnmov(path, FN_REFLEN, mysql_data_home, "/", tables->db, "/",
                 tables->real_name, triggers_file_ext, NullS);
        unpack_filename(path, path);
        return my_delete(path, MYF(MY_WME));
      }
      else
      {
        char dir_buff[FN_REFLEN], file_buff[FN_REFLEN];
        LEX_STRING dir, file;

        strxnmov(dir_buff, FN_REFLEN, mysql_data_home, "/", tables->db,
                 "/", NullS);
        dir.length= unpack_filename(dir_buff, dir_buff);
        dir.str= dir_buff;
        file.length=  strxnmov(file_buff, FN_REFLEN, tables->real_name,
                               triggers_file_ext, NullS) - file_buff;
        file.str= file_buff;

        return sql_create_definition_file(&dir, &file, &triggers_file_type,
                                          (gptr)this,
                                          triggers_file_parameters, 3);
      }
    }
  }

  my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
  return 1;
}


Table_triggers_list::~Table_triggers_list()
{
  for (int i= 0; i < 3; i++)
    for (int j= 0; j < 2; j++)
      delete bodies[i][j];

  if (old_field)
    for (Field **fld_ptr= old_field; *fld_ptr; fld_ptr++)
      delete *fld_ptr;
}


/*
  Check whenever .TRG file for table exist and load all triggers it contains.

  SYNOPSIS
    check_n_load()
      thd        - current thread context
      db         - table's database name
      table_name - table's name
      table      - pointer to table object

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::check_n_load(THD *thd, const char *db,
                                       const char *table_name, TABLE *table)
{
  char path_buff[FN_REFLEN];
  LEX_STRING path;
  File_parser *parser;
  MEM_ROOT *old_global_mem_root;

  DBUG_ENTER("Table_triggers_list::check_n_load");

  strxnmov(path_buff, FN_REFLEN, mysql_data_home, "/", db, "/", table_name,
           triggers_file_ext, NullS);
  path.length= unpack_filename(path_buff, path_buff);
  path.str= path_buff;

  // QQ: should we analyze errno somehow ?
  if (access(path_buff, F_OK))
    DBUG_RETURN(0);

  /*
    File exists so we got to load triggers
    FIXME: A lot of things to do here e.g. how about other funcs and being
    more paranoical ?
  */

  if ((parser= sql_parse_prepare(&path, &table->mem_root, 1)))
  {
    if (!strncmp(triggers_file_type.str, parser->type()->str,
                 parser->type()->length))
    {
      Field **fld, **old_fld;
      Table_triggers_list *triggers=
        new (&table->mem_root) Table_triggers_list();

      if (!triggers)
        DBUG_RETURN(1);

      if (parser->parse((gptr)triggers, &table->mem_root,
                        triggers_file_parameters, 1))
        DBUG_RETURN(1);

      table->triggers= triggers;

      /*
        We have to prepare array of Field objects which will represent OLD.*
        row values by referencing to record[1] instead of record[0]

        TODO: This could be avoided if there is no ON UPDATE trigger.
      */
      if (!(triggers->old_field=
              (Field **)alloc_root(&table->mem_root, (table->fields + 1) *
                                                     sizeof(Field*))))
        DBUG_RETURN(1);

      for (fld= table->field, old_fld= triggers->old_field; *fld;
           fld++, old_fld++)
      {
        /*
          QQ: it is supposed that it is ok to use this function for field
              cloning...
        */
        if (!(*old_fld= (*fld)->new_field(&table->mem_root, table)))
          DBUG_RETURN(1);
        (*old_fld)->move_field((my_ptrdiff_t)(table->record[1] -
                                              table->record[0]));
      }
      *old_fld= 0;

      List_iterator_fast<LEX_STRING> it(triggers->definitions_list);
      LEX_STRING *trg_create_str, *trg_name_str;
      char *trg_name_buff;
      LEX *old_lex= thd->lex, lex;

      thd->lex= &lex;

      while ((trg_create_str= it++))
      {
        mysql_init_query(thd, (uchar*)trg_create_str->str,
                         trg_create_str->length, true);
        lex.trg_table= table;
        if (yyparse((void *)thd) || thd->is_fatal_error)
        {
          /*
            Free lex associated resources
            QQ: Do we really need all this stuff here ?
          */
          if (lex.sphead)
          {
            if (&lex != thd->lex)
              thd->lex->sphead->restore_lex(thd);
            delete lex.sphead;
          }
          goto err_with_lex_cleanup;
        }

        triggers->bodies[lex.trg_chistics.event]
                             [lex.trg_chistics.action_time]= lex.sphead;
        lex.sphead= 0;

        if (!(trg_name_buff= alloc_root(&table->mem_root,
                                        sizeof(LEX_STRING) +
                                        lex.name_and_length.length + 1)))
          goto err_with_lex_cleanup;

        trg_name_str= (LEX_STRING *)trg_name_buff;
        trg_name_buff+= sizeof(LEX_STRING);
        memcpy(trg_name_buff, lex.name_and_length.str,
               lex.name_and_length.length + 1);
        trg_name_str->str= trg_name_buff;
        trg_name_str->length= lex.name_and_length.length;

	old_global_mem_root= my_pthread_getspecific_ptr(MEM_ROOT*, THR_MALLOC);
	my_pthread_setspecific_ptr(THR_MALLOC, &table->mem_root);

        if (triggers->names_list.push_back(trg_name_str))
          goto err_with_lex_cleanup;

        my_pthread_setspecific_ptr(THR_MALLOC, old_global_mem_root);

        lex_end(&lex);
      }
      thd->lex= old_lex;

      DBUG_RETURN(0);

err_with_lex_cleanup:
      // QQ: anything else ?
      lex_end(&lex);
      thd->lex= old_lex;
      DBUG_RETURN(1);
    }

    /*
      We don't care about this error message much because .TRG files will
      be merged into .FRM anyway.
    */
    my_error(ER_WRONG_OBJECT, MYF(0), table_name, triggers_file_ext, "TRIGGER");
    DBUG_RETURN(1);
  }

  DBUG_RETURN(1);
}
