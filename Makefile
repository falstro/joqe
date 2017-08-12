BISON=/usr/bin/bison
CC=gcc
CFLAGS=-Wall -g -MMD -O3
LDFLAGS=-lm
PREFIX=/usr/local
DESTDIR=

INSTALL=/usr/bin/install
RM=/bin/rm

GIT=/usr/bin/git
GZIP=/bin/gzip
TAR=/bin/tar

src/tests=test-lex test-ast test-hopscotch
tests=$(src/tests:%=src/%)

src/joqe=joqe joqe.tab json ast lex lex-source utf build err util hopscotch
joqe=$(src/joqe:%=src/%)

src/utf-cat=utf-cat lex-source utf
utf-cat=$(src/utf-cat:%=src/%)

targets=joqe utf-cat

deps=$(joqe:=.d) $(utf-cat:=.d) $(tests:=.d)

all: $(targets) check-depend

check: check-all

clean:
	$(RM) -f $(targets) $(joqe:=.o) $(utf-cat:=.o) \
    $(tests) $(tests:=.o) $(tests:=.tx) $(deps)

dist: src/joqe.tab.c src/joqe.tab.h
	@DEST=joqe-$$($(GIT) describe --abbrev=4 --always --tags); \
  $(GIT) archive --prefix=$$DEST/ -o $$DEST.tar HEAD && \
  $(TAR) rf $$DEST.tar --xform="s?^?$$DEST/?" $^ && \
  $(GZIP) -f $$DEST.tar

distclean: clean
	$(RM) -f src/joqe.tab.h src/joqe.tab.c

install:
	$(INSTALL) -D joqe $(DESTDIR)$(PREFIX)/bin/joqe
install-strip:
	$(INSTALL) -Ds joqe $(DESTDIR)$(PREFIX)/bin/joqe

uninstall:
	$(RM) -f $(DESTDIR)$(PREFIX)/bin/joqe

joqe: $(joqe:=.o)
	$(CC) $(LDFLAGS) -o $@ $^

utf-cat: $(utf-cat:=.o)
	$(CC) $(LDFLAGS) -o $@ $^

src/joqe.tab.h: src/joqe.tab.c
src/joqe.tab.c: joqe.y
	$(BISON) -o $@ $<

src/joqe.c: src/joqe.tab.h
src/json.c: src/joqe.tab.h
src/ast.c: src/joqe.tab.h
src/lex.c: src/joqe.tab.h

# run tests when their dependencies are affected
check-depend: $(tests:=.tx)
	@true

%.tx: %
	@ok=true; printf '  %-20s' $<; m=`./$< 2>&1 || false` \
    && { touch $@; echo ok; }\
    || { ok=false; echo fail; }; \
    [ -n "$$m" ] && echo "$$m"; \
    $$ok

check-all: $(tests)
	@ok=true; for t in $(tests); do \
        printf '  %-20s' $$t; m=`./$$t 2>&1 || false` \
          && echo ok \
          || { ok=false; echo fail; }; \
          [ -n "$$m" ] && echo "$$m"; \
      done; $$ok

src/test-lex=test-lex.o lex.o lex-source.o build.o \
  hopscotch.o utf.o
src/test-lex: $(src/test-lex:%=src/%)

src/test-ast=test-ast.o json.o joqe.tab.o ast.o lex.o lex-source.o build.o \
  err.o util.o hopscotch.o utf.o
src/test-ast: $(src/test-ast:%=src/%)

src/test-hopscotch=hopscotch.o
src/test-hopscotch: $(src/test-hopscotch:%=src/%)

-include $(deps)
