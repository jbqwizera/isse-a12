# -fsanitize=... documentation:
#   https://gcc.gnu.org/onlinedocs/gcc-11.4.0/gcc/Instrumentation-Options.html
# 	https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer

CFLAGS=-Wall -Werror -g -fsanitize=address
TARGETS=plaidsh psh_test
OBJS=clist.o tokenize.o pipeline.o parse.o 
HDRS=clist.h token.h tokenize.h pipeline.h parse.h
LIBS=-lasan -lreadline

all: $(TARGETS)

plaidsh: $(OBJS) plaidsh.o
	gcc $(LDFLAGS) $^ $(LIBS) -o $@

psh_test:  $(OBJS) psh_test.o
	gcc $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.c $(HDRS)
	gcc -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(TARGETS)
