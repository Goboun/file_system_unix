#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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
