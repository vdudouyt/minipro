
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <errno.h>

#include "version.h"
#include "minipro.h"
#include "database.h"
#include "StrHexToNum.h"


#define MAX_CHIP_FILE_SIZE	(1024 * 1024 * 1024) /* 1Gb */

#define LOG_ERR(__error, __descr)					\
	if (0 != (__error))						\
		fprintf(stderr, "%s:%i %s: error: %i - %s: %s\n",	\
		    __FILE__, __LINE__, __FUNCTION__,			\
		    (__error), strerror((__error)), (__descr))

#define LOG_ERR_FMT(__error, __fmt, args...)				\
	    if (0 != (__error))						\
		fprintf(stderr, "%s:%i %s: error: %i - %s: " __fmt "\n", \
		    __FILE__, __LINE__, __FUNCTION__,			\
		    (__error), strerror((__error)), ##args)

#define sstrlen(__str)	((NULL == (__str)) ? 0 : strlen((__str)))



typedef struct command_line_options_s {
	int		action;
	char 		*file_name;
	uint32_t	write_flags;
	int		post_wr_verify;
	off_t		file_offset;
	char		*chip_name;
	uint32_t	chip_id;
	uint8_t		chip_id_size;
	uint32_t	address;
	size_t		size;
	int		page;
	uint8_t		icsp;
	int		chip_id_check_no_fail;
	int		size_error;
	int		size_error_no_warn;
	int		quiet;
} cmd_opts_t, *cmd_opts_p;


static struct option lopts[] = {
	{ "read",	required_argument,	NULL,	'r'	},
	{ "verify",	required_argument,	NULL,	0	},
	{ "write",	required_argument,	NULL,	'w'	},
	{ "no-erase",	no_argument,		NULL,	'e'	},
	{ "no-pre-unprotect",	no_argument,	NULL,	'u'	},
	{ "no-post-protect",	no_argument,	NULL,	'P'	},
	{ "no-post-verify",	no_argument,	NULL,	'v'	},
	{ "file-offset", required_argument,	NULL,	0	},
	{ "chip",	required_argument,	NULL,	'p'	},
	{ "chip-id",	required_argument,	NULL,	0	},
	{ "addr",	required_argument,	NULL,	0	},
	{ "size",	required_argument,	NULL,	0	},
	{ "page",	required_argument,	NULL,	'c'	},
	{ "icsp",	no_argument,		NULL,	'I'	},
	{ "icsp-vcc",	no_argument,		NULL,	'i'	},
	{ "db-dump",	optional_argument,	NULL,	'l'	},
	{ "chip-id-check-no-fail", no_argument,	NULL,	'y'	},
	{ "no-size-error", no_argument,		NULL,	's'	},
	{ "no-size-error-warn", no_argument,	NULL,	'S'	},
	{ "quiet",	no_argument,		NULL,	0	},
	{ "help",	no_argument,		NULL,	'?'	},
	{ NULL,		0,			NULL,	0	}
};

static const char *lopts_descr[] = {
	"<file_name>		Read memory",
	"<file_name>		Verify memory",
	"<file_name>		Write memory",
	"			Do NOT erase chip",
	"		Do NOT disable write-protect before write",
	"		Do NOT enable write-protect after write",
	"		Do NOT verify after write",
	"<offset>		File offset (hex)",
	"<chip>		Specify chip (use quotes)",
	"<chip_id>		Specify chip ID (hex)",
	"<chip_addr>		Start pos for read/verify/write",
	"<size>			Size to read/verify/write (hex)",
	"<page>		Specify memory page type (optional)\n"
	"					Possible values: code, data, config",
	"			Use ICSP (without enabling Vcc)",
	"			Use ICSP",
	"			List all supported devices",
	"	Do NOT error on ID mismatch",
	"		Do NOT error on file size mismatch (only a warning)",
	"	No warning message for file size mismatch (can't combine with -s)",
	"				Less verboce",
	"			Show help",
	NULL
};


static int
cmd_opts_parse(int argc, char **argv, cmd_opts_p cmd_opts) {
	int i, ch, opt_idx;
	char opts_str[1024], tmbuf[16];


	memset(cmd_opts, 0x00, sizeof(cmd_opts_t));
	cmd_opts->action = -1;
	cmd_opts->page = MP_CHIP_PAGE_CODE;
	cmd_opts->post_wr_verify = 1;
	cmd_opts->size_error = 1;

	/* Process command line. */
	/* Generate opts string from long options. */
	for (i = 0, opt_idx = 0; NULL != lopts[i].name && (int)(sizeof(opts_str) - 1) > opt_idx; i ++) {
		if (0 == lopts[i].val)
			continue;
		opts_str[opt_idx ++] = (char)lopts[i].val;
		switch (lopts[i].has_arg) {
		case optional_argument:
			opts_str[opt_idx ++] = ':';
		case required_argument:
			opts_str[opt_idx ++] = ':';
		}
	}
	opts_str[opt_idx] = 0;
	opt_idx = -1;
	while ((ch = getopt_long_only(argc, argv, opts_str, lopts, &opt_idx)) != -1) {
restart_opts:
		switch (opt_idx) {
		case -1: /* Short option to index. */
			for (opt_idx = 0; NULL != lopts[opt_idx].name; opt_idx ++) {
				if (ch == lopts[opt_idx].val)
					goto restart_opts;
			}
			/* Unknown option. */
			break;
		case 0: /* read */
		case 1: /* verify */
		case 2: /* write */
			if (-1 != cmd_opts->action) {
				fprintf(stderr, "write / read / verify - can not be combined, select one of them.\n");
				return (EINVAL);
			}
			cmd_opts->action = opt_idx;
			cmd_opts->file_name = optarg;
			break;
		case 3: /* no-erase */
			cmd_opts->write_flags |= MP_PAGE_WR_F_NO_ERASE;
			break;
		case 4: /* no-pre-unprotect */
			cmd_opts->write_flags |= MP_PAGE_WR_F_PRE_NO_UNPROTECT;
			break;
		case 5: /* no-post-protect */
			cmd_opts->write_flags |= MP_PAGE_WR_F_POST_NO_PROTECT;
			break;
		case 6: /* no-post-verify */
			cmd_opts->post_wr_verify = 0;
			break;
		case 7: /* file-offset */
			cmd_opts->file_offset = (off_t)StrHexToUNum64(optarg, sstrlen(optarg));
			break;
		case 8: /* chip */
			cmd_opts->chip_name = optarg;
			break;
		case 9: /* chip-id */
			cmd_opts->chip_id = StrHexToUNum32(optarg, sstrlen(optarg));
			cmd_opts->chip_id_size = (uint8_t)snprintf(tmbuf, sizeof(tmbuf), "%x", cmd_opts->chip_id);
			cmd_opts->chip_id_size = ((cmd_opts->chip_id_size + 1) & ~0x01);
			break;
		case 10: /* addr */
			cmd_opts->address = StrHexToUNum32(optarg, sstrlen(optarg));
			break;
		case 11: /* size */
			cmd_opts->size = StrHexToUNum(optarg, sstrlen(optarg));
			break;
		case 12: /* page */
			for (i = MP_CHIP_PAGE_CODE; MP_CHIP_PAGE__COUNT__ > i; i ++) {
				if (strcasecmp(mp_chip_page_str[i], optarg))
					continue;
				cmd_opts->page = i;
				break;
			}
			if (MP_CHIP_PAGE__COUNT__ == i) {
				fprintf(stderr, "Unknown memory type: \"%s\".\n", optarg);
				return (EINVAL);
			}
			break;
		case 13: /* icsp */
			cmd_opts->icsp = MP_ICSP_FLAG_ENABLE;
			break;
		case 14: /* icsp-vcc */
			cmd_opts->icsp = (MP_ICSP_FLAG_ENABLE | MP_ICSP_FLAG_VCC);
			break;
		case 15: /* db-dump */
			chip_db_dump_flt(optarg);
			return (-1);
		case 16: /* chip-id-check-no-fail */
			cmd_opts->chip_id_check_no_fail = 1;
			break;
		case 17: /* no-size-error */
			cmd_opts->size_error = 0;
			break;
		case 18: /* no-size-error-warn */
			cmd_opts->size_error = 0;
			cmd_opts->size_error_no_warn = 1;
			break;
		case 19: /* quiet */
			cmd_opts->quiet = 1;
			break;
		default:
			return (EINVAL);
		}
		opt_idx = -1;
	}

	return (0);
}

static void
print_usage(const char *progname) {
	size_t i;
	const char *usage =
		"minipro version %s     A free and open TL866XX programmer\n"
		"Usage: %s [options]\n"
		"options:\n";
	fprintf(stderr, usage, VERSION, basename(progname));

	for (i = 0; NULL != lopts[i].name; i ++) {
		if (0 == lopts[i].val) {
			fprintf(stderr, "	-%s %s\n", lopts[i].name, lopts_descr[i]);
		} else {
			fprintf(stderr, "	-%s, -%c %s\n", lopts[i].name, lopts[i].val, lopts_descr[i]);
		}
	}
}

static void
progress_cb(minipro_p mp __unused, size_t done, size_t total, void *udata) {

	if (done == total) {
		printf("\r\e[K%sOK.\n", (const char*)udata);
	} else {
		printf("\r\e[K%s%zu / %zu bytes", (const char*)udata, done, total);
	}
	fflush(stdout);
}

static int
get_file_size(const char *file_name, size_t *file_size) {
	struct stat sb;

	if (NULL == file_name || NULL == file_size)
		return (EINVAL);
	if (0 != stat(file_name, &sb))
		return (errno);
	(*file_size) = (size_t)sb.st_size;

	return (0);
}

static int
read_file(const char *file_name, off_t offset, size_t size, size_t max_size,
    uint8_t **buf, size_t *buf_size) {
	int fd, error;
	ssize_t rd;
	struct stat sb;

	if (NULL == file_name || NULL == buf || NULL == buf_size)
		return (EINVAL);
	/* Open file. */
	fd = open(file_name, O_RDONLY);
	if (-1 == fd)
		return (errno);
	/* Get file size. */
	if (0 != fstat(fd, &sb)) {
		error = errno;
		goto err_out;
	}
	/* Check size and offset. */
	if (0 != size) {
		if ((offset + (off_t)size) > sb.st_size) {
			error = EINVAL;
			goto err_out;
		}
	} else {
		/* Check overflow. */
		if (offset >= sb.st_size) {
			error = EINVAL;
			goto err_out;
		}
		size = (size_t)(sb.st_size - offset);
		if (0 != max_size && max_size < size) {
			(*buf_size) = size;
			error = EFBIG;
			goto err_out;
		}
	}
	/* Allocate buf for file content. */
	(*buf_size) = size;
	(*buf) = malloc((size + sizeof(void*)));
	if (NULL == (*buf)) {
		error = ENOMEM;
		goto err_out;
	}
	/* Read file content. */
	rd = pread(fd, (*buf), size, offset);
	close(fd);
	if (-1 == rd) {
		error = errno;
		free((*buf));
		(*buf) = NULL;
		return (error);
	}
	(*buf)[size] = 0;
	return (0);

err_out:
	close(fd);
	return (error);
}


int
main(int argc, char **argv) {
	int error = 0;
	cmd_opts_t cmd_opts;
	minipro_p mp = NULL;
	chip_p chip = NULL;
	int fd;
	uint32_t chip_id, chip_val, buf_val;
	uint8_t chip_id_size, *file_data = NULL, *chip_data = NULL;
	size_t file_size, file_data_size, chip_data_size, chip_size = 0, tr_size, err_offset;
	char status_msg[64];

	error = cmd_opts_parse(argc, argv, &cmd_opts);
	if (0 != error) {
		if (-1 == error)
			return (0); /* Handled action. */
		print_usage(argv[0]);
		return (error);
	}

	/* Find chip. */
	if (NULL != cmd_opts.chip_name) { /* By name. */
		chip = chip_db_get_by_name(cmd_opts.chip_name);
		if (NULL == chip) {
			fprintf(stderr, "Chip \"%s\" not found, try one of listed below or type: %s --db-dump | grep %s\n",
			    cmd_opts.chip_name, basename(argv[0]), cmd_opts.chip_name);
			chip_db_dump_flt(cmd_opts.chip_name);
			return (-1);
		}
	} else if (0 != cmd_opts.chip_id_size) { /* By ID. */
		chip = chip_db_get_by_id(cmd_opts.chip_id, cmd_opts.chip_id_size);
		if (NULL == chip) {
			fprintf(stderr, "Chip not found.\n");
			return (-1);
		}
	}
	/* Display chip info. */
	if (0 == cmd_opts.quiet) {
		chip_db_print_info(chip);
	}

	/* Open MiniPro. */
	error = minipro_open(MP_TL866_VID, MP_TL866_PID, (0 == cmd_opts.quiet), &mp);
	if (0 != error)
		return (error);
	/* Check and print device info. */
	if (0 == cmd_opts.quiet) {
		minipro_print_info(mp);
	}

	/* Check some command line options before continue. */
	if (NULL == chip) { /* Is chip specified? */
		fprintf(stderr, "Chip not specified, can not continue.\n");
		error = -1;
		goto err_out;
	}
	switch (cmd_opts.page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == cmd_opts.page) ? chip->code_memory_size : chip->data_memory_size);
		if (0 == chip_size) {
			fprintf(stderr, "chip page \"%s\" size = 0 - does not exist.\n",
			    mp_chip_page_str[cmd_opts.page]);
			error = EINVAL;
			goto err_out;
		}
		if (0 != cmd_opts.address &&
		    0 != cmd_opts.size) {
			if ((cmd_opts.size - cmd_opts.address) > chip_size) {
				fprintf(stderr, "options error: chip page \"%s\" size = %zu, (addr + size) = %zu + %zu = %zu.\n",
				    mp_chip_page_str[cmd_opts.page], chip_size,
				    (size_t)cmd_opts.address, cmd_opts.size,
				    ((size_t)cmd_opts.address + cmd_opts.size));
				error = EINVAL;
				goto err_out;
			}
		} else if (0 != cmd_opts.address) {
			if (cmd_opts.address > chip_size) {
				fprintf(stderr, "options error: chip page \"%s\" size = %zu, addr = %zu is out of range.\n",
				    mp_chip_page_str[cmd_opts.page],
				    chip_size, (size_t)cmd_opts.address);
				error = EINVAL;
				goto err_out;
			}
		} else if (0 != cmd_opts.size) {
			if (cmd_opts.size > chip_size) {
				fprintf(stderr, "options error: chip page \"%s\" size = %zu, size = %zu is out of range.\n",
				    mp_chip_page_str[cmd_opts.page],
				    chip_size, cmd_opts.size);
				error = EINVAL;
				goto err_out;
			}
		}
		if (0 == cmd_opts.size) { /* Fixup size. */
			tr_size = (chip_size - cmd_opts.address);
		} else {
			tr_size = cmd_opts.size;
		}
		if (0 == tr_size) {
			fprintf(stderr, "Data to transfer size seto to 0, nothink to do.\n");
			error = -1;
			goto err_out;
		}
		printf("Will transfer: %zu bytes, starting from: 0x%08x.\n",
		    tr_size,  cmd_opts.address);
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (0 != cmd_opts.file_offset ||
		    0 != cmd_opts.address ||
		    0 != cmd_opts.size) {
			fprintf(stderr, "chip page \"%s\" does not allow to set options: file-offset, addr, size.\n",
			     mp_chip_page_str[MP_CHIP_PAGE_CONFIG]);
			error = EINVAL;
			goto err_out;
		}
		tr_size = 0; /* Autodetect from file size. */
		break;
	default:
		error = EINVAL;
		goto err_out;
	}

	/* Set chip info. */
	error = minipro_chip_set(mp, chip, cmd_opts.icsp);
	if (0 != error)
		goto err_out;

	/* Verify Chip ID (if applicable). */
	if (chip->chip_id_size &&
	    chip->chip_id) {
		error = minipro_get_chip_id(mp, &chip_id, &chip_id_size);
		if (0 != error) {
			LOG_ERR(error, "Fail on chip ID read.");
			goto err_out;
		}
		if (is_chip_id_prob_eq(chip, chip_id, chip_id_size)) {
			printf("Chip ID OK: expected 0x%02x, got 0x%02x.\n",
			    chip->chip_id, chip_id);
		} else {
			if (0 != cmd_opts.chip_id_check_no_fail) {
				printf("WARNING: Chip ID mismatch: expected 0x%02x, got 0x%02x.\n",
				    chip->chip_id, chip_id);
				chip_db_print_info(chip_db_get_by_id(chip_id, chip_id_size));
			} else {
				fprintf(stderr, "Invalid Chip ID: expected 0x%02x, got 0x%02x\n(use '-y' to continue anyway at your own risk).\n",
				    chip->chip_id, chip_id);
				chip_db_print_info(chip_db_get_by_id(chip_id, chip_id_size));
				error = -1;
				goto err_out;
			}
		}
	}

	/* Do action/work. */
	switch (cmd_opts.action) {
	case 0: /* read. */
		snprintf(status_msg, sizeof(status_msg), "Reading %s... ",
		    mp_chip_page_str[cmd_opts.page]);
		error = minipro_page_read(mp,
		    cmd_opts.page, cmd_opts.address, tr_size,
		    &chip_data, &chip_data_size, progress_cb, (void*)status_msg);
		if (0 != error) {
			LOG_ERR(error, "Fail on chip read.");
			goto err_out;
		}
		/* Save/update file. */
		fd = open(cmd_opts.file_name, (O_WRONLY | O_CREAT), 0600);
		if (-1 == fd) {
			error = errno;
			LOG_ERR(error, "Fail on file open for chip dump writing.");
			goto err_out;
		}
		if (chip_data_size != (size_t)pwrite(fd, chip_data, chip_data_size, cmd_opts.file_offset)) {
			error = errno;
			LOG_ERR(error, "Fail on chip write data to file.");
		}
		close(fd);
		break;
	case 1: /* verify. */
	case 2: /* write. */
		error = get_file_size(cmd_opts.file_name, &file_size);
		if (0 != error) {
			LOG_ERR(error, "Fail on get file size.");
			goto err_out;
		}

		if (file_size <= (size_t)cmd_opts.file_offset ||
		    (file_size - (size_t)cmd_opts.file_offset) < tr_size) {
			if (0 != cmd_opts.size_error) {
				fprintf(stderr, "Incorrect file size and offset: %zu - %zu = %zu, needed at least %zu.\n",
				    file_size, (size_t)cmd_opts.file_offset,
				    (file_size - (size_t)cmd_opts.file_offset),
				    tr_size);
				error = -1;
				goto err_out;
			} else if (0 == cmd_opts.size_error_no_warn) {
				printf("Warning: Incorrect file size and offset: %zu - %zu = %zu, needed at least %zu.\n",
				    file_size, (size_t)cmd_opts.file_offset,
				    (file_size - (size_t)cmd_opts.file_offset),
				    tr_size);
			}
		}
		/* Loading file. */
		/* file_data_size = ((0 != tr_size) ? tr_size : (file_size - cmd_opts.file_offset)); */
		error = read_file(cmd_opts.file_name, cmd_opts.file_offset, tr_size,
		    MAX_CHIP_FILE_SIZE, &file_data, &file_data_size);
		if (0 != error) {
			LOG_ERR(error, "Fail on file read.");
			goto err_out;
		}
		if (2 == cmd_opts.action) { /* write. */
			snprintf(status_msg, sizeof(status_msg), "Writing %s... ",
			    mp_chip_page_str[cmd_opts.page]);
			error = minipro_page_write(mp,
			    cmd_opts.write_flags, cmd_opts.page, cmd_opts.address,
			    file_data, file_data_size, progress_cb, (void*)status_msg);
			if (0 != error) {
				LOG_ERR(error, "Fail on chip write.");
				goto err_out;
			}
			if (0 == cmd_opts.post_wr_verify) /* Verify disabled. */
				break;
		}
		/* verify. */
		snprintf(status_msg, sizeof(status_msg), "Verifying %s... ",
		    mp_chip_page_str[cmd_opts.page]);
		error = minipro_page_verify(mp, cmd_opts.page, cmd_opts.address,
		    file_data, file_data_size, &err_offset, &buf_val, &chip_val,
		    progress_cb, (void*)status_msg);
		if (0 != error) {
			LOG_ERR(error, "Fail on chip read.");
			goto err_out;
		}
		if (err_offset < file_data_size) { /* Not euqual. */
			switch (cmd_opts.page) {
			case MP_CHIP_PAGE_CODE:
			case MP_CHIP_PAGE_DATA:
				fprintf(stderr, "\nVerification failed at 0x%02zx: 0x%02x (file) != 0x%02x (chip).\n",
				    err_offset, buf_val, chip_val);
				break;
			case MP_CHIP_PAGE_CONFIG:
				fprintf(stderr, "\nVerification failed fuse 0x%02zx - %s: 0x%02x (file) != 0x%02x (chip).\n",
				    err_offset,
				    chip->fuses[err_offset].name,
				    buf_val, chip_val);
				break;
			}
		}
		break;
	default:
		fprintf(stderr, "write / read / verify - not specified, nothink to do.\n");
		error = -1;
	}


err_out:
	free(chip_data);
	free(file_data);
	minipro_close(mp);

	return (error);
}
