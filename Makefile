all: 

#Write all executables you want to compile here

HEADERS = -I./include
SOURCES = -I./src -I./src/sensors -I./src/actuators
OUTPUT = -o ./bin  

in

service-functions.o: service-functions.h service-functions.c
	cc -c service-functions.h service-functions.c

install:
	mkdir -p bin
	mkdir -p tmp
	mkdir -p log


clean:
	rm -rf bin
	rm -rf tmp
	rm -rf log