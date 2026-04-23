#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include <dict.h>

struct dicts new_dicts(void) {
	struct dicts new_dicts = {
		0,
		malloc(sizeof(struct dict))
	};

	return new_dicts;
}

char *get_dict_value(const char *key, struct dicts *d) {
	
	for (int i = 0; i < d->count; i++) {
		struct dict dict = d->dicts[i];

		if (strcmp(dict.key, key) == 0 && strcmp(dict.value, "") != 0) {
			return d->dicts[i].value;
		}
	}

	return NULL;	
}

bool get_dict_bool(const char *key, struct dicts *d) {
	char *value = get_dict_value(key, d);
	return value != NULL && atoi(value);
}

void remove_dict_entry(const char *key, struct dicts *d) {
	bool move_back = false;
	
	for (int i = 0; i < d->count; i++) {
		struct dict dict = d->dicts[i];

		if (move_back) {
			d->dicts[i - 1] = d->dicts[i];
		}

		if (strcmp(dict.key, key) == 0) {
			move_back = true;
		}	
	}

	if (move_back) {
		d->count--;
		d->dicts = realloc(d->dicts, d->count * sizeof(struct dict));	
	}
}

void add_dict_entry(struct dicts *d, char *key, char *value, ...) {
	va_list args;
	char value_buffer[256];

    va_start(args, value);
    vsprintf(value_buffer, value, args);
    va_end(args);
	
	struct dict result;
	strlcpy(result.key, key, 256);
	strlcpy(result.value, value_buffer, 256);

	d->dicts = realloc(d->dicts, (d->count + 1) * sizeof(struct dict));
	d->dicts[d->count] = result;	
	d->count++;
}

bool parse_file(const char *path, struct dicts *d) {
	FILE *fp;
	ssize_t read;
	char key[256], value[256];

	char *line = NULL;
	size_t len = 0;

	if (access(path, F_OK) == -1)
		return false;

	fp = fopen(path, "r");

	while ((read = getline(&line, &len, fp)) != -1) {
		if (read <= 1)
			continue;

		char *quotation_mark_sub = strstr(line, "\"");
		char *equals_sub = strstr(line, "=");

		int len_for_key = -1;

		if (quotation_mark_sub != NULL)
			len_for_key = quotation_mark_sub - line;
		else if (equals_sub != NULL)
			len_for_key = equals_sub - line + 1;

		if (len_for_key == -1)
			continue;

		strlcpy(key, line, len_for_key);

		char *line_with_key_offset = line + len_for_key;
		int line_with_key_offset_len = strlen(line_with_key_offset);

		if (quotation_mark_sub != NULL) {
			line_with_key_offset++;
			line_with_key_offset_len -= 2;
		} 

		strlcpy(value, line_with_key_offset, line_with_key_offset_len);

		add_dict_entry(d, key, value);
	}

	fclose(fp);

	if (d->count == 0)
		return false;

	return true;
}

struct dicts get_config(void) {
	char conf_path[256];

	const char *home = getenv("HOME");

	snprintf(conf_path, sizeof(conf_path), "%s/.config/discord-fetch", home);

	struct dicts conf = new_dicts();

	if (!parse_file(conf_path, &conf)) {
		FILE *fp;

		add_dict_entry(&conf, "OS_NAME", "");
		add_dict_entry(&conf, "CPU_ARCH", "1");
		add_dict_entry(&conf, "OS_IMAGE", "");
		add_dict_entry(&conf, "ALT_IMAGE", "0");
		add_dict_entry(&conf, "OS_DETAILS", "DESKTOP SHELL TERM KERN");
		add_dict_entry(&conf, "CUSTOM_DESC", "");
		add_dict_entry(&conf, "SEPARATOR", " | ");
		add_dict_entry(&conf, "DISPLAY_MEDIA", "1");
		add_dict_entry(&conf, "FILTER_MEDIA_APPLICATIONS", "");
	
		fp = fopen(conf_path, "w");

		for (int i = 0; i < conf.count; i++) {
			char line[1024];

			struct dict entry = conf.dicts[i];

			snprintf(line, sizeof(line), "%s=\"%s\"\n", entry.key, entry.value);

			fprintf(fp, "%s", line);
		}

		fclose(fp);
	}
	
	return conf;
}

void clear_dicts(struct dicts *d) {
	free(d->dicts);
	d->count = 0;
}
