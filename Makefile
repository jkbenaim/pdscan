target  ?= pdscan
objects := escape.o main.o mapfile.o pdscan.o

#libs:=sqlite3

#EXTRAS += -fsanitize=bounds -fsanitize=undefined -fsanitize=null -fcf-protection=full -fstack-protector-all -fstack-check -Wimplicit-fallthrough -Wall -Wextra

ifdef libs
LDLIBS  += $(shell pkg-config --libs   ${libs})
CFLAGS  += $(shell pkg-config --cflags ${libs})
endif

LDFLAGS += ${EXTRAS}
CFLAGS  += -std=gnu99 -ggdb ${EXTRAS}

.PHONY: all
all:	$(target)

.PHONY: clean
clean:
	rm -f $(target) $(objects)

.PHONY: install
install: ${target} ${target}.1
	install -m 755 ${target} /usr/local/bin
	install -m 755 -d /usr/local/share/man/man1
	install -m 644 ${target}.1 /usr/local/share/man/man1

.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/${target} /usr/local/share/man/man1/${target}.1

README: ${target}.1
	MANWIDTH=77 man --nh --nj ./${target}.1 | col -b > $@

$(target): $(objects)
