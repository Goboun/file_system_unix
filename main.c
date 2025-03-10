#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//Librairies C (à renseigner dans le README.md)

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

//Ouvrir/Charger la partition DEJA CREE AU PREALABLE
int open_partition(const char *filename) {
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Erreur 117 : Impossible d'ouvrir la partition");
        return -1;
    }
    return fd;
}

//Créer un fichier
void create_file(file_entry **dir, const char *name) {
    file_entry *new_file = malloc(sizeof(file_entry)); //allocation mémoire avec la taille de la struct
    new_file->name = strdup(name);
    new_file->size = 0; //fichier vide par défaut
    new_file->permissions = 6; //lecture + écriture : 4+2
    new_file->is_directory = 0; //booléan, en gros si 0 c'est un fichier, si 1, un dossier
    new_file->next = *dir;
    *dir = new_file;
    printf("Fichier '%s' créé avec énorme succès.\n", name);
}

//Créer un répertoire
void create_directory(file_entry **dir, const char *name) {
    file_entry *new_dir = malloc(sizeof(file_entry));
    new_dir->name = strdup(name);
    new_dir->size = 0;
    new_dir->permissions = 7; //lecture + écriture + exécution : 4+2+1
    new_dir->is_directory = 1;
    new_dir->next = *dir;
    *dir = new_dir;
    printf("Répertoire '%s' créé.\n", name);
}

//Supprimer un fichier ou un répertoire
void remove_entry(file_entry **dir, const char *name) {
        file_entry *current = *dir;
        file_entry *prev = NULL;
        while (current) {
                if (strcmp(current->name, name) == 0) {
                        if(prev){ //check si le fichier prev existe bel et bien
                                prev->next = current->next;
                        }
                        else{
                                *dir = current->next;
                        }
                        free(current->name);
                        free(current);
                        printf("'%s' supprimé bien correctement.\n", name);
                        return; //quitter prématurément (à changer dans le futur)
                }
                prev = current;
                current = current->next;
        }
        printf("'%s' introuvable.\n", name);
}

//Lister les fichiers et répertoires comme un ptit ls en linux
void list_files(file_entry *dir) {
    if (!dir){
        printf("Le répertoire est vide.\n");
        return;
    }
    while (dir){
        printf("%s%s (permissions: %d)\n", dir->name, dir->is_directory ? "/" : "", dir->permissions);
        dir = dir->next;
    }
}

int main() {
    filesystem fs;
    fs.fd = open_partition("partition.fs"); //la partition est comme ça chez moi (.fs pour file system, veut rien dire en extension)
    if (fs.fd == -1) return 1;
    fs.root_dir = NULL;

    char command[256];
    while (1) {
        printf("fs> "); //simule le prompt de commande (cmd)
        if (!fgets(command, sizeof(command), stdin)) break; //cas où l'utilisateur ne rentre rien et command vaut NULL, on quitte avec break ( àvoir aussi)

        //Enlever le saut de ligne
        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "exit", 4) == 0) break;
        else if (strncmp(command, "ls", 2) == 0) list_files(fs.root_dir);
        else if (strncmp(command, "mkdir ", 6) == 0) create_directory(&fs.root_dir, command + 6);
        else if (strncmp(command, "touch ", 6) == 0) create_file(&fs.root_dir, command + 6);
        else if (strncmp(command, "rm ", 3) == 0) remove_entry(&fs.root_dir, command + 3);
        else printf("Commande non identifiée.\n");
    }

    //Lib de la mémoire
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
