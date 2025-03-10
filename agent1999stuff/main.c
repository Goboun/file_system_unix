#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//Librairies C (à renseigner dans le README.md)

#include "structures.h"
#include "fonctions.h"

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
