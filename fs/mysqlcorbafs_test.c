#include <stdio.h>
#include <stdlib.h>
#include <orb/orbit.h>

#include "CorbaFS.h"

CorbaFS_FileSystem fs;

int
main (int argc, char *argv[])
{
    CORBA_Environment ev;
    CORBA_ORB orb;
    CorbaFS_Inode inode;
    CorbaFS_Buffer *buffer;
    CorbaFS_DirEntSeq *dirents;
    CorbaFS_dirent *dirent;
    
    CORBA_unsigned_short mode;
    CORBA_unsigned_long uid;
    CORBA_unsigned_long gid;
    CORBA_unsigned_long size;
    CORBA_unsigned_long inodeNum;
    CORBA_unsigned_short numLinks;
    CORBA_long atime;
    CORBA_long mtime;
    CORBA_long ctime;
    
    int i;
    
    int niters = 10;

    CORBA_exception_init(&ev);
    orb = CORBA_ORB_init(&argc, argv, "orbit-local-orb", &ev);

    if(argc < 2)
      {
	printf("Need a binding ID thing as argv[1]\n");
	return 1;
      }


    fs = CORBA_ORB_string_to_object(orb, argv[1], &ev);
    if (!fs) {
	printf("Cannot bind to %s\n", argv[1]);
	return 1;
    }
    
    if (argc >= 3)
            inode = CorbaFS_FileSystem_getInode(fs, argv[2], &ev);
    else
            inode = CorbaFS_FileSystem_getInode(fs, "/proc/cpuinfo", &ev);
    
    if (!inode)
    {
            printf("Cannot get inode\n");
    }

    CorbaFS_Inode_getStatus(inode, 
                            &mode,
                            &uid,
                            &gid,
                            &size,
                            &inodeNum,
                            &numLinks,
                            &atime,
                            &mtime,
                            &ctime,
                            &ev);
    
    printf("inode = %x\n", inode);
    CorbaFS_Inode_readpage(inode, &buffer, 100000, 100, &ev);
    printf("readpage got %d bytes\n", buffer->_length);
    printf("readpage returned : %s\n", buffer->_buffer);

    if (argc >= 3)
       dirents = CorbaFS_FileSystem_readdir(fs, argv[2], &ev);
    else
       dirents = CorbaFS_FileSystem_readdir(fs, "/", &ev);

    dirent = dirents->_buffer;
    for (i = 0; i < dirents->_length; i++)
    {
            printf("%d = %s\n", dirent->inode, dirent->name);
            dirent++;
    }
    
    CORBA_Object_release(fs, &ev);
    CORBA_Object_release((CORBA_Object)orb, &ev);

    return 0;
}
