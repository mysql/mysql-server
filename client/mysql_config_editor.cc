/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  MySQL Configuration Utility
*/

/* ORACLE_WELCOME_COPYRIGHT_NOTICE */
#include "my_config.h"
#include <welcome_copyright_notice.h>
#include <signal.h>
#include <my_dir.h>
#include <my_rnd.h>
#include "my_aes.h"
#include "client_priv.h"
#include "my_default.h"
#include "my_default_priv.h"

#define MYSQL_CONFIG_EDITOR_VERSION "1.0"
#define MY_LINE_MAX 4096
/*
  Header length for the login file.
  4-byte (unused) + LOGIN_KEY_LEN
 */
#define MY_LOGIN_HEADER_LEN (4 + LOGIN_KEY_LEN)

static int g_fd;

/*
  Length of the contents in login file
  excluding the header part.
*/
static size_t file_size;
static char *opt_user= NULL, *opt_password= NULL, *opt_host=NULL,
            *opt_login_path= NULL;

static char my_login_file[FN_REFLEN];
static char my_key[LOGIN_KEY_LEN];

static my_bool opt_verbose, opt_all, tty_password= 0, opt_warn,
               login_path_specified= TRUE;

int execute_commands(int argc, char **argv);
static void print_login_path(DYNAMIC_STRING *file_buf, const char *path_name);
static void remove_login_path(DYNAMIC_STRING *file_buf, const char *path_name);
static char* locate_login_path(DYNAMIC_STRING *file_buf, const char *path_name);
static my_bool check_and_create_login_file(void);
static void mask_password_and_print(char *buf);
static void reset_login_file(bool gen_key);

static int encrypt_buffer(const char *plain, int plain_len, char cipher[]);
static int decrypt_buffer(const char *cipher, int cipher_len, char plain[]);
static int encrypt_and_write_file(DYNAMIC_STRING *file_buf);
static int read_and_decrypt_file(DYNAMIC_STRING *file_buf);
void generate_login_key(void);
int read_login_key(void);
int add_header(void);

static void verbose_msg(const char *fmt, ...);
static void print_version(void);
static void usage(void);
extern "C" my_bool get_one_option(int optid, const struct my_option *opt,
                                  char *argument);


/*
  The order of commands must be the same as command_names,
  except MY_CONFIG_ERROR.
*/
enum commands {
  MY_CONFIG_ERROR,
  MY_CONFIG_SET,
  MY_CONFIG_REMOVE,
  MY_CONFIG_PRINT,
  MY_CONFIG_RESET,
  MY_CONFIG_HELP
};

static const char *command_names[]= {
  "set",
  "remove",
  "print",
  "reset",
  "help",
  NullS
};

static TYPELIB command_typelib=
{ array_elements(command_names)-1, "commands", command_names, NULL};

static struct my_option my_long_options[] =
{
  {"all", OPT_CONFIG_ALL, "Used with print command to print all login paths.",
    &opt_all, &opt_all, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Host name to be entered into the login file.", &opt_host,
    &opt_host, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"login-path", 'G', "Name of the login path to use in the login file.",
    &opt_login_path, &opt_login_path, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
    0, 0, 0},
  {"password", 'p', "Prompt for password to be entered into the login file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User name to be entered into the login file.", &opt_user,
    &opt_user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Write more information.", &opt_verbose,
   &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"warn", 'w', "Warn and ask for confirmation if set command attempts to "
   "overwrite an existing login path (enabled by default).",
   &opt_warn, &opt_warn, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o,/tmp/mysql_config_editor.trace");
    break;
  case 'p':
    tty_password= 1;
    break;
  case 'V':
    print_version();
    exit(0);
    break;
  case '?':
    usage();
    exit(0);
    break;
  }
  return 0;
}


int main(int argc, char *argv[])
{
  DBUG_ENTER("main");
  int error;

  MY_INIT(argv[0]);

  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
  {
    exit(error);
  }

  if (argc == 0)
  {
    usage();
    exit(1);
  }

  if (tty_password)
    opt_password= get_tty_password(NullS);

  /* if not, set it to 'client' (default) */
  if (!opt_login_path)
  {
    login_path_specified= FALSE;
    opt_login_path= my_strdup("client", MYF(MY_WME));
  }

  error= execute_commands(argc, argv);
  exit(error ? 1 : 0);
  DBUG_RETURN(0);
}


