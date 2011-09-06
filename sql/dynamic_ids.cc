#include "dynamic_ids.h"

int cmp_string(const void *id1, const void *id2)
{
  return strcmp((char *) id1, (char *) id2);
}

int cmp_ulong(const void *id1, const void *id2)
{
  return ((*(ulong *) id1) - (* (ulong *)id2));
}

Dynamic_ids::Dynamic_ids(size_t param_size): size(param_size)
{
  my_init_dynamic_array(&dynamic_ids, size, 16, 16);
}

Dynamic_ids::~Dynamic_ids()
{
  delete_dynamic(&dynamic_ids);
}

bool Server_ids::do_unpack_dynamic_ids(char *param_dynamic_ids)
{
  char *token= NULL, *last= NULL;
  uint num_items= 0;
 
  DBUG_ENTER("Server_ids::unpack_dynamic_ids");

  token= strtok_r((char *)const_cast<const char*>(param_dynamic_ids),
                  " ", &last);

  if (token == NULL)
    DBUG_RETURN(TRUE);

  num_items= atoi(token);
  for (uint i=0; i < num_items; i++)
  {
    token= strtok_r(NULL, " ", &last);
    if (token == NULL)
      DBUG_RETURN(TRUE);
    else
    {
      ulong val= atol(token);
      insert_dynamic(&dynamic_ids, (uchar *) &val);
    }
  }
  DBUG_RETURN(FALSE);
}

bool Server_ids::do_pack_dynamic_ids(String *buffer)
{
  DBUG_ENTER("Server_ids::pack_dynamic_ids");

  if (buffer->set_int(dynamic_ids.elements, FALSE, &my_charset_bin))
    DBUG_RETURN(TRUE);

  for (ulong i= 0;
       i < dynamic_ids.elements; i++)
  {
    ulong s_id;
    get_dynamic(&dynamic_ids, (uchar*) &s_id, i);
    if (buffer->append(" ") ||
        buffer->append_ulonglong(s_id))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}

bool Server_ids::do_search_id(const void *id)
{
  return (bsearch((ulong *) id, dynamic_ids.buffer,
          dynamic_ids.elements, size,
          (int (*) (const void*, const void*))
          cmp_ulong) != NULL);
}


bool Database_ids::do_unpack_dynamic_ids(char *param_dynamic_ids)
{
  char *token= NULL, *last= NULL;
  uint num_items= 0;
 
  DBUG_ENTER("Server_ids::unpack_dynamic_ids");

  token= strtok_r((char *)const_cast<const char*>(param_dynamic_ids),
                  " ", &last);

  if (token == NULL)
    DBUG_RETURN(TRUE);

  num_items= atoi(token);
  for (uint i=0; i < num_items; i++)
  {
    token= strtok_r(NULL, " ", &last);
    if (token == NULL)
      DBUG_RETURN(TRUE);
    else
    {
      size_t size= strlen(token);
      if (token[size - 1] == '\n')
      {
        /*
          Remove \n as there may be one when reading from file.
          After improving init_dynarray_intvar_from_file we can
          remove this.
        */
        token[size -1]= '\0';
      }
      insert_dynamic(&dynamic_ids, (uchar *) token);
    }
  }
  DBUG_RETURN(FALSE);
}

bool Database_ids::do_pack_dynamic_ids(String *buffer)
{
  char token[2000];

  DBUG_ENTER("Server_ids::pack_dynamic_ids");

  if (buffer->set_int(dynamic_ids.elements, FALSE, &my_charset_bin))
    DBUG_RETURN(TRUE);

  for (ulong i= 0;
       i < dynamic_ids.elements; i++)
  {
    get_dynamic(&dynamic_ids, (uchar*) token, i);
    if (buffer->append(" ") ||
        buffer->append(token))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}

bool Database_ids::do_search_id(const void *id)
{
  return (bsearch((const char *) id, dynamic_ids.buffer,
          dynamic_ids.elements, size,
          (int (*) (const void*, const void*))
          cmp_string) != NULL);
}
