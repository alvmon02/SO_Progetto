all: bigMain

bigMain: smallerMain.o
			cc bin/smallerMain.o -o bin/bigMain

smallerMain.o: src/input_output.c 
				cc -c src/input_output.c -o bin/smallerMain.o