# Makefile
# -*- Mode: sh

VERSION=$(shell cut -f2 -d\" VERSION)
PROJ=$(shell basename `pwd`)
LIB=lib$(PROJ).a
BIN=$(PROJ)
MAIN=$(PROJ).c
CFLAGS=-g -Wall
DOCS=README COPYING Changelog
PREFIX=/usr/local

OBJS=mp_core.o

##################################################################

all: $(BIN)

config.h: VERSION config.sh
	sh config.sh

config:
	sh config.sh

version:
	@echo $(VERSION)

Changelog:
	rcs2log > Changelog

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dep: config
	gcc -MM *.c > makefile.depend

# include dependencies
-include makefile.depend

# library
$(LIB): $(OBJS)
	$(AR) rv $(LIB) $(OBJS)

# main binary
$(BIN): $(MAIN) $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) `cat config.libs` -o $@

clean:
	rm -f $(BIN) $(LIB) $(OBJS) config.h config.libs Changelog tags

dist: clean doc Changelog
	cd .. ; ln -s $(PROJ) $(PROJ)-$(VERSION); \
	tar czvf $(PROJ)-$(VERSION)/$(PROJ)-$(VERSION).tar.gz --exclude=CVS $(PROJ)-$(VERSION)/* ; \
	rm $(PROJ)-$(VERSION)

install:
	install $(BIN) -o root -g root -m 755 $(PREFIX)/bin
	install -o root -d $(PREFIX)/share/doc/$(PROJ)
	install -o root -m 0644 $(DOCS) $(PREFIX)/share/doc/$(PROJ)
