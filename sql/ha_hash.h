
int ha_hash::create(my_string name, register TABLE *form,
		    ulonglong auto_increment_value)
{
  register uint i,j;
  char buff[FN_REFLEN];
  KEY *pos;
  H_KEYDEF keydef[MAX_KEY];
  DBUG_ENTER("cre_hash");

  pos=form->key_info;
  for (i=0; i < form->keys ; i++, pos++)
  {
    keydef[i].hk_flag=	 pos->flags & HA_NOSAME;
    for (j=0 ; (int7) j < pos->key_parts ; j++)
    {
      uint flag=pos->key_part[j].key_type;
      if (!f_is_packed(flag) && f_packtype(flag) == (int) FIELD_TYPE_DECIMAL &&
	  !(flag & FIELDFLAG_BINARY))
	keydef[i].hk_keyseg[j].key_type= (int) HA_KEYTYPE_TEXT;
      else
	keydef[i].hk_keyseg[j].key_type= (int) HA_KEYTYPE_BINARY;
      keydef[i].hk_keyseg[j].start=  pos->key_part[j].offset;
      keydef[i].hk_keyseg[j].length= pos->key_part[j].length;
    }
    keydef[i].hk_keyseg[j].key_type= 0;
  }
  DBUG_RETURN(h_create(fn_format(buff,name,"","",2+4+16),i,
		       keydef,form->reclength,form->max_rows,form->min_rows,
		       0));
} /* cre_hash */
