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

static void print_code(const struct code *code)
{
	int i;
	printf("#");
	for (i=0; i<code->count; i++)
		printf(" %s%02x%s",
				i == code->start_ofs ? "<" : "",
				code->bytes[i],
				i == code->start_ofs ? ">" : "");
	printf("\n");
}

int main(void)
{
	const char *text = NULL;
	struct code *code;

	text = find_oops_code(stdin);
	printf("# %s", text);

	code = parse_oops_code(text);
	print_code(code);

	return 0;
}
