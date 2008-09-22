
TARGET = oops-code

all: ${TARGET}

${TARGET}: oops-code.c

clean:
	rm -f ${TARGET}

.PHONY: test
test: ${TARGET}
	./${TARGET} < oops
