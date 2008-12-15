/*
  To change or add messages mysqld writes to the Windows error log, run
   mc.exe message.mc
  and checkin generated messages.h, messages.rc and msg000001.bin under the 
  source control.
  mc.exe can be installed with Windows SDK, some Visual Studio distributions 
  do not include it.
*/
//
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: MSG_DEFAULT
//
// MessageText:
//
//  %1For more information, see Help and Support Center at http://www.mysql.com.
//  
//  
//
#define MSG_DEFAULT                      0xC0000064L