int execute_commands(int argc, char **argv)
{
  DBUG_ENTER("execute_commands");
  DYNAMIC_STRING file_buf, path_buf;
  int error= 0;

  init_dynamic_string(&path_buf, "", MY_LINE_MAX, MY_LINE_MAX);
  init_dynamic_string(&file_buf, "", file_size, 3 * MY_LINE_MAX);

  for (; argc > 0; argv++, argc--)
  {
    if ((error= check_and_create_login_file()))
      goto done;

    switch(find_type(argv[0], &command_typelib, FIND_TYPE_BASIC)) {
      case MY_CONFIG_SET :
        verbose_msg("Executing set command.\n");
        if (file_size)
        {
          if (read_and_decrypt_file(&file_buf) == -1)
            goto done;
        }

        dynstr_append(&path_buf, "[");          /* --login=path */
        if (opt_login_path)
          dynstr_append(&path_buf, opt_login_path);
        else
          dynstr_append(&path_buf, "client");
        dynstr_append(&path_buf, "]");

        if (opt_user)                           /* --user */
        {
          dynstr_append(&path_buf, "\nuser = ");
          dynstr_append(&path_buf, opt_user);
        }

        if (opt_password)                       /* --password */
        {
          dynstr_append(&path_buf, "\npassword = ");
          dynstr_append(&path_buf, opt_password);
        }

        if (opt_host)                           /* --host */
        {
          dynstr_append(&path_buf, "\nhost = ");
          dynstr_append(&path_buf, opt_host);
        }

        dynstr_append(&path_buf, "\n");

        /* Warn if login path already exists */
        if (opt_warn && ((locate_login_path (&file_buf, opt_login_path))
                         != NULL))
        {
          int choice;
          printf ("WARNING : \'%s\' path already exists and will be "
                  "overwritten. \n Continue? (Press y|Y for Yes, any "
                  "other key for No) : ",
                  opt_login_path);
          choice= getchar();

          if (choice != (int) 'y' && choice != (int) 'Y')
            break;                              /* skip */
        }

        /* Remove the login path. */
        remove_login_path(&file_buf, opt_login_path);

        /* Append the new login path to the file buffer. */
        dynstr_append(&file_buf, path_buf.str);

        encrypt_and_write_file(&file_buf);
        break;

      case MY_CONFIG_REMOVE :
        verbose_msg("Executing remove command.\n");
        if (file_size)
        {
          if (read_and_decrypt_file(&file_buf) == -1)
            goto done;
        }
        else
          break;                                /* Nothing to remove, skip.. */

        /* Warn if no login path is specified. */
        if (opt_warn &&
            ((locate_login_path (&file_buf, opt_login_path)) != NULL) &&
            (login_path_specified == FALSE)
            )
        {
          int choice;
          printf ("WARNING : No login path specified, so default login path "
                  "will be removed. \n Continue? (Press y|Y for Yes, any "
                  "other key for No) : ");
          choice= getchar();

          if (choice != (int) 'y' && choice != (int) 'Y')
            break;                              /* skip */
        }

        remove_login_path(&file_buf, opt_login_path);

        encrypt_and_write_file(&file_buf);
        break;

      case MY_CONFIG_PRINT :
        verbose_msg("Executing print command.\n");
        if (file_size)
        {
          if (read_and_decrypt_file(&file_buf) == -1)
            goto done;
        }
        else
          break;                                /* Nothing to print, skip..*/

        print_login_path(&file_buf, opt_login_path);
        break;

      case MY_CONFIG_RESET :
        verbose_msg("Resetting login file.\n");
        reset_login_file(1);
        break;

      case MY_CONFIG_HELP :
        verbose_msg("Printing usage info.\n");
        usage();
        break;

      default :
        printf("Error! Unknown command.\n");
        exit(1);
    }
    dynstr_trunc(&file_buf, 0);
    dynstr_trunc(&path_buf, 0);

    my_close(g_fd, MYF(MY_WME));
  }

done:
  dynstr_free(&file_buf);
  dynstr_free(&path_buf);

  DBUG_RETURN(error);
}


/**
  Create the login file if it does not exist, check
  and set its permissions and modes.

  @param void

  @return 1               error
          0               success
*/

