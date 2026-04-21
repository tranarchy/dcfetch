struct dicts new_dicts(void);
void remove_dict_entry(const char *key, struct dicts *d);
void add_dict_entry(struct dicts *d, char *key, char *value, ...);
bool get_dict_bool(const char *key, struct dicts *d);
char *get_dict_value(const char *key, struct dicts *d);
bool parse_file(const char *path, struct dicts *d);
struct dicts get_config(void);
void clear_dicts(struct dicts *d);
