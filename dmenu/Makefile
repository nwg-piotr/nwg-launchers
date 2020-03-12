CC=g++
CFLAGS=-c -Wall -O2 -std=c++17
SOURCES=dmenu.cc
OBJECTS=$(SOURCES:.cc=.o)
EXECUTABLE=nwgdmenu

PREFIX ?= /usr

all: $(SOURCES) $(EXECUTABLE) 

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ `pkg-config gtkmm-3.0 --libs --cflags`

.cc.o:
	$(CC) $(CFLAGS) $< -o $@ `pkg-config gtkmm-3.0 --libs --cflags`

clean:
	rm -rf *.o dmenu

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dmenu.sh ${DESTDIR}${PREFIX}/bin/nwgdmenu
	chmod 755 ${DESTDIR}${PREFIX}/bin/nwgdmenu
	
	mkdir -p ${DESTDIR}${PREFIX}/lib/nwgdmenu
	cp -f nwgdmenu ${DESTDIR}${PREFIX}/lib/nwgdmenu
	chmod 755 ${DESTDIR}${PREFIX}/lib/nwgdmenu/nwgdmenu
	
	mkdir -p ${DESTDIR}${PREFIX}/share/nwgdmenu
	cp -f style.css ${DESTDIR}${PREFIX}/share/nwgdmenu

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nwgdmenu
	rm -rf ${DESTDIR}${PREFIX}/lib/nwgdmenu
	rm -rf ${DESTDIR}${PREFIX}/share/nwgdmenu
	rm -f /tmp/nwgdmenu.lock
