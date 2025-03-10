int open_partition(const char *filename);

void create_file(file_entry **dir, const char *name);

void create_directory(file_entry **dir, const char *name);

void remove_entry(file_entry **dir, const char *name);

void list_files(file_entry *dir);

void display_help();