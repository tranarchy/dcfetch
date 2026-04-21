#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>

#include <os.h>
#include <rpc.h>
#include <audio.h>
#include <dict.h>
#include <dict_util.h>

struct dicts os_details;
struct dicts config;

void sigint_handler(int sig_num) {
	signal(SIGINT, sigint_handler);
	clear_dicts(&os_details);
	clear_dicts(&config);
	close_socket_and_exit(0);
}

int main (int argc, char *argv[]) {
	signal(SIGINT, sigint_handler);

	config = get_config();
	os_details = get_os_details();

	if (!open_socket())
		close_socket_and_exit(1);
	
	if (get_dict_bool("DISPLAY_MEDIA", &config)) {
		if (!init_audio(argc, argv))
			puts("Failed to init audio");
	}
	
	set_os_activity();

	for(;;) {}
	
	return 0;
}