static my_bool check_and_create_login_file(void)
{
  DBUG_ENTER("check_and_create_login_file");

  MY_STAT stat_info;

// This is a hack to make it compile. File permissions are different on Windows.
#ifdef _WIN32
#define S_IRUSR  00400
#define S_IWUSR  00200
#define S_IRWXU  00700
#define S_IRWXG  00070
#define S_IRWXO  00007
#endif

  const int access_flag= (O_RDWR | O_BINARY);
  const ushort create_mode= (S_IRUSR | S_IWUSR );

  /* Get the login file name. */
  if (! my_default_get_login_file(my_login_file, sizeof(my_login_file)))
  {
    verbose_msg("Error! Failed to set login file name.\n");
    goto error;
  }

  /*
    NOTE : MYSQL_TEST_LOGIN_FILE env must be a full path,
    where the directory structure must exist. However the
    login file will be created if it does not exist.
  */
#ifdef _WIN32
  if (! (getenv("MYSQL_TEST_LOGIN_FILE")))
  {
    /* Check if 'MySQL' directory is in place. */
    MY_STAT stat_info_dir;
    char login_dir[FN_REFLEN];
    size_t size;

    dirname_part(login_dir, my_login_file, &size);
    /* Remove the trailing '\' */
    if (login_dir[-- size] == FN_LIBCHAR)
      login_dir[size]= 0;

    /* Now check if directory exists? */
    if ( my_stat(login_dir, &stat_info_dir, MYF(0)))
    {
      verbose_msg("%s directory exists.\n", login_dir);
    }
    else
    {
      /* Create the login directory. */
      verbose_msg("%s directory doesn't exist, creating it.\n", login_dir);
      if (my_mkdir(login_dir, 0, MYF(0)))
        goto error;
    }
  }
#endif

  /* Check for login file's existence and permissions (0600). */
  if (my_stat(my_login_file, &stat_info, MYF(0)))
  {
    verbose_msg("File exists.\n");

    file_size= stat_info.st_size;

#ifdef _WIN32
    if (1)
#else
    if (!(stat_info.st_mode & (S_IXUSR | S_IRWXG | S_IRWXO)))
#endif
    {
      verbose_msg("File has the required permission.\nOpening the file.\n");
      if ((g_fd= my_open(my_login_file, access_flag, MYF(MY_WME))) == -1)
      {
        verbose_msg("Error! Couldn't open the file.\n");
        goto error;
      }
    }
    else
    {
      verbose_msg("File does not have the required permission.\n");
      if (opt_warn)
        printf ("WARNING : Login file does not have the required"
                " file permissions.\nPlease set the mode to 600 &"
                " run the command again.\n");
      goto error;
    }
  }
  else
  {
    verbose_msg("File does not exist.\nCreating login file.\n");
    if ((g_fd= my_create(my_login_file, create_mode, access_flag,
                       MYF(MY_WME)) == -1))
    {
      verbose_msg("Error! Couldn't create the login file.\n");
      goto error;
    }
    else
    {
      verbose_msg("Login file created.\n");
      verbose_msg("Opening the file.\n");

      if((g_fd= my_open(my_login_file, access_flag, MYF(MY_WME))) == -1)
      {
        verbose_msg("Error! couldn't open the file.\n");
        goto error;
      }
      file_size= 0;
    }
  }

  if (file_size == 0)
  {
    generate_login_key();
    if(add_header() == -1)
      goto error;
  }
  else
  {
    if (read_login_key() == -1)
      goto error;
  }

  DBUG_RETURN(0);

error:
  DBUG_RETURN(1);
}


/**
  Print options under the specified login path. If '--all'
  option is used, print all the optins stored in the login
  file.

  @param file_buf  [in]   Buffer storing the unscrambled login
                          file contents.
  @param path_name [in]   Path name.

  @return                 void

*/

static void print_login_path(DYNAMIC_STRING *file_buf, const char *path_name)
{
  DBUG_ENTER("print_login_path");

  char *start= NULL, *end= NULL, temp= '\0';

  if (file_buf->length == 0)
    goto done;                                  /* Nothing to print. */

  if (opt_all)
  {
    start= file_buf->str;
    end= file_buf->str + file_buf->length;
  }
  else
  {
    start= locate_login_path(file_buf, path_name);
    if (! start)
      /* login path not found, skip..*/
      goto done;

    end= strstr(start, "\n[");
  }

  if (end)
  {
    temp= *end;
    *end= '\0';
  }

  mask_password_and_print(start);

  if (temp != '\0')
    *end= temp;

done:
  DBUG_VOID_RETURN;
}


