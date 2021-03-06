
TARGET = oops-code

CFLAGS = -ggdb -Wall

all: ${TARGET}

${TARGET}: oops-code.c

clean:
	rm -f ${TARGET} *~ *.o

.PHONY: test
test: ${TARGET}
	./${TARGET} < oops
