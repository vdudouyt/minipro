// Dmitry Smirnov, 2013
// Valentin Dudouyt, 2014
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "easyconfig.h"

#define LINE_LENGTH 200 

FILE *pFile;
char param_value[50];
char* config_path;
int config_lines_qty=0;

struct  conf{ 
	char config_line[LINE_LENGTH];
	char param_name[40]; 
	char param_value[40];
} *config_content;

static char *str_trim_left(char *str) {
	while(isspace(*str)) str++;
	return str;
}

static char *str_trim_right(char *str) {
	char *end = str + strlen(str) - 1;
	while(isspace(*end) && end >= str) {
		*end = '\0'; end--;
	}
	return str;
}

int Config_init(const char *path) {
	pFile=fopen(path,"w");
	if (pFile == NULL) { return 1; }
	return Config_open(path);
}

int Config_open(const char *path) {
	int ret=0, counter=0, counter1=1;
	char temp[LINE_LENGTH], *result;

	config_content = (struct conf*)calloc(1, sizeof(struct conf));
	config_path = (char*) path;

	if(!(pFile=fopen(config_path,"r"))) {
		return 1;
	}
	while (fgets(temp,LINE_LENGTH,pFile)) {
		config_lines_qty++;
	}

	config_content = (struct conf*)realloc(config_content, (config_lines_qty+50)*sizeof(struct conf));

	int i;
	for (i=0;i<config_lines_qty+50;i++) {
		strcpy(config_content[i].config_line, "");
		strcpy(config_content[i].param_name, "");
		strcpy(config_content[i].param_value, "");
	}
	
	rewind(pFile);
	while (fgets(temp,LINE_LENGTH,pFile)) {
		if (strlen(temp) == LINE_LENGTH -1) { printf("Config error. Line too long: %d\n", counter1); ret=1; errno=EINVAL; break; }
		if (strlen(temp)<4) { 
			counter1++;
			continue;
		}

		strcpy(config_content[counter].config_line,temp);
		result=strtok(temp,"="); 
		result=str_trim_right(result);
		if (!result) { printf("Config error. Line: %d\n", counter1); ret=1; errno=EINVAL; break; }
		if (strlen(result) >= sizeof(config_content[counter].param_name)) { printf("Config error. Key too long on line: %d\n", counter1); ret=1; errno=EINVAL; break; }
		strcpy(config_content[counter].param_name,result);
		result=strtok(NULL,"\n"); 
		result=str_trim_left(result);
		if (!result) { printf("Config error. Line: %d\n", counter1); ret=1; errno=EINVAL; break; }
		if (strlen(result) >= sizeof(config_content[counter].param_value)) { printf("Config error. Value too long on line: %d\n", counter1); ret=1; errno=EINVAL; break; }
		strcpy(config_content[counter].param_value,result);
		
		counter++; counter1++;
	}
	fclose(pFile);
	return ret;
}

char *Config_get_str(const char *par_name) {
	int i;
	for (i=0;i<config_lines_qty;i++) {
		
		if (!strcmp(config_content[i].param_name,par_name)) {
			if (strlen(param_value) >= sizeof(config_content[i].param_value)) { break; }
			strcpy(param_value,config_content[i].param_value);
			return param_value;
		}
	}
	return NULL;
}

int Config_set_str(const char *par_name, const char *value) {
	int i;

	for (i=0;i<config_lines_qty;i++) { 
		if (!strcmp(config_content[i].param_name,par_name)) {
			if (strlen(param_value) >= sizeof(config_content[i].param_value)) { return -1; }
			strcpy(config_content[i].param_value, value);
			if (strlen(par_name) + strlen(value) + 4 >= sizeof(config_content[i].config_line)) { return -1; }
			sprintf(config_content[i].config_line,"%s = %s\n",par_name,value);
			return 0;
		}
	}

	if (strlen(par_name) + strlen(value) + 4 >= sizeof(config_content[i].config_line)) { return -1; }
	sprintf(config_content[i].config_line,"%s = %s\n",par_name,value);
	if (strlen(par_name) >= sizeof(config_content[i].param_name)) { return -1; }
	strcpy(config_content[i].param_name, par_name);
	if (strlen(param_value) >= sizeof(config_content[i].param_value)) { return -1; }
	strcpy(config_content[i].param_value, value);
	
	config_lines_qty++;

	return 0;
}

int Config_get_int(const char *par_name) {
	unsigned int intval;
	char *strval = Config_get_str(par_name);
	if (strval == NULL)
		return -1;
	if(!sscanf(strval, "0x%04x", &intval) && !sscanf(strval, "%d", &intval)) {
		return -1;
	}
	return(intval);
}

int Config_set_int(const char *par_name, unsigned int value) {
	char strval[16];
	sprintf(strval, "0x%04x", value);
	return Config_set_str(par_name, strval);
}

int Config_close() {
	int i=0;
	pFile=fopen(config_path,"w");
	
	while (i<config_lines_qty) {
		fputs (config_content[i].config_line,pFile);
		
		i++;
	}
	fclose(pFile);
	free (config_content);
	return 0;
}
