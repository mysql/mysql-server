CL -I\my\include -AL -Gsm2 -FPi -DDBUG_OFF *.c
rm \my\lib\dbug.lib
lib.exe \my\lib\dbug dbug.obj sanity.obj;
link /NOD /STACK:8000 main factoria,factoria,,DBUG+STRINGS+LLIBCEP+DOSCALLS;
