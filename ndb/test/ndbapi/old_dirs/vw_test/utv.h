/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <semaphore.h>
#include <thread.h>
#include <limits.h>

#define TESTLEV

#define ASubscriberNumber_SIZE 16
#define BSubscriberNumber_SIZE 29
#define TRUE 1
#define FALSE 0
#define WRITE_LIMIT 100000
#define EVER ;;
#define CONNINFO "/"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define BIT_1   0x1
#define BIT_2   0x2
#define BIT_3   0x4
#define BIT_4   0x8
#define BIT_5   0x10
#define BIT_6   0x20
#define BIT_7   0x40
#define BIT_8   0x80

/*------------------------------------------------------*/
/* record defines structure over an alarm thresholds    */
/* CallAttemptState 	Beskriver status på samtal	*/
/*		0 - Subscriber is calling		*/
/*		1 - Called part answer call		*/
/*		2 - Release of call			*/
/*		3-255 reserved for furter use		*/
/* USED_FILEDS  Indicates active fields within call	*/
/*	   bit	1   - START_TIME			*/
/*              2   - TimeForStartOfCharge 		*/
/*              3   - TimeForStopOfCharge	 	*/
/*              4   - ReroutingIndicator		*/
/*              5   - RINParameter	 		*/
/*              6   - ACategory 			*/
/*              7   - EndOfSelectionInformation 			*/
/*              8   - UserToUserIndicatior		*/
/*              9   - UserToUserInformation		*/
/*              10  - CauseCode 			*/
/*              11  - ASubscriberNumber 		*/
/*              12  - BSubscriberNumber			*/
/*              13  - RedirectingNumber		        */
/*              14  - OriginalCalledNumber 		*/
/*              15  - LocationCode 			*/
/*              16  - OriginatingPointCode			*/
/*              17  - DestinationPointCode		*/
/*              18  - CircuitIdentificationCode		*/
/*              19  - NetworkIndicator			*/
/*------------------------------------------------------*/

struct cdr_record
{
	unsigned int	USED_FIELDS;
	unsigned long 	ClientId;
	unsigned int 	CallIdentificationNumber;		
	unsigned int 	START_TIME;
	unsigned int 	OurSTART_TIME;
	unsigned int 	TimeForStartOfCharge;
	unsigned int 	TimeForStopOfCharge;
	time_t 		OurTimeForStartOfCharge;
	time_t		OurTimeForStopOfCharge;
	unsigned short 	DestinationPointCode;
	unsigned short 	CircuitIdentificationCode;
	unsigned short 	OriginatingPointCode;
	unsigned short	ReroutingIndicator;
	unsigned short	RINParameter;
	char		NetworkIndicator;
	char 		CallAttemptState;		
	char		ACategory;
	char		EndOfSelectionInformation;
	char		UserToUserInformation;
	char		UserToUserIndicatior;
	char		CauseCode;
	char 		ASubscriberNumber[ASubscriberNumber_SIZE]; 
	char		ASubscriberNumberLength;
	char		TonASubscriberNumber;
	char 		BSubscriberNumber[BSubscriberNumber_SIZE];
	char		BSubscriberNumberLength;
	char		TonBSubscriberNumber;
	char 		RedirectingNumber[16];
	char		TonRedirectingNumber;
	char 		OriginalCalledNumber[16];
	char 		TonOriginalCalledNumber;
	char 		LocationCode[16];
	char 		TonLocationCode;
};

/*------------------------------------------------------*/
/* Define switches for each tag 			*/
/*------------------------------------------------------*/

#define B_START_TIME			0x1
#define B_TimeForStartOfCharge 		0x2
#define B_TimeForStopOfCharge	 	0x4
#define B_ReroutingIndicator		0x8
#define B_RINParameter	 		0x10
#define B_ACategory			0x20
#define B_EndOfSelectionInformation 	0x40	
#define B_UserToUserIndicatior		0x80
#define B_UserToUserInformation		0x100
#define B_CauseCode 			0x200	
#define B_ASubscriberNumber 		0x400
#define B_BSubscriberNumber		0x800
#define B_RedirectingNumber		0x1000
#define B_OriginalCalledNumber 		0x2000
#define B_LocationCode 			0x4000
#define B_OriginatingPointCode		0x8000
#define B_DestinationPointCode		0x10000
#define B_CircuitIdentificationCode	0x20000

#define B_NetworkIndicator		0x40000
#define B_TonASubscriberNumber 		0x80000
#define B_TonBSubscriberNumber		0x100000
#define B_TonRedirectingNumber		0x200000
#define B_TonOriginalCalledNumber 	0x4000000
#define B_TonLocationCode 		0x8000000

#define K_START_TIME			0xFF01
#define K_TimeForStartOfCharge 		0xFF02
#define K_TimeForStopOfCharge	 	0xFF03
#define K_ReroutingIndicator		0x13
#define K_RINParameter	 		0xFC
#define K_ACategory			0x09
#define K_EndOfSelectionInformation 	0x11	
#define K_UserToUserIndicatior		0x2A
#define K_UserToUserInformation		0x20
#define K_CauseCode 			0x12	
#define K_ASubscriberNumber 		0x0A
#define K_BSubscriberNumber		0x04
#define K_RedirectingNumber		0x0B
#define K_OriginalCalledNumber 		0x28
#define K_LocationCode 			0x3F
#define K_OriginatingPointCode		0xFD
#define K_DestinationPointCode		0xFE
#define K_CircuitIdentificationCode	0xFF

#define K_NetworkIndicator		0xF0
#define K_TonASubscriberNumber 		0xF1
#define K_TonBSubscriberNumber		0xF2
#define K_TonRedirectingNumber		0xF3
#define K_TonOriginalCalledNumber 	0xF4
#define K_TonLocationCode 		0xF5