/**
  Print the specified buffer by masking the actual
  password string.

  @param buf [in]         Buffer to be printed.

  @raturn                 void
*/

static void mask_password_and_print(char *buf)
{
  DBUG_ENTER("mask_password_and_print");
  const char *password_str= "\npassword = ", *mask = "*****";
  char *next= NULL;

  while ((next= strstr(buf, password_str)) != NULL)
  {
    while ( *buf != 0 && buf != next)
      putc( *(buf ++), stdout);
    printf("%s", password_str);
    printf("%s\n", mask);
    if (*buf == '\n')                           /* Move past \n' */
      buf ++;

    /* Ignore the password. */
    while( *buf && *(buf ++) != '\n')
    {}

    if ( !opt_all)
      break;
  }

  /* Now print the rest of the buffer. */
  while ( *buf) putc( *(buf ++), stdout);
  // And a new line.. if required.
  if (* (buf - 1) != '\n')
    putc('\n', stdout);

  DBUG_VOID_RETURN;
}


/**
  Remove the specified login path from the login file.

  @param file_buf  [in]   Buffer storing the unscrambled login
                          file contents.
  @param path_name [in]   Path name.

  @return                 void

*/

static void remove_login_path(DYNAMIC_STRING *file_buf, const char *path_name)
{
  DBUG_ENTER("remove_login_path");

  char *start=NULL, *end= NULL;
  int tot_len, len, diff;

  if((start= locate_login_path(file_buf, path_name)) == NULL)
    /* login path was not found, skip.. */
    goto done;

  end= strstr(start, "\n[");

  if (end)
  {
    end ++;                                     /* Move past '\n' */
    len= ((diff= (start - end)) > 0) ? diff : - diff;
    tot_len= file_buf->length - len ;
  }
  else
  {
    *start= '\0';
    file_buf->length= ((diff= (file_buf->str - start)) > 0) ? diff : - diff;
    goto done;
  }

  while(tot_len --)
    *(start ++)= *(end ++);

  *start= '\0';
  file_buf->length -= len;

done:
  DBUG_VOID_RETURN;
}

/**
  Remove all the contents from the login file.

  @param gen_key [in]     Flag to control the generation of
                          a new key.

  @return                 void
*/

static void reset_login_file(bool gen_key)
{
  DBUG_ENTER("reset_login_file");

  if (my_chsize(g_fd, 0, 0, MYF(MY_WME)))
    verbose_msg("Error while truncating the file.\n");

  /* Seek to the beginning of the file. */
  my_seek(g_fd, 0L, SEEK_SET, MYF(MY_WME));

  if (gen_key)
    generate_login_key();                       /* Generate a new key. */

  add_header();

  file_size= 0;

  DBUG_VOID_RETURN;
}


/**
  Find the specified login path in the login file buffer
  and return the starting address.

  @param file_buf  [in]   Buffer storing the unscrambled login
                          file contents.
  @param path_name [in]   Path name.

  @return                 If found, the starting address of the
                          login path, NULL otherwise.
*/

static char* locate_login_path(DYNAMIC_STRING *file_buf, const char *path_name)
{
  DBUG_ENTER("locate_login_path");

  char *addr= NULL;
  DYNAMIC_STRING dy_path_name;

  init_dynamic_string(&dy_path_name, "", 512, 512);

  dynstr_append(&dy_path_name, "\n[");
  dynstr_append(&dy_path_name, path_name);
  dynstr_append(&dy_path_name, "]");

  /* First check if it is the very first login path. */
  if (file_buf->str == strstr(file_buf->str, dy_path_name.str + 1))
    addr= file_buf->str;
  /* If not, scan through the file. */
  else
  {
    addr= strstr(file_buf->str, dy_path_name.str);
    if (addr)
      addr ++;                                  /* Move past '\n' */
  }

  dynstr_free(&dy_path_name);
  DBUG_RETURN(addr);
}


/**
  Encrypt the file buffer and write it to the login file.

  @param file_buf  [in]   Buffer storing the unscrambled login
                          file contents.

  @return                 -1, if error encountered;
                          bytes written to the file, otherwise.

  @note The contents of the file buffer are encrypted
        on a line-by-line basis with each line having
        the following format :
        [<first 4 bytes store cipher-length>|<Next cipher-length
        bytes store actual cipher>]
*/

