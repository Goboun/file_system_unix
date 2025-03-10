#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "structures.h"
#include "fonctions.h"

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

//Afficher l'aide dans l'invite de commandes
void display_help(){
    printf("Commandes à disposition :\n");
    printf(" ls             - Lister les fichiers et dossiers\n");
    printf(" mkdir <nom>    - Créer un répertoire\n");
    printf(" touch <nom>    - Créer un fichier\n");
    printf(" rm <nom>       - Supprimer un fichier ou un répertoire\n");
    printf(" help           - Afficher l'aide aux commandes\n");
    printf(" exit           - Quitter l'invite de commandes\n");
}