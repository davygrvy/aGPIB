#agpib.so : aGPIB.o
#	ld asdasda

aGPIBChan.o : aGPIBChan.c
	gcc -c -DUSE_TCL_STUBS -DBUILD_agpib -I /usr/include/tcl8.5/ aGPIBChan.c

aGPIB.o : aGPIB.cpp
	g++ -c -DUSE_TCL_STUBS -DBUILD_agpib -I /usr/include/tcl8.5/ aGPIB.cpp
