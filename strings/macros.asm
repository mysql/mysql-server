; Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
; 
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Library General Public
; License as published by the Free Software Foundation; either
; version 2 of the License, or (at your option) any later version.
; 
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; Library General Public License for more details.
; 
; You should have received a copy of the GNU Library General Public
; License along with this library; if not, write to the Free
; Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
; MA 02111-1307, USA

; Some useful macros

	.386P
	.387

_FLAT	equ	0		;FLAT memory model
_STDCALL equ	0		;default to _stdcall
I386	equ	1

begcode macro	module
    if _FLAT
_TEXT	segment dword use32 public 'CODE'
	  assume	CS:FLAT,DS:FLAT,SS:FLAT
    else
_TEXT	segment dword public 'CODE'
	  assume	CS:_TEXT
    endif
	endm

endcode macro	module
_TEXT	ENDS
	endm

begdata macro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Set up segments for data
; Regular initialized data goes in _DATA

_DATA	segment dword public 'DATA'
_DATA	ends

;Function pointers to constructors
XIB	segment dword public 'DATA'
XIB	ends
XI	segment dword public 'DATA'
XI	ends
XIE	segment dword public 'DATA'
XIE	ends

;Function pointers to destructors
XCB	segment dword public 'DATA'
XCB	ends
XC	segment dword public 'DATA'
XC	ends
XCE	segment dword public 'DATA'
XCE	ends

;Constant data, such as switch tables, go here.

CONST	segment dword public 'CONST'
CONST	ends

;Segment for uninitialized data. This is set to 0 by the startup code/OS,
;so it does not consume room in the executable file.

_BSS	segment dword public 'BSS'
_BSS	ends

HUGE_BSS	segment dword public 'HUGE_BSS'
HUGE_BSS	ends

EEND	segment dword public 'ENDBSS'
EEND	ends

STACK	segment para stack 'STACK'
STACK	ends
DGROUP	group	_DATA,XIB,XI,XIE,XCB,XC,XCE,CONST,_BSS,EEND,STACK

_DATA	segment
	if _FLAT
	  assume DS:FLAT
	else
	  assume DS:DGROUP
	endif
	endm

enddata macro
_DATA	ends
	endm

P	equ	8	; Offset of start of parameters on the stack frame
			; From EBP assuming EBP was pushed.
PS	equ	4	; Offset of start of parameters on the stack frame
			; From ESP assuming EBP was NOT pushed.
ESeqDS	equ	0
FSeqDS	equ	0
GSeqDS	equ	0
SSeqDS	equ	1
SIZEPTR equ	4	; Size of a pointer
LPTR	equ	0
SPTR	equ	1
LCODE	equ	0

func	macro	name
_&name	proc	near
    ifndef name
name	equ	_&name
    endif
	endm

callm	macro	name
	call	_&name
	endm

;Macros to replace public, extrn, and endp for C-callable assembly routines,
; and to define labels: c_label defines labels,
; c_public replaces public, c_extrn replaces extrn, and c_endp replaces endp

c_name	macro	name
	name equ _&name
	endm

c_label macro	name
_&name:
	endm

c_endp	macro	name
_&name	ENDP
	endm

clr	macro	list		;clear a register
	irp	reg,<list>
	 xor	reg,reg
	endm
	endm

jmps	macro	lbl
	jmp	short	lbl
	endm
