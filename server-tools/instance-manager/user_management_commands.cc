#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "user_management_commands.h"

#include "exit_codes.h"
#include "options.h"
#include "user_map.h"

/*************************************************************************
  Module-specific (internal) functions.
*************************************************************************/

/*
  The function returns user name. The user name is retrieved from command-line
  options (if specified) or from console.

  NOTE
    This function must not be used in user-management command implementations.
    Use get_user_name() instead.

  SYNOPSYS
    get_user_name_impl()

  RETURN
    NULL            on error
    valid pointer   on success
*/

static char *get_user_name_impl()
{
  static char user_name_buf[1024];
  char *ptr;

  if (Options::User_management::user_name)
    return Options::User_management::user_name;

  printf("Enter user name: ");
  fflush(stdout);

  if (!fgets(user_name_buf, sizeof (user_name_buf), stdin))
    return NULL;

  if ((ptr= strchr(user_name_buf, '\n')))
    *ptr= 0;

  if ((ptr= strchr(user_name_buf, '\r')))
    *ptr= 0;

  return user_name_buf;
}


/*
  The function is intended to provide user name for user-management
  operations. It also checks that length of the specified user name is correct
  (not empty, not exceeds USERNAME_LENGTH). Report to stderr if something is
  wrong.

  SYNOPSYS
    get_user_name()
    user_name     [OUT] on success contains user name

  RETURN
    TRUE    on error
    FALSE   on success
*/

static bool get_user_name(LEX_STRING *user_name)
{
  char *user_name_str= get_user_name_impl();

  if (!user_name_str)
  {
    fprintf(stderr, "Error: unable to read user name from stdin.\n");
    return TRUE;
  }

  user_name->str= user_name_str;
  user_name->length= strlen(user_name->str);

  if (user_name->length == 0)
  {
    fprintf(stderr, "Error: user name can not be empty.\n");
    return TRUE;
  }

  if (user_name->length > USERNAME_LENGTH)
  {
    fprintf(stderr, "Error: user name must not exceed %d characters.\n",
            (int) USERNAME_LENGTH);
    return TRUE;
  }

  return FALSE;
}


/*
  The function is intended to provide password for user-management operations.
  The password is retrieved from command-line options (if specified) or from
  console.

  SYNOPSYS
    get_password()

  RETURN
    NULL            on error
    valid pointer   on success
*/

static const char *get_password()
{
  if (Options::User_management::password)
    return Options::User_management::password;

  const char *passwd1= get_tty_password("Enter password: ");
  const char *passwd2= get_tty_password("Re-type password: ");

  if (strcmp(passwd1, passwd2))
  {
    fprintf(stderr, "Error: passwords do not match.\n");
    return 0;
  }

  return passwd1;
}


/*
  Load password file into user map.

  SYNOPSYS
    load_password_file()
    user_map            target user map

  RETURN
    See exit_codes.h for possible values.
*/

static int load_password_file(User_map *user_map)
{
  int err_code;
  const char *err_msg;

  if (user_map->init())
  {
    fprintf(stderr, "Error: can not initialize user map.\n");
    return ERR_OUT_OF_MEMORY;
  }

  if ((err_code= user_map->load(Options::Main::password_file_name, &err_msg)))
    fprintf(stderr, "Error: %s.\n", (const char *) err_msg);

  return err_code;
}


/*
  Save user map into password file.

  SYNOPSYS
    save_password_file()
    user_map            user map

  RETURN
    See exit_codes.h for possible values.
*/

static int save_password_file(User_map *user_map)
{
  int err_code;
  const char *err_msg;

  if ((err_code= user_map->save(Options::Main::password_file_name, &err_msg)))
    fprintf(stderr, "Error: %s.\n", (const char *) err_msg);

  return err_code;
}

/*************************************************************************
  Print_password_line_cmd
*************************************************************************/

