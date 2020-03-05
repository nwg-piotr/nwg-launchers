CC=g++
CFLAGS=-c -Wall -O2 -std=c++17
SOURCES=grid.cc
OBJECTS=$(SOURCES:.cc=.o)
EXECUTABLE=nwggrid

PREFIX ?= /usr

all: $(SOURCES) $(EXECUTABLE) 

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ `pkg-config gtkmm-3.0 --libs --cflags`

.cc.o:
	$(CC) $(CFLAGS) $< -o $@ `pkg-config gtkmm-3.0 --libs --cflags`

clean:
	rm -rf *.o nwggrid

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f grid.sh ${DESTDIR}${PREFIX}/bin/nwggrid
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwggrid
	
	mkdir -p ${DESTDIR}${PREFIX}/lib/nwggrid
	cp -f nwggrid ${DESTDIR}${PREFIX}/lib/nwggrid
	chmod 755 ${DESTDIR}${PREFIX}/lib/nwggrid/nwggrid
	
	mkdir -p ${DESTDIR}${PREFIX}/share/nwggrid
	cp -f style.css ${DESTDIR}${PREFIX}/share/nwggrid
	cp -f icon-missing.svg ${DESTDIR}${PREFIX}/share/nwggrid

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nwggrid
	rm -rf ${DESTDIR}${PREFIX}/lib/nwggrid
	rm -rf ${DESTDIR}${PREFIX}/share/nwggrid
