#include <time.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <rpc.h>
#include <data.h>
#include <dict.h>
#include <dict_util.h>
#include <pipewire/pipewire.h> 

typedef void (*PFNPWINIT)(int *argc, char **argv[]);
typedef struct pw_main_loop *(*PFNPWMAINLOOPNEW)(const struct spa_dict *props);
typedef struct pw_loop *(*PFNPWMAINLOOPGETLOOP)(struct pw_main_loop *loop);
typedef struct pw_context *(*PFNPWCONTEXTNEW)(struct pw_loop *main_loop, struct pw_properties *properties, size_t user_data_size);
typedef struct pw_core *(*PFNPWCONTEXTCONNECT)(struct pw_context *context, struct pw_properties *properties, size_t user_data_size);
typedef struct pw_registry *(*PFNPWCOREGETREGISTRY)(struct pw_core *core, uint32_t version, size_t user_data_size);
typedef int (*PFNPWREGISTRYADDLISTENER)(struct pw_registry *registry, struct spa_hook *listener, const struct pw_registry_events *events, void *data);
typedef int (*PFNPWMAINLOOPRUN)(struct pw_main_loop *loop); 	
typedef void (*PFNPWPROXYDESTROY)(struct pw_proxy *proxy); 	
typedef int (*PFNPWCOREDISCONNECT)(struct pw_core *core);
typedef void (*PFNPWCONTEXTDESTROY)(struct pw_context *context); 	
typedef void (*PFNPWMAINLOOPDESTROY)(struct pw_main_loop *loop);
typedef const char *(*PFNPWNODESTATEASSTRING)(enum pw_node_state state); 	

static PFNPWINIT pw_init_ptr = NULL;
static PFNPWMAINLOOPNEW pw_main_loop_new_ptr = NULL;
static PFNPWMAINLOOPGETLOOP pw_main_loop_get_loop_ptr = NULL;
static PFNPWCONTEXTNEW pw_context_new_ptr = NULL;
static PFNPWCONTEXTCONNECT pw_context_connect_ptr = NULL;
static PFNPWCOREGETREGISTRY pw_core_get_registry_ptr = NULL;
static PFNPWREGISTRYADDLISTENER pw_registry_add_listener_ptr = NULL;
static PFNPWMAINLOOPRUN pw_main_loop_run_ptr = NULL;
static PFNPWPROXYDESTROY pw_proxy_destroy_ptr = NULL;
static PFNPWCOREDISCONNECT pw_core_disconnect_ptr = NULL;
static PFNPWCONTEXTDESTROY pw_context_destroy_ptr = NULL;
static PFNPWMAINLOOPDESTROY pw_main_loop_destroy_ptr = NULL;
static PFNPWNODESTATEASSTRING pw_node_state_as_string_ptr = NULL;

static time_t prev_time = 0;

static const char *pw_libs[] = { "libpipewire-0.3.so", "libpipewire-0.3.so.0"};

extern struct dicts config;
extern struct dicts os_details;

const char *substrings_to_remove[] = {
	" - mpv",
	" - YouTube",	
};

struct dicts medias, media_states;

static void link_event_info(void *data, const struct pw_link_info *info) {
	if (info == NULL)
		return;

	char id_buff[8];
	snprintf(id_buff, sizeof(id_buff), "%d", info->output_node_id);

	char state_buff[8];
	snprintf(state_buff, sizeof(state_buff), "%d", info->state);

	if (get_dict_value(id_buff, &media_states) != NULL) {
		remove_dict_entry(id_buff, &media_states);
	}

	add_dict_entry(&media_states, id_buff, state_buff);
}

static const struct pw_link_events link_events = {
	PW_VERSION_LINK_EVENTS,
	.info = link_event_info
};

static void node_event_info(void *data, const struct pw_node_info *info) {
	if (info == NULL)
		return;

	if (info->props == NULL)
		return;

	const char *app_name = spa_dict_lookup(info->props, PW_KEY_APP_NAME);

	if (app_name == NULL)
		return;

	char *filter_apps = get_dict_value("FILTER_MEDIA_APPLICATIONS", &config);

	if (filter_apps != NULL && strcasestr(filter_apps, app_name) == NULL)
		return;

	const char *media_name = spa_dict_lookup(info->props, PW_KEY_MEDIA_NAME);

	if (media_name == NULL || strcmp(media_name, "AudioStream") == 0)
		return;

	int substring_index = -1;

	for (int i = 0; i < sizeof(substrings_to_remove) / sizeof(substrings_to_remove[0]); i++) {
		const char *strstr_ret = strstr(media_name, substrings_to_remove[i]);
		if (strstr_ret != NULL) {
			substring_index = strstr_ret - media_name;
		}
	}

	char filtered_media_name[256];

	if (substring_index != -1)
		strlcpy(filtered_media_name, media_name, substring_index + 1);
	else
		strlcpy(filtered_media_name, media_name, 256);

	for (int i = strlen(filtered_media_name); i > 0; i--) {
		if (filtered_media_name[i] == '.') {
			filtered_media_name[i] = '\0';
			break;
		}
	}

	char id_buff[8];
	snprintf(id_buff, sizeof(id_buff), "%d", info->id);

	if (get_dict_value(id_buff, &medias) != NULL)
		remove_dict_entry(id_buff, &medias);

	add_dict_entry(&medias, id_buff, filtered_media_name);
}

