
TARGET = oops-code

all: ${TARGET}

${TARGET}: oops-code.c

clean:
	rm -f ${TARGET}

.PHONY: test
test: ${TARGET}
	./${TARGET} < oops | tee test.c
	gcc -m32 -ggdb -o test.o test.c
	ld -melf_i386 \
		--section-start .before=0xc01a516b \
		--section-start .oops=0xc01a5196 \
		-o test test.o
