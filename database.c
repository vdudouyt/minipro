#include "database.h"
#include <stdio.h>
#include <strings.h>

device_t devices[] = {
	#include "devices.h"
	{ .name = NULL },
};

device_t *get_device_by_name(const char *name) {
	device_t *device;
	for(device = &(devices[0]); device[0].name; device = &(device[1])) {
		if(!strcasecmp(name, device->name))
			return(device);
	}
	return NULL;
}
