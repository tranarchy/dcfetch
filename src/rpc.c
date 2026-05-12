#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/un.h>
#include <sys/socket.h>

static const char *env_vars[] = { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" };
static const char *subpaths[] = {
    "",
    "/app/com.discordapp.Discord",
    "/.flatpak/com.discordapp.Discord/xdg-run"
};

static const char *APP_ID = "1494653358454214726";

static int fd;

static void get_ipc_path(char **ipc_path) {
	for (size_t i = 0; i < sizeof(env_vars) / sizeof(env_vars[0]); i++) {
		const char *path = getenv(env_vars[i]);

		if (path == NULL)
                    continue;

              
    for (size_t s = 0; s < sizeof(subpaths) / sizeof(subpaths[0]); s++) {
                    for (int j = 0; j < 10; j++) {
                        char ipc_path_buff[1024];
                        snprintf(ipc_path_buff, sizeof(ipc_path_buff), "%s%s/discord-ipc-%d", path, subpaths[s], j);

                        if (access(ipc_path_buff, F_OK) == -1)
                            continue;

                        *ipc_path = strdup(ipc_path_buff);
                        return;
                    }
                }
	}
}

static char *get_random_uuid() {
    	char v[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    
   	static char buf[37] = {0};

    	for(int i = 0; i < 36; ++i) {
        	buf[i] = v[rand()%16];
    	}

    	buf[8] = '-';
    	buf[13] = '-';
    	buf[18] = '-';
    	buf[23] = '-';

    
    	buf[36] = '\0';

    	return buf;
}

void close_socket_and_exit(int status) {
	close(fd);
	printf("Closed socket!\n");
	exit(status);
}

static bool write_buffer_to_socket(int32_t opcode, char *msg) {
	int ret;
	char buffer_to_recv[1024];

	size_t len = strlen(msg);

	char *buffer_to_send = (char *)malloc((len * sizeof(char)) + 8);

	buffer_to_send[3] = (opcode >> 24) & 0xFF;
	buffer_to_send[2] = (opcode >> 16) & 0xFF;
	buffer_to_send[1] = (opcode >> 8) & 0xFF;
	buffer_to_send[0] = opcode & 0xFF;

	buffer_to_send[7] = (len >> 24) & 0xFF;
	buffer_to_send[6] = (len >> 16) & 0xFF;
	buffer_to_send[5] = (len >> 8) & 0xFF;
	buffer_to_send[4] = len & 0xFF;
	
	memcpy(buffer_to_send + 8, msg, len);	
	
	ret = send(fd, buffer_to_send, len + 8, MSG_NOSIGNAL);

	free(buffer_to_send);
	
	if (ret < 0)
		return false;

	len = recv(fd, buffer_to_recv, 1024, MSG_NOSIGNAL);

	if (len < 0)
		return false;

	return true;
}

static bool perform_handshake(void) {
	char json_buff[64];

	snprintf(json_buff, sizeof(json_buff), "{\"v\":1,\"client_id\":\"%s\"}", APP_ID);
	
	if (!write_buffer_to_socket(0, json_buff))
		close_socket_and_exit(1);

	printf("Sent handshake!\n");

	return true;
}

bool set_activity(char *details, char *state, char *image) {
     char json_buff[512];
     
     char *UUID = get_random_uuid();
     pid_t pid = getpid();

     snprintf(
		json_buff, sizeof(json_buff), 
		"{\"nonce\":\"%s\",\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d,\"activity\":{\"details\":\"%s\", \"state\": \"%s\", \"assets\": {\"large_image\":\"%s\", \"large_text\":\"%s\" }}}}",
		 UUID, pid, details, state, image, details
     );

     if (!write_buffer_to_socket(1, json_buff))
     	close_socket_and_exit(1);

     return true;
}

bool open_socket(void) {
	int ret;
	struct sockaddr_un addr;

	srand(time(NULL));

	char *ipc_path = NULL;
	get_ipc_path(&ipc_path);

	if (ipc_path == NULL)
		return false;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
		return false;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, ipc_path);
	ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
	free(ipc_path);

	if (ret == -1)
		return false;

	printf("Connected to Discord!\n");
	
	perform_handshake();

	return true;
} 