static int encrypt_and_write_file(DYNAMIC_STRING *file_buf)
{
  DBUG_ENTER("encrypt_and_write_file");

  my_bool done= FALSE;
  char cipher[MY_LINE_MAX], *tmp= NULL;
  uint bytes_read=0, len= 0;
  int enc_len= 0;                               // Can be negative.

  reset_login_file(0);

  /* Move past key first. */
  if (my_seek(g_fd, MY_LOGIN_HEADER_LEN, SEEK_SET, MYF(MY_WME))
      != (MY_LOGIN_HEADER_LEN))
    DBUG_RETURN(-1);                            /* Error while seeking. */


  tmp= &file_buf->str[bytes_read];

  while(! done)
  {
    len= 0;

    while(*tmp++ != '\n')
      if (len < (file_buf->length - bytes_read))
        len ++;
      else
      {
        done= TRUE;
        break;
      }

    if (done)
      break;

    if ((enc_len= encrypt_buffer(&file_buf->str[bytes_read],
                                 ++ len, cipher + MAX_CIPHER_STORE_LEN)) < 0)
    {
      verbose_msg("Error! failed to encrypt the login file buffer.\n");
      goto error;
    }

    bytes_read += len;

    if (enc_len > MY_LINE_MAX)
      goto error;

    /* Store cipher length first. */
    int4store(cipher, enc_len);

    if ((my_write(g_fd, (const uchar *)cipher, enc_len + MAX_CIPHER_STORE_LEN,
                  MYF(MY_WME))) != (enc_len + MAX_CIPHER_STORE_LEN))
    {
      verbose_msg("Error! couldn't write to the file.\n");
      goto error;
    }
  }

  verbose_msg("Successfully written encrypted data to the login file.\n");

  /* Update file_size */
  file_size= bytes_read;

  DBUG_RETURN(file_size);

error:
  DBUG_RETURN(-1);
}


/**
  Read the login file, unscramble its contents and store
  them into the file buffer.

  @param file_buf  [in]   Buffer for storing the unscrambled login
                          file contents.

  @return                 -1, if error encountered;
                          total length of plain content added to
                          the file buffer, otherwise.

*/

static int read_and_decrypt_file(DYNAMIC_STRING *file_buf)
{
  DBUG_ENTER("read_and_decrypt_file");

  char cipher[MY_LINE_MAX], plain[MY_LINE_MAX];
  uchar len_buf[MAX_CIPHER_STORE_LEN];
  int cipher_len= 0, dec_len= 0, total_len= 0;

  /* Move past key first. */
  if (my_seek(g_fd, MY_LOGIN_HEADER_LEN, SEEK_SET, MYF(MY_WME))
      != (MY_LOGIN_HEADER_LEN))
    DBUG_RETURN(-1);                            /* Error while seeking. */

  /* First read the length of the cipher. */
  while (my_read(g_fd, len_buf, MAX_CIPHER_STORE_LEN,
                 MYF(MY_WME)) == MAX_CIPHER_STORE_LEN)
  {
    cipher_len= sint4korr(len_buf);

    if (cipher_len > MY_LINE_MAX)
      DBUG_RETURN(-1);

    /* Now read 'cipher_len' bytes from the file. */
    if ((int) my_read(g_fd, (uchar *) cipher, cipher_len, MYF(MY_WME)) == cipher_len)
    {
      if ((dec_len= decrypt_buffer(cipher, cipher_len, plain)) < 0)
      {
        verbose_msg("Error! failed to decrypt the file.\n");
        DBUG_RETURN(-1);
      }

      total_len += dec_len;
      plain[dec_len]= 0;
      dynstr_append(file_buf, plain);
    }
  }

  verbose_msg("Successfully decrypted the login file.\n");
  DBUG_RETURN(total_len);
}


/**
  Encrypt the given plain text.

  @param plain     [in]   Plain text to be encrypted.
  @param plain_len [in]   Length of the plain text.
  @param cipher    [in]   Encrypted cipher text.

  @return                 -1 if error encountered,
                          length encrypted, otherwise.
*/

static int encrypt_buffer(const char *plain, int plain_len, char cipher[])
{
  DBUG_ENTER("encrypt_buffer");
  int aes_len;

  aes_len= my_aes_get_size(plain_len);

  if (my_aes_encrypt(plain, plain_len, cipher, my_key, LOGIN_KEY_LEN) == aes_len)
  {
    DBUG_RETURN(aes_len);
  }
  else
  {
    fprintf(stderr, "Error! failed to encrypt.\n");
  }
  DBUG_RETURN(-1);                              /* Error */
}


