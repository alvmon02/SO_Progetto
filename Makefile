all: exec

CFLAGS = -Wall
HEADERS = ./include/

EXECUTABLE = ./bin/

SRC_DIR = ./src/
SRC_SEN_DIR = ./src/sensors/
SRC_ACT_DIR = ./src/actuators/

#Compilazione del file eseguibile, unione dei file precompilati
exec: sensors actuators central-ECU.o hmi service-functions.o
	cc -o $(EXECUTABLE)exec $(EXECUTABLE)central-ECU.o \
	$(EXECUTABLE)sensors $(EXECUTABLE)actuators \
	$(EXECUTABLE)hmi $(EXECUTABLE)service-functions.o


#Pre-Compilazione dei file relativi ai sensori

#	Creazione della libreria sensors per un uso successivo semplificato
sensors:bytes-sensors.o park-assist.o \
	windshield-camera.o surround-view-cameras.o \
	service-functions.o
	ar rs $(EXECUTABLE)sensors $(EXECUTABLE)bytes-sensors.o \
	$(EXECUTABLE)park-assist.o \
	$(EXECUTABLE)surround-view-cameras.o $(EXECUTABLE)windshield-camera.o

bytes-sensors.o: service-functions.o
	cc -c -o$(EXECUTABLE)bytes-sensors.o $(SRC_SEN_DIR)bytes-sensors.c

# forward-facing-radar.o: service-functions.o
# 	cc -c -o$(EXECUTABLE)forward-facing-radar.o $(SRC_SEN_DIR)forward-facing-radar.c

park-assist.o:service-functions.o
	cc -c -o$(EXECUTABLE)park-assist.o $(SRC_SEN_DIR)park-assist.c

surround-view-cameras.o:service-functions.o
	cc -c -o$(EXECUTABLE)surround-view-cameras.o \
	$(SRC_SEN_DIR)bytes-sensors.c

windshield-camera.o: service-functions.o
	cc -c -o$(EXECUTABLE)windshield-camera.o $(SRC_SEN_DIR)windshield-camera.c


#Pre-Compilazione dei file relativi agli attuatori

#	Creazione della libreria actuators per un uso successivo semplificato
actuators: brake-by-wire.o steer-by-wire.o throttle-control.o
	ar rs $(EXECUTABLE)actuators $(EXECUTABLE)brake-by-wire.o \
	$(EXECUTABLE)steer-by-wire.o $(EXECUTABLE)throttle-control.o


brake-by-wire.o: $(SRC_ACT_DIR)brake-by-wire.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)brake-by-wire.o $(SRC_ACT_DIR)brake-by-wire.c

steer-by-wire.o: $(SRC_ACT_DIR)steer-by-wire.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)steer-by-wire.o $(SRC_ACT_DIR)steer-by-wire.c

throttle-control.o: $(SRC_ACT_DIR)throttle-control.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)throttle-control.o $(SRC_ACT_DIR)throttle-control.c




#Pre-Compilazione dell'hmi

#	Creazione della libreria hmi per un uso successivo semplificato
hmi: hmi-output.o hmi-input.o
	ar rs $(EXECUTABLE)hmi $(EXECUTABLE)hmi-output.o \
	$(EXECUTABLE)hmi-input.o

hmi-output.o: $(SRC_DIR)hmi-output.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)hmi-output.o $(SRC_DIR)hmi-output.c

hmi-input.o: $(SRC_DIR)hmi-input.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)hmi-input.o  $(SRC_DIR)hmi-input.c

service-functions.o: $(SRC_DIR)service-functions.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)service-functions.o $(SRC_DIR)service-functions.c

#Pre-Compilazione della central-ECU
central-ECU.o: $(SRC_DIR)central-ECU.c $(HEADERS)service-functions.h
	cc -c -o$(EXECUTABLE)central-ECU.o $(SRC_DIR)central-ECU.c

install:
	mkdir -p bin
	mkdir -p tmp
	mkdir -p log

clean:
	rm -rf bin/*
	rm -rf tmp/*
	rm -rf log/*

uninstall:
	rm -rf bin
	rm -rf tmp
	rm -rf log
