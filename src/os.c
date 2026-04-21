#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/utsname.h>

#include <rpc.h>
#include <dict.h>
#include <dict_util.h>

extern struct dicts config;
extern struct dicts os_details;

void set_os_activity(void) {
	char *os_name = get_dict_value("OS_NAME", &os_details);
	char *image = get_dict_value("OS_IMAGE", &os_details);
	char *separator_conf = get_dict_value("SEPARATOR", &config);

	char *separator = !separator_conf ? " | " : separator_conf;

	char state[1024] = "";	

	for (int i = 2; i < os_details.count; i++) {
		if (i != 2)
			strcat(state, separator);
		
		strcat(state, os_details.dicts[i].value);
	}

	char *custom_desc = get_dict_value("CUSTOM_DESC", &config);

	set_activity(os_name, !custom_desc ? state : custom_desc, image);	
}

char *get_os_name(struct utsname *utsname_data, struct dicts *os_release) {
	char *os_name_conf = get_dict_value("OS_NAME", &config);

	if (os_name_conf)
		return os_name_conf;

	char *os_name_release = get_dict_value("PRETTY_NAME", os_release);
	
	if (os_name_release)
		return os_name_release;

	if (strcmp(utsname_data->sysname, "Darwin") == 0)
		return "macOS";

	return utsname_data->sysname;
}

char *get_image(struct utsname *utsname_data, struct dicts *os_release) {
	const char *valid_os_images = "void arch gentoo debian ubuntu fedora nixos macos";

	char *os_image_conf = get_dict_value("OS_IMAGE", &config);

	if (os_image_conf)
		return os_image_conf;

	char *id = get_dict_value("ID", os_release);
	
	if (id != NULL && strstr(valid_os_images, id) != NULL)
		return id;
	
	char *id_like = get_dict_value("ID_LIKE", os_release);

	if (id_like != NULL && strstr(valid_os_images, id_like) != NULL)
		return id_like;
	
	if (strcmp(utsname_data->sysname, "Darwin") == 0)
		return "macos";

	return "default";	
}

struct dicts get_os_details(void) {
	os_details = new_dicts();
	struct dicts os_release = new_dicts();

	struct utsname utsname_data;

	uname(&utsname_data);

	parse_file("/etc/os-release", &os_release);
	
	char *os_name = get_os_name(&utsname_data, &os_release);
	bool add_arch = get_dict_bool("CPU_ARCH", &config);
	add_dict_entry(&os_details, "OS_NAME", "%s %s", os_name, add_arch ? utsname_data.machine : "");

	char *os_image = get_image(&utsname_data, &os_release);
	bool alt_image = get_dict_bool("ALT_IMAGE", &config);
	add_dict_entry(&os_details, "OS_IMAGE", "%s%s", os_image, alt_image ? "alt" : "");

	char *os_details_conf = get_dict_value("OS_DETAILS", &config);

	if (os_details_conf == NULL)
		return os_details;

	if (strstr(os_details_conf, "DESKTOP") != NULL ) {
		char *desktop = getenv("XDG_CURRENT_DESKTOP");
		
		if (desktop != NULL)
			add_dict_entry(&os_details, "DESKTOP", desktop);
	}

	if (strstr(os_details_conf, "TERM") != NULL) {
		char *term = getenv("TERM");

		if (term != NULL)
			add_dict_entry(&os_details, "TERM", term);
	}

	if (strstr(os_details_conf, "SHELL") != NULL) {
		char *shell = getenv("SHELL");

		if (shell != NULL) {

			int strip_index = -1;

			for (int i = 0; i < strlen(shell); i++) {
				if (shell[i] == '/')
					strip_index = i + 1;
			}

			if (strip_index != -1)
				shell += strip_index;
			
			add_dict_entry(&os_details, "SHELL", shell);
		}
	}

	if (strstr(os_details_conf, "KERN") != NULL) {
		add_dict_entry(&os_details, "KERN", "%s %s", utsname_data.sysname, utsname_data.release);
	}


	return os_details;
}
