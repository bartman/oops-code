#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

static void print_code(const struct code *code, FILE *out)
{
	int i = 0;

	fprintf(out,
		".text\n"
		".global main\n"
		"main:\n"
		"nop\n"
		"\n");

	fprintf(out,
		".section \".oops\"\n");

	if (code->start_ofs) {
		fprintf(out,
			".global before\n"
			"before:\n"
			"// %u bytes\n"
			".byte 0x%02x",
			code->start_ofs,
			code->bytes[0]);
		for (i=1; i<code->start_ofs; i++)
			fprintf(out,
				", 0x%02x", code->bytes[i]);
		fprintf(out,
			"\n"
			"\n");
	}

	if (i < code->count) {
		fprintf(out,
			".global oops\n"
			"oops:\n"
			"// %u bytes\n"
			".byte 0x%02x",
			code->count - i,
			code->bytes[i]);
		for (i++; i<code->count; i++)
			fprintf(out,
				", 0x%02x", code->bytes[i]);
		fprintf(out,
			"\n"
			"\n");
	}
}

static void gen_names(char **s_name, char **x_name)
{
	int pid = getpid();

	*s_name = malloc(128);
	assert(*s_name);
	snprintf(*s_name, 128, "/tmp/oops-code-%u.S", pid);

	*x_name = malloc(128);
	assert(*x_name);
	snprintf(*x_name, 128, "/tmp/oops-code-%u", pid);
}

static void process(FILE *oops)
{
	const char *text = NULL;
	struct code *code;

	off_t oops_ip;

	char *asm_name;
	char *exe_name;

	int asm_fd;
	FILE *asm_file;

	char *cmd;
	int rc;

	oops_ip = 0xc01a516b;

	gen_names(&asm_name, &exe_name);

	asm_fd = creat(asm_name, O_CREAT | O_EXCL | O_WRONLY
			| S_IRUSR | S_IWUSR);
	assert(asm_fd > 0);

	asm_file = fdopen(asm_fd, "w");
	assert(asm_file);

	text = find_oops_code(oops);
	printf("# Code: %s \n", text);
	fflush(stdout);

	code = parse_oops_code(text);
	print_code(code, asm_file);

	fclose(asm_file);
	close(asm_fd);

	cmd = malloc(1024);
	assert(cmd);

	snprintf(cmd, 1024,
		"gcc -m32 -ggdb "
		"-Xlinker --section-start -Xlinker .oops=0x%08lx "
		"-o %s %s",
		oops_ip, exe_name, asm_name);

	rc = system (cmd);
	assert(rc == 0);

	snprintf(cmd, 1024,
		"objdump -D -j .oops "
		"--start-address=0x%08lx "
		"--stop-address=0x%08lx %s",
		oops_ip,
		oops_ip + code->count,
		exe_name);

	rc = system (cmd);
	assert(rc == 0);

	unlink (asm_name);
	unlink (exe_name);
}

int main(void)
{
	process(stdin);

	return 0;
}
