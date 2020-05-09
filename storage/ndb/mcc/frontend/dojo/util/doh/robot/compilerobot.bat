setlocal
set JDK14_HOME=C:\Program Files\IBM\Java70
del DOHRobot*.class
"%JDK14_HOME%\bin\javac" -source 1.5 -target 1.5 -classpath "%JDK14_HOME%\jre\lib\plugin.jar" DOHRobot.java
del DOHRobot.jar
"%JDK14_HOME%\bin\jar" cvf DOHRobot.jar DOHRobot*.class
"%JDK14_HOME%\bin\jar" umvf manifest.txt DOHRobot.jar
"%JDK14_HOME%\bin\jarsigner" -keystore ./dohrobot DOHRobot.jar dojo <key
del DOHRobot*.class
endlocal
