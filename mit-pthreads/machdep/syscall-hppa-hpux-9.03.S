	.CODE

machdep_error
	sub %r0,%r28,%r28			 
	bv,n %r0(%r2)				 

machdep_sys_fork														
																		
	.PROC															
	.CALLINFO 	NO_CALLS,FRAME=0	 									
																		
	ldil -0x80000,%r1			 	
	ble 4(%sr7,%r1)				  					
	ldi 2 ,%r22			  		
	or,= %r0,%r22,%r0			 						
	b,n machdep_error			 		
	or,= %r29,%r0,%r0				 				
	copy %r0,%r28				 	
	bv,n %r0(%r2)				 							
																		
	.PROCEND															
	.EXPORT		machdep_sys_fork,ENTRY

