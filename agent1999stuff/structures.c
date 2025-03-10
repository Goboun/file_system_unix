//La structure de donnée d'un fichier et du système de gestion de fichier (un tableau de fichier)
typedef struct file_entry {
    char *name;
    size_t size;
    int permissions; //lecture (4), écriture (2), exécution (1)
    int is_directory;
    struct file_entry *next;
} file_entry;

typedef struct filesystem {
    int fd;
    size_t size;
    file_entry *root_dir;
} filesystem;
