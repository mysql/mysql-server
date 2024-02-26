/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef FILE_H
#define FILE_H

#include <ndb_global.h>
#include <time.h>

/**
 * This class provides a file abstraction . It has operations
 * to create, read, write and delete a file.
 *
 * @version #@ $Id: File.hpp,v 1.5 2002/04/26 13:15:38 ejonore Exp $
 */
class File_class
{
public:
  /**
   * Returns time for last contents modification of a file.
   *
   * @param aFileName a filename to check.
   * @return the time for last contents modificaton of the file.
   */
  static time_t mtime(const char* aFileName);      

  /**
   * Returns true if the file exist.
   *
   * @param aFileName a filename to check.
   * @return true if the file exists.
   */
  static bool exists(const char* aFileName);      

  /**
   * Returns the size of a file.
   *
   * @param f a pointer to a FILE descriptor.
   * @return the size of the file.
   */
  static ndb_off_t size(FILE* f);

  /**
   * Renames a file.
   *
   * @param currFileName the current name of the file.
   * @param newFileName the new name of the file.
   * @return true if successful.
   */
  static bool rename(const char* currFileName, const char* newFileName);

  /**
   * Removes/deletes a file.
   *
   * @param aFileName the file to remove.
   * @return true if successful.
   */
  static bool remove(const char* aFileName);      
  
  /**
   * Default constructor.
   */
  File_class();

  /**
   * Creates a new File  with the specified filename and file mode. 
   * The real file itself will not be created unless open() is called!
   *
   * To see the available file modes use 'man 3 fopen'.
   *
   * @param aFileName a filename.
   * @param mode the mode which the file should be opened/created with, default "r".
   */
  File_class(const char* aFileName, const char* mode = "r");

  /**
   * Destructor.
   */
  ~File_class();

  /**
   * Opens/creates the file. If open() fails then 'errno' and perror() 
   * should be used to determine the exact failure cause.
   *
   * @return true if successful. Check errno if unsuccessful.
   */
  bool open();

  /**
   * Opens/creates the file with the specified name and mode.
   * If open() fails then 'errno' and perror() should be used to 
   * determine the exact failure cause.
   *
   * @param aFileName the file to open.
   * @param mode the file mode to use.
   * @return true if successful. Check errno if unsuccessful.   
   */
  bool open(const char* aFileName, const char* mode);

  /**
   * Check if the file is open
   *
   * @return true if file is open, false if closed
   */
  bool is_open();

  /**
   * Removes the file.
   *
   * @return true if successful.
   */
  bool remove();

  /**
   * Closes the file, i.e., the FILE descriptor is closed.
   */
  bool close();

  /**
   * Read from the file. See fread() for more info.
   *
   * @param buf the buffer to read into.
   * @param itemSize the size of each item. 
   * @param nitems read max n number of items.
   * @return 0 if successful.
   */
  int read(void* buf, size_t itemSize, size_t nitems) const;

  /**
   * Read char from the file. See fread() for more info.
   *
   * @param buf the buffer to read into.
   * @param start the start index of the buf.
   * @param length the length of the buffer.
   * @return 0 if successful.
   */
  int readChar(char* buf, long start, long length) const;  

  /**
   * Read chars from the file. See fread() for more info.
   *
   * @param buf the buffer to read into.
   * @return 0 if successful.
   */
  int readChar(char* buf);  

  /**
   * Write to file. See fwrite() for more info.
   *
   * @param buf the buffer to read from.
   * @param itemSize the size of each item. 
   * @param nitems write max n number of items.
   * @return 0 if successful.
   */
  int write(const void* buf, size_t itemSize, size_t nitems);     

  /**
   * Write chars to file. See fwrite() for more info.
   *
   * @param buf the buffer to read from.
   * @param start the start index of the buf.
   * @param length the length of the buffer.
   * @return 0 if successful.
   */
  int writeChar(const char* buf, long start, long length);   

  /**
   * Write chars to file. See fwrite() for more info.
   *
   * @param buf the buffer to read from.
   * @return 0 if successful.
   */
  int writeChar(const char* buf); 

  /**
   * Returns the file size.
   *
   * @return the file size.
   */
  ndb_off_t size() const;

  /**
   * Returns the filename.
   *
   * @return the filename.
   */
  const char* getName() const;

  /**
   * Flush the buffer.
   *
   * @return 0 if successful.
   */
  int flush() const;

private:
  FILE* m_file;
  char m_fileName[PATH_MAX];
  const char* m_fileMode;
  /* Prohibit */
  File_class (const File_class& aCopy);
  File_class operator = (const File_class&); 
  bool operator == (const File_class&);
};

/**
 * simple File guard to make sure no file is left open
 */
class FileGuard
{
  FILE * f;
public:

  FileGuard(FILE * f1)
    : f(f1)
  {
  }

  void close()
  {
    if (f)
    {
      fclose(f);
      f = nullptr;
    }
  }

  ~FileGuard()
  {
    close();
  }
};
#endif

