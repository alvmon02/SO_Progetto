all: bin_mkdir exec

#Need to have an executable for each sensor, and one executable for each actuator

CFLAGS = -Wall
HEADERS = ./include/

EXECUTABLE = ./bin/

SRC_DIR = ./src/
SRC_SEN_DIR = ./src/sensors/
SRC_ACT_DIR = ./src/actuators/

#Creazione della cartella di bin
bin_mkdir:
	mkdir -p $(EXECUTABLE)


#Compilazione del file eseguibile, unione dei file precompilati
exec: sensors actuators hmi ADAS-simulator

#Compilazione del file con il Main principale
ADAS-simulator: service-functions.o
	cc -o $(EXECUTABLE)ADAS-simulator \
	$(EXECUTABLE)service-functions.o $(SRC_DIR)central-ECU.c

#Compilazione dei eseguibili relativi ai sensori
sensors: park-assist windshield-camera bytes-sensors

park-assist:service-functions.o
	cc -o $(EXECUTABLE)park-assist \
	$(EXECUTABLE)service-functions.o $(SRC_SEN_DIR)park-assist.c

windshield-camera: service-functions.o
	cc -o $(EXECUTABLE)windshield-camera \
	$(EXECUTABLE)service-functions.o $(SRC_SEN_DIR)windshield-camera.c

bytes-sensors: service-functions.o
	cc -o $(EXECUTABLE)bytes-sensors \
	$(EXECUTABLE)service-functions.o $(SRC_SEN_DIR)bytes-sensors.c

#Compilazione dei eseguibili relativi agli attuatori
actuators: brake-by-wire steer-by-wire throttle-control

brake-by-wire: service-functions.o
	cc -o $(EXECUTABLE)brake-by-wire \
	$(EXECUTABLE)service-functions.o $(SRC_ACT_DIR)brake-by-wire.c

steer-by-wire: $(EXECUTABLE)service-functions.o
	cc -o $(EXECUTABLE)steer-by-wire \
	$(EXECUTABLE)service-functions.o $(SRC_ACT_DIR)steer-by-wire.c

throttle-control: $(EXECUTABLE)service-functions.o
	cc -o $(EXECUTABLE)throttle-control \
	$(EXECUTABLE)service-functions.o $(SRC_ACT_DIR)throttle-control.c




#Compilazione della HMI, due eseguibili
hmi: hmi-output hmi-input

hmi-output: service-functions.o
	cc -o $(EXECUTABLE)hmi-output \
	$(EXECUTABLE)service-functions.o $(SRC_DIR)hmi-output.c

hmi-input: service-functions.o
	cc -o $(EXECUTABLE)hmi-input \
	$(EXECUTABLE)service-functions.o $(SRC_DIR)hmi-input.c


#	Pre-Compilazione delle funzioni di servizio
service-functions.o: $(HEADERS)service-functions.h $(SRC_DIR)service-functions.c
	cc -c -o$(EXECUTABLE)service-functions.o $(SRC_DIR)service-functions.c


install:
	mkdir -p tmp
	mkdir -p log
	ln -sf $(EXECUTABLE)ADAS-simulator .

clean:
	rm -rf bin/*
	rm -rf tmp/*
	rm -rf log/*

uninstall:
	rm -rf bin
	rm -rf tmp
	rm -rf log
	rm ADAS-simulator
