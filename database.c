/*
 * database.c - Functions for dealing with the device database.
 *
 * This file is a part of Minipro.
 *
 * Minipro is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Minipro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

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
