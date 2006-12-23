/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef OPENFILES_H
#define OPENFILES_H

#include <Vector.hpp>

class OpenFiles
{
public:
   OpenFiles(){ }
   
  /* Get a pointer to the file with id */
  AsyncFile* find(Uint16 id) const;
  /* Insert file with id */
  bool insert(AsyncFile* file, Uint16 id);
  /* Erase file with id */
  bool erase(Uint16 id);
  /* Get number of open files */
  unsigned size();

  Uint16 getId(unsigned i);
  AsyncFile* getFile(unsigned i);
  
   
private:

  class OpenFileItem {
  public:
    OpenFileItem():  m_file(NULL), m_id(0){};

    AsyncFile* m_file;      
    Uint16 m_id;
  };

  Vector<OpenFileItem> m_files;
};


//*****************************************************************************
inline AsyncFile* OpenFiles::find(Uint16 id) const {
  for (unsigned i = 0; i < m_files.size(); i++){
    if (m_files[i].m_id == id){
      return m_files[i].m_file;
    }
  }
  return NULL;
}

//*****************************************************************************
inline bool OpenFiles::erase(Uint16 id){
  for (unsigned i = 0; i < m_files.size(); i++){
    if (m_files[i].m_id == id){
      m_files.erase(i);
      return true;
    }
  }
  // Item was not found in list
  return false;
}


//*****************************************************************************
inline bool OpenFiles::insert(AsyncFile* file, Uint16 id){
  // Check if file has already been opened
  for (unsigned i = 0; i < m_files.size(); i++){
    if(m_files[i].m_file == NULL)
      continue;

    if(strcmp(m_files[i].m_file->theFileName.c_str(), 
	      file->theFileName.c_str()) == 0)
    {
      BaseString names;
      names.assfmt("open: >%s< existing: >%s<",
		   file->theFileName.c_str(),
		   m_files[i].m_file->theFileName.c_str());
      ERROR_SET(fatal, NDBD_EXIT_AFS_ALREADY_OPEN, names.c_str(),
		"OpenFiles::insert()");    
    }
  }
  
  // Insert the file into vector
  OpenFileItem openFile;
  openFile.m_id = id;
  openFile.m_file = file;
  m_files.push_back(openFile);
  
  return true;
}

//*****************************************************************************
inline Uint16 OpenFiles::getId(unsigned i){
  return m_files[i].m_id; 
}

//*****************************************************************************
inline AsyncFile* OpenFiles::getFile(unsigned i){
  return m_files[i].m_file; 
}

//*****************************************************************************
inline unsigned OpenFiles::size(){
  return m_files.size(); 
}

#endif
