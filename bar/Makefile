CC=g++
CFLAGS=-c -Wall -O2 -std=c++17
SOURCES=bar.cc
OBJECTS=$(SOURCES:.cc=.o)
EXECUTABLE=nwgbar

PREFIX ?= /usr

all: $(SOURCES) $(EXECUTABLE) 

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ `pkg-config gtkmm-3.0 --libs --cflags` -lstdc++fs

.cc.o:
	$(CC) $(CFLAGS) $< -o $@ `pkg-config gtkmm-3.0 --libs --cflags`

clean:
	rm -rf *.o nwgbar

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f nwgbar ${DESTDIR}${PREFIX}/bin/nwgbar
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwgbar
	
	mkdir -p ${DESTDIR}${PREFIX}/share/nwgbar
	cp -f style.css ${DESTDIR}${PREFIX}/share/nwgbar
	cp -f bar.json ${DESTDIR}${PREFIX}/share/nwgbar
	cp -f icon-missing.svg ${DESTDIR}${PREFIX}/share/nwgbar

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nwgbar
	rm -rf ${DESTDIR}${PREFIX}/share/nwgbar
