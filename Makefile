
JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk1.8.0_192.jdk/Contents/Home

programmeC.o: programmeC.c def.h
	gcc -I"${JAVA_HOME}/include" -I"${JAVA_HOME}/include/darwin" -c programmeC.c

client.o: client.c
	gcc -c client.c

libPerso.dylib: programmeC.o client.o
		gcc -dynamiclib -o libPerso.dylib programmeC.o client.o
#	gcc -dynamiclib -fPIC -Wl,-undefined,dynamic_lookup -o libPerso.dylib programmeC.o client.o
