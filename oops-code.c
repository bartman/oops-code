#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

static const char *find_oops_code(FILE *in)
{
	char *buff = NULL;
	size_t buff_size = 0;
	ssize_t buff_len;

	while ((buff_len = getline(&buff, &buff_size, in)) != -1) {
		char *p;

		if (strncmp(buff, "Code: ", 6))
			continue;

		p = buff + 6;

		return p;
	}

	return NULL;
}

struct code {
	unsigned size;
	unsigned count;
	unsigned start_ofs;
	uint8_t  bytes[0];
};

static struct code *parse_oops_code(const char *text)
{
	unsigned size = strlen(text) / 3 + 2;
	struct code *code;
	unsigned idx = 0;
	const char *p;
	char *n = NULL;
	unsigned long int num;

	code = calloc(1, sizeof(*code) + size);
	assert(code);

	code->size = size;

	for (p=text; (num=strtoul(p, &n, 16)) < 0x100 && n != p; p=n) {

		if (!code->start_ofs && *n=='>' && (n-text)>3 && *(n-3)=='<') {
			code->start_ofs = idx;
			n++;
		}

		code->bytes[idx++] = num;

		if (idx>=code->size) {
			size *= 2;
			code = realloc(code, sizeof(*code) + size);
			assert(code);

			code->size = size;
		}

		while (*n && (*n==' ' || *n=='<')) n++;
	}

	code->count = idx;

	return code;
}

#define ASM_BLOCK_START "asm volatile ("
#define ASM_BLOCK_END   ");"

#define PROTOTYPE(x) "void "x"(void) __attribute__((section(\"."x"\")));"

static void print_code(const struct code *code, FILE *out)
{
	int i = 0;

	printf("int main(void) { return 0; }\n\n");

	if (code->start_ofs) {
		printf(PROTOTYPE("before") "\n");
		printf("void before(void)\n{\n");
		printf("\t" ASM_BLOCK_START "\n"
			"\t\t// %u bytes\n"
			"\t\t\".byte 0x%02x",
			code->start_ofs,
			code->bytes[0]);
		for (i=1; i<code->start_ofs; i++)
			printf(", 0x%02x", code->bytes[i]);
		printf("\"\n"
			"\t" ASM_BLOCK_END "\n"
			"}\n\n");
	}

	if (i < code->count) {
		printf(PROTOTYPE("oops") "\n");
		printf("void oops(void)\n{\n");
		printf("\t" ASM_BLOCK_START "\n"
			"\t\t// %u bytes\n"
			"\t\t\".byte 0x%02x",
			code->count - i,
			code->bytes[i]);
		for (i++; i<code->count; i++)
			printf(", 0x%02x", code->bytes[i]);
		printf("\"\n"
			"\t" ASM_BLOCK_END "\n"
			"}\n\n");
	}
}

int main(void)
{
	const char *text = NULL;
	struct code *code;

	text = find_oops_code(stdin);
	printf("/*\n * Code: %s */\n\n", text);

	code = parse_oops_code(text);
	print_code(code, stdout);

	return 0;
}
