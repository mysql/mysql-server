/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Allow use of the -O variable= option to set long variables */

#include "mysys_priv.h"
#include <m_string.h>
#include <m_ctype.h>

	/* set all changeable variables */

void set_all_changeable_vars(CHANGEABLE_VAR *vars)
{
  for ( ; vars->name ; vars++)
    *vars->varptr= vars->def_value;
}


my_bool set_changeable_varval(const char* var, ulong val,
			      CHANGEABLE_VAR *vars)
{
  char buffer[256];
  sprintf( buffer, "%s=%lu", var, (unsigned long) val );
  return set_changeable_var( buffer, vars );
}


my_bool set_changeable_var(my_string str,CHANGEABLE_VAR *vars)
{
  char endchar;
  my_string end;
  DBUG_ENTER("set_changeable_var");
  DBUG_PRINT("enter",("%s",str));

  if (str)
  {
    if (!(end=strchr(str,'=')))
      fprintf(stderr,"Can't find '=' in expression '%s' to option -O\n",str);
    else
    {
      uint length,found_count=0;
      CHANGEABLE_VAR *var,*found;
      my_string var_end;
      const char *name;
      longlong num;

      /* Skip end space from variable */
      for (var_end=end ; end > str && isspace(var_end[-1]) ; var_end--) ;
      length=(uint) (var_end-str);
      /* Skip start space from argument */
      for (end++ ; isspace(*end) ; end++) ;

      for (var=vars,found=0 ; (name=var->name) ; var++)
      {
	if (!my_casecmp(name,str,length))
	{
	  found=var; found_count++;
	  if (!name[length])
	  {
	    found_count=1;
	    break;
	  }
	}
      }
      if (found_count == 0)
      {
	fprintf(stderr,"No variable match for: -O '%s'\n",str);
	DBUG_RETURN(1);
      }
      if (found_count > 1)
      {
	fprintf(stderr,"Variable prefix '%*s' is not unique\n",length,str);
	DBUG_RETURN(1);
      }

      num=atoll(end); endchar=strend(end)[-1];
      if (endchar == 'k' || endchar == 'K')
	num*=1024;
      else if (endchar == 'm' || endchar == 'M')
	num*=1024L*1024L;
      else if (endchar == 'g' || endchar == 'G')
	num*=1024L*1024L*1024L;
      else if (!isdigit(endchar))
      {
	fprintf(stderr,"Unknown prefix used for variable value '%s'\n",str);
	DBUG_RETURN(1);
      }
      if (num < (longlong) found->min_value)
	num=(longlong) found->min_value;
      else if (num > (longlong) (ulong) found->max_value)
	num=(longlong) (ulong) found->max_value;
      num=((num- (longlong) found->sub_size) / (ulonglong) found->block_size);
      (*found->varptr)= (long) (num*(ulonglong) found->block_size);
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);
}
