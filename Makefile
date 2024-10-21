PREFIX=/accounts/1000/shared/clitools
DESTDIR=$$DESTDIR
CFLAGS=-fPIC

all:
	make -C src/c
	make -C src/c/demos

install:
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	mkdir -p $(DESTDIR)/$(PREFIX)/lib
	cp src/c/demos/demoPlayWav $(DESTDIR)/$(PREFIX)/bin
	cp src/c/demos/demoCaptureEco $(DESTDIR)/$(PREFIX)/bin
	cp src/c/libnixtla-audio.so $(DESTDIR)/$(PREFIX)/lib
