struct dict {
	char key[256];
	char value[256];
};

struct dicts {
	int count;
	struct dict *dicts;
};