static const struct pw_node_events node_events = {
	PW_VERSION_NODE_EVENTS,
	.info = node_event_info
};

static void registry_event_global(void *data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props) {
	if (props == NULL)
		return;

	struct pw_registry *registry = (struct pw_registry*)data;

	if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {

		const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
		if (media_class == NULL)
			return;

		if (strcmp(media_class, "Stream/Output/Audio") != 0)
	       		return;

		struct pw_node *node = pw_registry_bind(registry, id, type, version, 0);
		struct spa_hook *hook = malloc(sizeof(struct spa_hook));
		pw_node_add_listener(node, hook, &node_events, NULL);
	} else if (strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
		struct pw_link *link = pw_registry_bind(registry, id, type, version, 0);
	       	struct spa_hook *hook = malloc(sizeof(struct spa_hook));
		pw_link_add_listener(link, hook, &link_events, NULL);
	}
	
}

static const struct pw_registry_events registry_events = {
        PW_VERSION_REGISTRY_EVENTS,
        .global = registry_event_global
};


void idle_loop(void *data) {
	time_t cur_time = time(NULL);

	if (cur_time - prev_time >= 5) {
		for (int i = 0; i < media_states.count; i++) {
			if (atoi(media_states.dicts[i].value) == PW_LINK_STATE_ACTIVE) {
				char *media = get_dict_value(media_states.dicts[i].key, &medias);

				if (media != NULL) {
					set_activity("Listening to music", media, get_dict_value("OS_IMAGE", &os_details));
					prev_time = cur_time;
					return;
				}
			}
		}

		set_os_activity();

		prev_time = cur_time;
	}
}

void *get_pw_handle(void) {
	for (size_t i = 0; i < sizeof(pw_libs) / sizeof(pw_libs[0]); i++) {
       		void *pw_handle = dlopen(pw_libs[i], RTLD_LAZY);
		
		if (pw_handle != NULL)
			return pw_handle;
	}

	return NULL;
}

bool find_pw_symbols(void) {
	void *pw_handle = get_pw_handle();
	
	if (pw_handle == NULL)
		return false;

	pw_init_ptr = dlsym(pw_handle, "pw_init");
	pw_main_loop_new_ptr = dlsym(pw_handle, "pw_main_loop_new");
	pw_context_new_ptr = dlsym(pw_handle, "pw_context_new");
	pw_main_loop_get_loop_ptr = dlsym(pw_handle, "pw_main_loop_get_loop");
	pw_context_connect_ptr = dlsym(pw_handle, "pw_context_connect");
	pw_core_get_registry_ptr = dlsym(pw_handle, "pw_core_get_registry");
	pw_registry_add_listener_ptr = dlsym(pw_handle, "pw_registry_add_listener");
	pw_main_loop_run_ptr = dlsym(pw_handle, "pw_main_loop_run");
	pw_proxy_destroy_ptr = dlsym(pw_handle, "pw_proxy_destroy");
	pw_core_disconnect_ptr = dlsym(pw_handle, "pw_core_disconnect");
	pw_context_destroy_ptr = dlsym(pw_handle, "pw_context_destroy");
	pw_main_loop_destroy_ptr = dlsym(pw_handle, "pw_main_loop_destroy");
	pw_node_state_as_string_ptr = dlsym(pw_handle, "pw_node_state_as_string");

	return true;
}

bool init_audio(int argc, char *argv[]) {
        struct pw_main_loop *loop;
        struct pw_context *context;
        struct pw_core *core;
        struct pw_registry *registry;
        struct spa_hook registry_listener;

	medias = new_dicts();
	media_states = new_dicts();

	prev_time = time(NULL);

	if (!find_pw_symbols())
		return false;

        pw_init_ptr(&argc, &argv);
 
        loop = pw_main_loop_new_ptr(NULL /* properties */);
        context = pw_context_new_ptr(pw_main_loop_get_loop_ptr(loop),
                        NULL /* properties */,
                        0 /* user_data size */);
 
        core = pw_context_connect_ptr(context,
                        NULL,
                        0 /* user_data size */);
 
        registry = pw_core_get_registry_ptr(core, PW_VERSION_REGISTRY,
                        0 /* user_data size */);
 
        spa_zero(registry_listener);

	pw_registry_add_listener_ptr(registry, &registry_listener,
                                       &registry_events, registry);
	
	pw_loop_add_idle(pw_main_loop_get_loop_ptr(loop), true, idle_loop, NULL);

        pw_main_loop_run_ptr(loop);
 
        pw_proxy_destroy_ptr((struct pw_proxy*)registry);
        pw_core_disconnect_ptr(core);
        pw_context_destroy_ptr(context);
        pw_main_loop_destroy_ptr(loop);

	return true;
}
