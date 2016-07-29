# simple Makefile for rogg and related utilities

PACKAGE = rogg
VERSION = 0.5git

prefix = /usr/local

OPTS = -g -O2 -Wall

rogg_UTILS = rogg_pagedump rogg_eosfix rogg_crcfix \
	rogg_stats rogg_serial rogg_theora rogg_kate \
	rogg_opus rogg_granule

all : librogg.a $(rogg_UTILS)

EXTRA_DIST = Makefile README

librogg.a : rogg.o
	$(AR) cr $@ $^
	ranlib $@

rogg_eosfix : rogg_eosfix.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_crcfix : rogg_crcfix.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_pagedump : rogg_pagedump.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_stats : rogg_stats.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_serial : rogg_serial.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_theora : rogg_theora.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_kate : rogg_kate.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^

rogg_opus : rogg_opus.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^ -lm

rogg_granule : rogg_granule.o librogg.a
	$(CC) $(CFLAGS) -o $@ $^ -lm

check : all

clean :
	-rm -f $(rogg_UTILS)
	-rm -f librogg.a
	-rm -f *.o

.PHONY : all check clean install uninstall dist

.c.o :
	$(CC) $(OPTS) $(CFLAGS) -I. -c $<

install : all
	cp librogg.a $(prefix)/lib/
	cp rogg.h $(prefix)/include/
	cp $(rogg_UTILS) $(prefix)/bin/

uninstall : 
	-rm -f $(prefix)/lib/librogg.a
	-rm -f $(prefix)/include/rogg.h
	-for util in $(rogg_UTILS); do rm -f $(prefix)/bin/$$util; done

# Override static version with git revision info if possible.
GIT_VERSION := $(shell git describe --always --tags  --dirty)
VERSION := $(or $(GIT_VERSION),$(VERSION),unknown)

distdir = $(PACKAGE)-$(VERSION)
dist : *.c *.h Makefile
	if test -d $(distdir); then rm -rf $(distdir); fi
	mkdir $(distdir)
	cp *.c *.h $(distdir)/
	cp $(EXTRA_DIST) $(distdir)/
	tar czf $(distdir).tar.gz $(distdir)
	rm -rf $(distdir)

distcheck : dist
	if test -d $(distdir); then rm -rf $(distdir); fi
	tar xf $(distdir).tar.gz
	cd $(distdir) && make check
	cd $(distdir) && make dist
	rm -rf $(distdir)
	@echo $(distdir).tar.gz is ready to distribute
