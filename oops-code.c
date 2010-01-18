#define _GNU_SOURCE
#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char *program = "oops-code";
const char *argp_program_version = "oops-code 0.0";
const char *argp_program_bug_address = "<bart@jukie.net>";

static char doc[] = "oops-code - parses oops messages";

static char args_doc[] = "ARG1 ARG2";

static struct argp_option options[] = {
	{"bits",     'b', "NUM",  0,  "Produce verbose output" },
	{NULL,       'm', "NUM",  OPTION_ALIAS },
	{"verbose",  'v', 0,      0,  "Produce verbose output" },
	{"quiet",    'q', 0,      0,  "Don't produce any output" },
	{ 0 }
};

struct conf {
	int silent, verbose;
	FILE *input;
	unsigned bits;
};

struct oops {
	const char *code_text;
	const char *ip_text;
};



static void __warnf(int __errno, const char *fmt, va_list ap)
{
	fflush(stdout);
	fprintf(stderr, "%s: ", basename(program));
	vfprintf(stderr, fmt, ap);
	if (__errno)
		fprintf(stderr, ": %s.\n", strerror(errno));
	else
		fprintf(stderr, ".\n");
}

static void warn(const char *fmt, ...)
{
	va_list ap;
	int __errno = errno;

	va_start(ap, fmt);
	__warnf(__errno, fmt, ap);
	va_end(ap);
}

static void die(const char *fmt, ...)
{
	va_list ap;
	int __errno = errno;

	va_start(ap, fmt);
	__warnf(__errno, fmt, ap);
	va_end(ap);

	exit(1);
}

static int read_oops(FILE *in, struct oops *oops)
{
	char *buff = NULL;
	size_t buff_size = 0;
	ssize_t buff_len;

	while ((buff_len = getline(&buff, &buff_size, in)) != -1) {
		char *p = buff;
		char *e;

		while (isspace(*p)) p++;

		if (*p == '[') {
			do { p++; } while (*p==' ' || *p=='.' || isdigit(*p));
			if (*p!=']') continue;
			do { p++; } while (*p==' ');
		}

		e = p + strlen(p) - 1;
		while (e > p && (isspace(*e) || iscntrl(*e))) *(e--) = 0;

		if (!strncmp(p, "Code: ", 6))
			oops->code_text = strdup(p + 6);

		else if (!strncmp(p, "RIP: ", 5))
			oops->ip_text = strdup(p + 5);

		else if (!strncmp(p, "EIP: ", 5))
			oops->ip_text = strdup(p + 5);

		else if (!strncmp(p, "---[ end trace ", 15))
			break;
	}

	return !oops->code_text || !oops->ip_text;
}

#define have(cnt,check,ptr) ({ \
	int __i, __ret = 1; \
	const char *__ptr = (ptr); \
	for (__i = 0; __i < (cnt); __i++) { \
		if (is##check(*(__ptr++))) continue; \
		__ret = 0; \
		break; \
	} \
	__ret; \
})

