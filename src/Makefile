CC=gcc
CFLAGS=-W -Wall -ansi -pedantic -std=c99 -D _BSD_SOURCE -g
LDFLAGS=-lpthread
EXEC=thermostatd
INSTDIR=/opt/thermostatd

all: $(EXEC)

thermostatd: thermostatd.o sonde.o
	$(CC) -o $@ $^ $(LDFLAGS)

sonde.o: sonde.c sonde.h
thermostatd.o: thermostatd.c thermostatd.h sonde.h
	$(COMPILE.c) $< -D_REENTRANT $(OUTPUT_OPTION)

.PHONY: clean mrproper install

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(EXEC)

install: all
	@if [ ! -d "$(INSTDIR)" ]; \
	    then \
	    echo "Creation du dossier $(INSTDIR)"; \
	    mkdir "$(INSTDIR)"; \
	fi
	@cp $(EXEC) $(INSTDIR)
	@echo "Installé dans $(INSTDIR)"
