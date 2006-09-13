/*C4*/
/****************************************************************/
/*	Author:	Jethro Wright, III	TS :  3/ 4/1998  9:15	*/
/*	Date:	02/18/1998					*/
/*	mytest.c :  do some testing of the libmySQL.DLL....	*/
/*								*/
/*	History:						*/
/*		02/18/1998  jw3  also sprach zarathustra....	*/
/****************************************************************/


#include        <windows.h>
#include	<stdio.h>
#include	<string.h>

#include	<mysql.h>

#define		DEFALT_SQL_STMT	"SELECT * FROM db"
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif


/********************************************************
**
**		main  :-
**
********************************************************/

int
main( int argc, char * argv[] )
{

  char		szSQL[ 200 ], aszFlds[ 25 ][ 25 ], szDB[ 50 ] ;
  const  char   *pszT;
  int			i, j, k, l, x ;
  MYSQL		* myData ;
  MYSQL_RES	* res ;
  MYSQL_FIELD	* fd ;
  MYSQL_ROW	row ;

  //....just curious....
  printf( "sizeof( MYSQL ) == %d\n", (int) sizeof( MYSQL ) ) ;
  if ( argc == 2 )
    {
      strcpy( szDB, argv[ 1 ] ) ;
      strcpy( szSQL, DEFALT_SQL_STMT ) ;
      if (!strcmp(szDB,"--debug"))
      {
	strcpy( szDB, "mysql" ) ;
	printf("Some mysql struct information (size and offset):\n");
	printf("net:\t%3d %3d\n",(int) sizeof(myData->net),
	       (int) offsetof(MYSQL,net));
	printf("host:\t%3d %3d\n",(int) sizeof(myData->host),
	       (int) offsetof(MYSQL,host));
	printf("port:\t%3d %3d\n", (int) sizeof(myData->port),
	       (int) offsetof(MYSQL,port));
	printf("protocol_version:\t%3d %3d\n",
	       (int) sizeof(myData->protocol_version),
	       (int) offsetof(MYSQL,protocol_version));
	printf("thread_id:\t%3d %3d\n",(int) sizeof(myData->thread_id),
	       (int) offsetof(MYSQL,thread_id));
	printf("affected_rows:\t%3d %3d\n",(int) sizeof(myData->affected_rows),
	       (int) offsetof(MYSQL,affected_rows));
	printf("packet_length:\t%3d %3d\n",(int) sizeof(myData->packet_length),
	       (int) offsetof(MYSQL,packet_length));
	printf("status:\t%3d %3d\n",(int) sizeof(myData->status),
	       (int) offsetof(MYSQL,status));
	printf("fields:\t%3d %3d\n",(int) sizeof(myData->fields),
	       (int) offsetof(MYSQL,fields));
	printf("field_alloc:\t%3d %3d\n",(int) sizeof(myData->field_alloc),
	       (int) offsetof(MYSQL,field_alloc));
	printf("free_me:\t%3d %3d\n",(int) sizeof(myData->free_me),
	       (int) offsetof(MYSQL,free_me));
	printf("options:\t%3d %3d\n",(int) sizeof(myData->options),
	       (int) offsetof(MYSQL,options));
	puts("");
      }
    }		
  else if ( argc > 2 ) {
    strcpy( szDB, argv[ 1 ] ) ;
    strcpy( szSQL, argv[ 2 ] ) ;
  }
  else {
    strcpy( szDB, "mysql" ) ;
    strcpy( szSQL, DEFALT_SQL_STMT ) ;
  }
  //....
		  
  if ( (myData = mysql_init((MYSQL*) 0)) && 
       mysql_real_connect( myData, NULL, NULL, NULL, NULL, MYSQL_PORT,
			   NULL, 0 ) )
    {
      myData->reconnect= 1;
      if ( mysql_select_db( myData, szDB ) < 0 ) {
	printf( "Can't select the %s database !\n", szDB ) ;
	mysql_close( myData ) ;
	return 2 ;
      }
    }
  else {
    printf( "Can't connect to the mysql server on port %d !\n",
	    MYSQL_PORT ) ;
    mysql_close( myData ) ;
    return 1 ;
  }
  //....
  if ( ! mysql_query( myData, szSQL ) ) {
    res = mysql_store_result( myData ) ;
    i = (int) mysql_num_rows( res ) ; l = 1 ;
    printf( "Query:  %s\nNumber of records found:  %ld\n", szSQL, i ) ;
    //....we can get the field-specific characteristics here....
    for ( x = 0 ; fd = mysql_fetch_field( res ) ; x++ )
      strcpy( aszFlds[ x ], fd->name ) ;
    //....
    while ( row = mysql_fetch_row( res ) ) {
      j = mysql_num_fields( res ) ;
      printf( "Record #%ld:-\n", l++ ) ;
      for ( k = 0 ; k < j ; k++ )
	printf( "  Fld #%d (%s): %s\n", k + 1, aszFlds[ k ],
		(((row[k]==NULL)||(!strlen(row[k])))?"NULL":row[k])) ;
      puts( "==============================\n" ) ;
    }
    mysql_free_result( res ) ;
  }
  else printf( "Couldn't execute %s on the server !\n", szSQL ) ;
  //....
  puts( "====  Diagnostic info  ====" ) ;
  pszT = mysql_get_client_info() ;
  printf( "Client info: %s\n", pszT ) ;
  //....
  pszT = mysql_get_host_info( myData ) ;
  printf( "Host info: %s\n", pszT ) ;
  //....
  pszT = mysql_get_server_info( myData ) ;
  printf( "Server info: %s\n", pszT ) ;
  //....
  res = mysql_list_processes( myData ) ; l = 1 ;
  if (res)
    {
      for ( x = 0 ; fd = mysql_fetch_field( res ) ; x++ )
	strcpy( aszFlds[ x ], fd->name ) ;
      while ( row = mysql_fetch_row( res ) ) {
	j = mysql_num_fields( res ) ;
	printf( "Process #%ld:-\n", l++ ) ;
	for ( k = 0 ; k < j ; k++ )
	  printf( "  Fld #%d (%s): %s\n", k + 1, aszFlds[ k ],
		  (((row[k]==NULL)||(!strlen(row[k])))?"NULL":row[k])) ;
	puts( "==============================\n" ) ;
      }
    }
  else
    {
      printf("Got error %s when retreiving processlist\n",mysql_error(myData));
    }
  //....
  res = mysql_list_tables( myData, "%" ) ; l = 1 ;
  for ( x = 0 ; fd = mysql_fetch_field( res ) ; x++ )
    strcpy( aszFlds[ x ], fd->name ) ;
  while ( row = mysql_fetch_row( res ) ) {
    j = mysql_num_fields( res ) ;
    printf( "Table #%ld:-\n", l++ ) ;
    for ( k = 0 ; k < j ; k++ )
      printf( "  Fld #%d (%s): %s\n", k + 1, aszFlds[ k ],
	      (((row[k]==NULL)||(!strlen(row[k])))?"NULL":row[k])) ;
    puts( "==============================\n" ) ;
  }
  //....
  pszT = mysql_stat( myData ) ;
  puts( pszT ) ;
  //....
  mysql_close( myData ) ;
  return 0 ;

}