static off_t parse_oops_addr(struct conf *conf, const char *text)
{
	unsigned long long addr = 0;
	int i, width;
	const char *p = text;
	const char *e = p + strlen(p) - 1;

	/*
	 * We could see things like this:
	 *
	 *    0060:[<90628d27>] EFLAGS: 00010202 CPU: 3
	 *    [<90628d27>] sock_common_recvmsg+0x2c/0x45 SS:ESP 0068:f6c7dd98
	 *       0060:[<c01a5196>]    Tainted: P     U VLI
	 *    0010:[<ffffffff810b82f7>]  [<ffffffff810b82f7>] do_sys_poll+
	*/

	while (p<e && isblank(*p)) p++;
	while (p<e && isblank(*e)) e--;

	if (have(4,xdigit,p) && p[4] == ':')
		p += 5;

	if (p[0] != '[' || p[1] != '<') {
		warn("Expecting '[<' at '%s'", p);
		return 0;
	}
	p += 2;

	if (conf->bits) {
		width = conf->bits / 4;
		if (!have(width, xdigit, p)) {
			warn("Expecting %d hexdigits at '%s'", width, p);
			return 0;
		}
	} else {
		for (width = sizeof(off_t)*2; width; width -= 8)
			if (have(width, xdigit, p))
				break;
		if (!width) {
			warn("Expecting hexdigits at '%s'", p);
			return 0;
		}

		conf->bits = width * 4;
		printf("# Guessing platform is %d bits\n", conf->bits);
	}

	printf("# Width: %d\n", width);

	if (p[width] != '>' || p[width+1] != ']') {
		warn("Expecting '>]' at '%s'", p);
		return 0;
	}

	for (i = 0; i < width; i++) {
		char c = tolower(*(p++));
		addr <<= 4;
		addr |= isdigit(c) ? (c - '0') : (c - 'a' + 10);
	}

	printf("# Addr: 0x%0*llx\n", width, addr);

	return addr;
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

static void process(struct conf *conf)
{
	struct oops oops = { 0, 0 };
	struct code *code;

	off_t addr;

	char *asm_name;
	char *exe_name;

	int asm_fd;
	FILE *asm_file;

	char *cmd;
	int rc;

	gen_names(&asm_name, &exe_name);

	asm_fd = creat(asm_name, O_CREAT | O_EXCL | O_WRONLY
			| S_IRUSR | S_IWUSR);
	assert(asm_fd > 0);

	asm_file = fdopen(asm_fd, "w");
	assert(asm_file);

	if (read_oops(conf->input, &oops))
		die("Parsing failure");
	printf("# RIP:  %s \n", oops.ip_text);
	printf("# Code: %s \n", oops.code_text);
	fflush(stdout);

	addr = parse_oops_addr(conf, oops.ip_text);
	if (!addr)
		die("Cannot extract IP from '%s'", oops.ip_text);

	code = parse_oops_code(oops.code_text);
	print_code(code, asm_file);

	fclose(asm_file);
	close(asm_fd);

	cmd = malloc(1024);
	assert(cmd);

	snprintf(cmd, 1024,
		"gcc -m%u -ggdb "
		"-Xlinker --section-start -Xlinker .oops=0x%08lx "
		"-o %s %s",
		conf->bits,
		addr, exe_name, asm_name);

	rc = system (cmd);
	assert(rc == 0);

	snprintf(cmd, 1024,
		"objdump -D -j .oops "
		"--start-address=0x%08lx "
		"--stop-address=0x%08lx %s",
		addr,
		addr + code->count,
		exe_name);

	rc = system (cmd);
	assert(rc == 0);

	unlink (asm_name);
	unlink (exe_name);
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
	struct conf *conf = state->input;

	switch (key)
	{
	case 'q':
		conf->silent = 1;
		break;

	case 'v':
		conf->verbose = 1;
		break;

	case 'b':
	case 'm':
		conf->bits = atoi(arg);
		switch (conf->bits) {
		case 32:
		case 64:
			break;
		default:
			die("Only 32 and 64 are supported for --bits");
		}
		break;

	case ARGP_KEY_ARG:
		if (state->arg_num >= 1)
			/* Too many arguments. */
			argp_usage (state);

		conf->input = fopen(arg, "r");
		if (!conf->input)
			die("Could not open '%s' for reading", arg);

		fprintf(stderr, "Reading from '%s'.\n", arg);
		break;

	case ARGP_KEY_END:
		if (!conf->input) {
			fprintf(stderr, "Reading from stdin.\n");
			conf->input = stdin;
		}
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char *argv[])
{
	struct conf conf;

	memset(&conf, 0, sizeof(conf));

	program = argv[0];
	argp_parse(&argp, argc, argv, 0, 0, &conf);

	process(&conf);

	return 0;
}

