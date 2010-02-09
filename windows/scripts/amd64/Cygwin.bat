@echo off

call "C:\Program Files (x86)\Intel\Compiler\C++\10.1.032\EM64T\Bin\ICLVars.bat"
C:
chdir C:\cygwin\bin

bash --login -i %*