int Print_password_line_cmd::execute()
{
  LEX_STRING user_name;
  const char *password;

  printf("Creating record for new user.\n");

  if (get_user_name(&user_name))
    return ERR_CAN_NOT_READ_USER_NAME;

  if (!(password= get_password()))
    return ERR_CAN_NOT_READ_PASSWORD;

  {
    User user(&user_name, password);

    printf("%s:%s\n",
           (const char *) user.user,
           (const char *) user.scrambled_password);
  }

  return ERR_OK;
}


/*************************************************************************
  Add_user_cmd
*************************************************************************/

int Add_user_cmd::execute()
{
  LEX_STRING user_name;
  const char *password;

  User_map user_map;
  User *new_user;

  int err_code;

  if (get_user_name(&user_name))
    return ERR_CAN_NOT_READ_USER_NAME;

  /* Load the password file. */

  if ((err_code= load_password_file(&user_map)) != ERR_OK)
    return err_code;

  /* Check that the user does not exist. */

  if (user_map.find_user(&user_name))
  {
    fprintf(stderr, "Error: user '%s' already exists.\n",
            (const char *) user_name.str);
    return ERR_USER_ALREADY_EXISTS;
  }

  /* Add the user. */

  if (!(password= get_password()))
    return ERR_CAN_NOT_READ_PASSWORD;

  if (!(new_user= new User(&user_name, password)))
    return ERR_OUT_OF_MEMORY;

  if (user_map.add_user(new_user))
  {
    delete new_user;
    return ERR_OUT_OF_MEMORY;
  }

  /* Save the password file. */

  return save_password_file(&user_map);
}


/*************************************************************************
  Drop_user_cmd
*************************************************************************/

int Drop_user_cmd::execute()
{
  LEX_STRING user_name;

  User_map user_map;
  User *user;

  int err_code;

  if (get_user_name(&user_name))
    return ERR_CAN_NOT_READ_USER_NAME;

  /* Load the password file. */

  if ((err_code= load_password_file(&user_map)) != ERR_OK)
    return err_code;

  /* Find the user. */

  user= user_map.find_user(&user_name);

  if (!user)
  {
    fprintf(stderr, "Error: user '%s' does not exist.\n",
            (const char *) user_name.str);
    return ERR_USER_NOT_FOUND;
  }

  /* Remove the user (ignore possible errors). */

  user_map.remove_user(user);

  /* Save the password file. */

  return save_password_file(&user_map);
}


/*************************************************************************
  Edit_user_cmd
*************************************************************************/

int Edit_user_cmd::execute()
{
  LEX_STRING user_name;
  const char *password;

  User_map user_map;
  User *user;

  int err_code;

  if (get_user_name(&user_name))
    return ERR_CAN_NOT_READ_USER_NAME;

  /* Load the password file. */

  if ((err_code= load_password_file(&user_map)) != ERR_OK)
    return err_code;

  /* Find the user. */

  user= user_map.find_user(&user_name);

  if (!user)
  {
    fprintf(stderr, "Error: user '%s' does not exist.\n",
            (const char *) user_name.str);
    return ERR_USER_NOT_FOUND;
  }

  /* Modify user's password. */

  if (!(password= get_password()))
    return ERR_CAN_NOT_READ_PASSWORD;

  user->set_password(password);

  /* Save the password file. */

  return save_password_file(&user_map);
}


/*************************************************************************
  Clean_db_cmd
*************************************************************************/

int Clean_db_cmd::execute()
{
  User_map user_map;

  if (user_map.init())
  {
    fprintf(stderr, "Error: can not initialize user map.\n");
    return ERR_OUT_OF_MEMORY;
  }

  return save_password_file(&user_map);
}


/*************************************************************************
  Check_db_cmd
*************************************************************************/

int Check_db_cmd::execute()
{
  User_map user_map;

  return load_password_file(&user_map);
}


/*************************************************************************
  List_users_cmd
*************************************************************************/

int List_users_cmd::execute()
{
  User_map user_map;

  int err_code;

  /* Load the password file. */

  if ((err_code= load_password_file(&user_map)))
    return err_code;

  /* Print out registered users. */

  {
    User_map::Iterator it(&user_map);
    User *user;

    while ((user= it.next()))
      fprintf(stderr, "%s\n", (const char *) user->user);
  }

  return ERR_OK;
}