/**
  Decrypt the given cipher text.

  @param cipher     [in]  Cipher text to be decrypted.
  @param cipher_len [in]  Length of the cipher text.
  @param plain      [in]  Decrypted plain text.

  @return                 -1 if error encountered,
                          length decrypted, otherwise.
*/

static int decrypt_buffer(const char *cipher, int cipher_len, char plain[])
{
  DBUG_ENTER("decrypt_buffer");
  int aes_length;

  if ((aes_length= my_aes_decrypt(cipher, cipher_len, (char *) plain,
                                  my_key, LOGIN_KEY_LEN)) > 0)
  {
    DBUG_RETURN(aes_length);
  }
  else
  {
    fprintf(stderr, "Error! failed to decrypt.\n");
  }

  DBUG_RETURN(-1);                              /* Error */
}


/**
  Add unused bytes alongwith the to the login key
  to the login file.

  @return                 -1 if error encountered,
                          length written, otherwise.
*/

int add_header(void)
{
  DBUG_ENTER("add_header");

  /* Reserved for future use. */
  const char unused[]= {'\0','\0','\0','\0'};

  /* Write 'unused' bytes first. */
  if ((my_write(g_fd, (const uchar *) unused, 4, MYF(MY_WME))) != 4)
  {
    verbose_msg("Error! couldn't write to the file.\n");
    goto error;
  }

  /* Write the login key. */
  if ((my_write(g_fd, (const uchar *)my_key, LOGIN_KEY_LEN, MYF(MY_WME)))
      != LOGIN_KEY_LEN)
  {
    verbose_msg("Error! couldn't write to the file.\n");
    goto error;
  }

  verbose_msg("Key successfully written to the file.\n");
  DBUG_RETURN(MY_LOGIN_HEADER_LEN);

error:
  DBUG_RETURN(-1);
}


/**
  Algorithm to generate key.
*/

void generate_login_key()
{
  DBUG_ENTER("generate_login_key");
  struct rand_struct rnd;

  verbose_msg("Generating a new key.\n");
  /* Get a sequence of random non-printable ASCII */
  for (uint i= 0; i < LOGIN_KEY_LEN; i++)
    my_key[i]= (char)((int)(my_rnd_ssl(&rnd) * 100000) % 32);

  DBUG_VOID_RETURN;
}

/**
  Read the stored login key.

  @return -1              Error
           0              Success
*/

int read_login_key(void)
{
  DBUG_ENTER("read_login_key");

  verbose_msg("Reading the login key.\n");
  /* Move past the unused buffer. */
  if (my_seek(g_fd, 4, SEEK_SET, MYF(MY_WME)) != 4)
    DBUG_RETURN(-1);                            /* Error while seeking. */

  if (my_read(g_fd, (uchar *)my_key, LOGIN_KEY_LEN, MYF(MY_WME))
      != LOGIN_KEY_LEN)
  {
    verbose_msg("Failed to read login key.\n");
    DBUG_RETURN(-1);
  }

  verbose_msg("Login key read successfully.\n");
  DBUG_RETURN(0);
}


static void verbose_msg(const char *fmt, ...)
{
  DBUG_ENTER("verbose_msg");
  va_list args;

  if (!opt_verbose)
    DBUG_VOID_RETURN;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fflush(stderr);

  DBUG_VOID_RETURN;
}


static void usage(void)
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2012"));
  puts("MySQL Configuration Utility.");
  printf("Usage: %s command [options] \n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  puts("\nWhere command can be any one of the following :\n\
       set [options]             Sets user name/password/host name for a\n\
                                 given login path (section).\n\
       remove [options]          Remove a login path from the login file.\n\
       print [options]           Print all the options for a specified\n\
                                 login path.\n\
       reset                     Deletes the contents of the login file.\n\
       help                      Display this usage/help information.\n");
}


static void print_version(void) {
  printf ("%s Ver %s Distrib %s, for %s on %s\n", my_progname,
          MYSQL_CONFIG_EDITOR_VERSION, MYSQL_SERVER_VERSION,
          SYSTEM_TYPE, MACHINE_TYPE);
}

