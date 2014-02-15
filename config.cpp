#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

#define MAXCONFIGS 300

typedef struct {
	char key[50];
	char stringvalue[200];
} t_config;

char config_filename[200];
t_config *configs = NULL;
int num_configs = 0;

void config_init(const char *filename) {
	strcpy(config_filename, filename);

	// alloc / reset memory.
	if (configs == NULL) {
		configs = (t_config *)malloc(sizeof(t_config) * MAXCONFIGS);
	}

	memset(configs, 0, sizeof(t_config) * MAXCONFIGS);
	num_configs = 0;

	// load file
	FILE *f = fopen(config_filename, "rt");
	if (f != NULL) {
		char buf[300];
		while(fgets((char *)&buf, 300, f) != NULL) {
			printf("READ CONFIG LINE: %s\n", (char *)&buf);
		}
		fclose(f);
	}
}

void config_clear() {
	memset(configs, 0, sizeof(t_config) * MAXCONFIGS);
	num_configs = 0;
}

void config_save() {
	// save file
	FILE *f = fopen(config_filename, "wt");
	if (f != NULL) {
		for(int i=0; i<num_configs; i++) {
			t_config *ent = (t_config *)(configs + i);
			fprintf(f, "%s: %s\n", ent->key, ent->stringvalue);
		}
		fclose(f);
	}
}

void config_get_string(const char *key, char *destination, char *defaultvalue) {
	strcpy(destination, defaultvalue);

	int found = -1;
	for(int i=0; i<num_configs; i++) {
		t_config *ent = (t_config *)(configs + i);
		if (strcmp(ent->key, key) == 0) {
			strcpy(destination, ent->stringvalue);
		}
	}
}

void config_set_string(const char *key, char *value) {
	int found = -1;
	for(int i=0; i<num_configs; i++) {
		t_config *ent = (t_config *)configs + i;
		if (strcmp(ent->key, key) == 0) {
			found = i;
		}
	}
	if (found == -1) {
		// insert
		t_config *ent = (t_config *)(configs + num_configs);
		strcpy(ent->key, key);
		strcpy(ent->stringvalue, value);
		num_configs ++;
	} else {
		t_config *ent = (t_config *)configs + found;
		strcpy(ent->key, key);
		strcpy(ent->stringvalue, value);
	}
}

int config_get_int(const char *key, int defaultvalue) {
	int i = defaultvalue;
	char defbuf[50];
	char outbuf[50];
	sprintf(defbuf, "%d", defaultvalue);
	config_get_string(key, (char *)&outbuf, (char *)&defbuf);
	i = atoi(outbuf);
	return i;
}

void config_set_int(const char *key, int value) {
	char outbuf[50];
	sprintf(outbuf, "%d", value);
	config_set_string(key, (char *)&outbuf);
}
