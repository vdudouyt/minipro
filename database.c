#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "database.h"

static device_t devices[] = {
	#include "devices.h"
	{ .name = NULL },
};


device_p
dev_db_get_by_name(const char *name) {
	device_p dev;

	for (dev = devices; dev->name; dev ++) {
		if (!strcasecmp(name, dev->name))
			return (dev);
	}

	return (NULL);
}

void
dev_db_dump_flt(const char *dname) {
	size_t dname_size = 0;
	device_p dev;

	if (NULL != dname) {
		dname_size = strnlen(dname, DEVICE_NAME_MAX);
	}
	for (dev = devices; dev->name; dev ++) {
		if (0 != dname_size &&
		    strncasecmp(dev->name, dname, dname_size))
			continue;
		printf("%s\n", dev->name);
	}
}
