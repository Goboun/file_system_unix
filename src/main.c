#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

//Structure pour un fichier ou répertoire
typedef struct file_entry {
    char *name;
    size_t size;
    int permissions;
    int is_directory;
    struct file_entry *next;
}file_entry;

//Structure du système de fichiers
typedef struct filesystem {
    int fd;
    size_t size;
    file_entry *root_dir;
}filesystem;

//Ouvrir une partition existante
int open_partition(const char *filename) {
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Erreur 117 : Impossible d'ouvrir la partition");
        return -1;
    }

    return fd;
}

//Créer un fichier
file_entry *create_file(file_entry *dir, const char *name) {
    file_entry *new_file = malloc(sizeof(file_entry));
    new_file->name = strdup(name);
    new_file->size = 0;
    new_file->permissions = 6;
    new_file->is_directory = 0;
    new_file->next = dir;
    printf("Fichier '%s' créé avec succès.\n", name);

    return new_file;
}

//Créer un répertoire
file_entry *create_directory(file_entry *dir, const char *name) {
    file_entry *new_dir = malloc(sizeof(file_entry));
    new_dir->name = strdup(name);
    new_dir->size = 0;
    new_dir->permissions = 7;
    new_dir->is_directory = 1;
    new_dir->next = dir;
    printf("Répertoire '%s' créé.\n", name);

    return new_dir;
}

//Supprimer un fichier ou un répertoire
file_entry *remove_entry(file_entry *dir, const char *name) {
    file_entry *current = dir;
    file_entry *prev = NULL;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                dir = current->next;
            }
            free(current->name);
            free(current);
            printf("'%s' supprimé.\n", name);

            return dir;
        }
        prev = current;
        current = current->next;
    }
    printf("'%s' introuvable.\n", name);

    return dir;
}

//Lister les fichiers et répertoires
void list_files(file_entry *dir) {
    if (!dir) {
        printf("Le répertoire est vide.\n");
    }
    while (dir) {
        printf("%s%s (permissions: %d)\n", dir->name, dir->is_directory ? "/" : "", dir->permissions);
        dir = dir->next;
    }
}

//Afficher l'aide
void display_help() {
    printf("Commandes disponibles :\n");
    printf("  ls            - Lister les fichiers et dossiers\n");
    printf("  rmdir <nom>   - Créer un répertoire\n");
    printf("  hedit <nom>   - Créer un fichier\n");
    printf("  rm <nom>      - Supprimer un fichier ou un répertoire\n");
    printf("  help          - Afficher cette aide\n");
    printf("  exit          - Quitter\n");
}

int main() {
    filesystem fs;
    fs.fd = open_partition("partition.fs");
    if (fs.fd == -1){
	return 1;
    }
    fs.root_dir = NULL;

    char command[256];

    while (1) {
        printf("hfs> ");
        if (!fgets(command, sizeof(command), stdin)) break;
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) break;
        else if (strcmp(command, "ls") == 0) list_files(fs.root_dir);
        else if (strncmp(command, "rmdir ", 6) == 0) fs.root_dir = create_directory(fs.root_dir, command + 6);
        else if (strncmp(command, "hedit ", 6) == 0) fs.root_dir = create_file(fs.root_dir, command + 6);
        else if (strncmp(command, "rm ", 3) == 0) fs.root_dir = remove_entry(fs.root_dir, command + 3);
        else if (strcmp(command, "help") == 0) display_help();
        else printf("Commande non reconnue. Tapez 'help' pour voir les commandes disponibles.\n");
    }

    file_entry *current = fs.root_dir;
    while (current) {
        file_entry *next = current->next;
        free(current->name);
        free(current);
        current = next;
    }

    close(fs.fd);
    return 0;
}

