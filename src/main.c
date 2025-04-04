/**
 * @file main.c
 * @brief Systeme de fichiers simple en C.
 *
 * Ce programme simule un systeme de fichiers en memoire.
 * Il supporte les commandes de base suivantes :
 *   mkfs, read, write, lseek, mkdir, rmdir, cd, pwd, ls, ls -l,
 *   cat, create, chmod, link, ln, unlink, rm, mv, fsck, tree, help et exit.
 *
 * Les liens physiques partagent le meme inode, tandis que les liens symboliques
 * en reçoivent un nouveau et conservent un pointeur sur l’original.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* --- Structures --- */

typedef struct FileEntry {
    int inode;
    int is_symbol;            // 1 si lien symbolique, 0 sinon
    struct FileEntry *origin; // Pointeur vers l'entree d'origine pour les liens symboliques
    char* nom_origin;
    char *name;
    int is_directory;         // 1 si repertoire, 0 si fichier
    int size;                 // Taille en octets (pour fichiers)
    char *content;            // Contenu (pour fichiers, NULL pour repertoires)
    int link_count;           // Nombre de liens physiques
    int perms;                // 4 = lecture, 2 = ecriture, 1 = execution
    struct FileEntry *child;  // Premier enfant (pour repertoires)
    struct FileEntry *next;   // Element suivant dans le meme repertoire
    struct FileEntry *parent; // Repertoire parent (NULL pour la racine)
} FileEntry;

typedef struct FileSystem {
    FileEntry *root;    // Racine du systeme de fichiers
    FileEntry *current; // Repertoire courant
} FileSystem;

typedef struct OpenFile {
    int fd;             
    FileEntry *file;    
    int flags;          // 1 = lecture, 2 = ecriture, 3 = lecture/ecriture
    int offset;         
    struct OpenFile *next;
} OpenFile;

/* --- Variables globales --- */
FileSystem fs = { NULL, NULL };
OpenFile *open_files = NULL;
int next_inode = 1;
int next_fd = 3; // Descripteurs reserves pour stdio
const int DEFAULT_FILE_SIZE = 100; // Taille par defaut d'un fichier

/* --- Fonctions utilitaires --- */

void free_file_entry(FileEntry *entry) {
    if (!entry)
        return;
    if (entry->is_directory) {
        FileEntry *child = entry->child;
        while (child) {
            FileEntry *suivant = child->next;
            free_file_entry(child);
            child = suivant;
        }
    }
    free(entry->name);
    if (entry->content)
        free(entry->content);
    free(entry);
}

FileEntry* find_entry(FileEntry *dir, const char *name) {
    if (!dir || !dir->is_directory)
        return NULL;
    FileEntry *child = dir->child;
    while (child) {
        if (strcmp(child->name, name) == 0)
            return child;
        child = child->next;
    }
    return NULL;
}

void add_entry(FileEntry *dir, FileEntry *entry) {
    if (!dir || !dir->is_directory)
        return;
    entry->next = dir->child;
    dir->child = entry;
    entry->parent = dir;
}

char *build_path(FileEntry *entry) {
    if (!entry->parent) {
        char *chemin = malloc(2);
        strcpy(chemin, "/");
        return chemin;
    }
    char *chemin_parent = build_path(entry->parent);
    int len = strlen(chemin_parent) + strlen(entry->name) + 2;
    char *chemin_complet = malloc(len);
    if (strcmp(chemin_parent, "/") == 0)
        snprintf(chemin_complet, len, "/%s", entry->name);
    else
        snprintf(chemin_complet, len, "%s/%s", chemin_parent, entry->name);
    free(chemin_parent);
    return chemin_complet;
}

FileEntry* resolve_path(const char *path, FileEntry **parentOut) {
    FileEntry *courant = (path[0]=='/') ? fs.root : fs.current;
    char *copie = strdup(path);
    char *token = strtok(copie, "/");
    FileEntry *parent = NULL;
    while (token) {
        parent = courant;
        courant = find_entry(courant, token);
        if (!courant) {
            free(copie);
            if (parentOut)
                *parentOut = parent;
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    free(copie);
    if (parentOut)
        *parentOut = parent;
    return courant;
}

void print_tree(FileEntry *entry, int level, int show_inodes) {
    if (!entry)
        return;
    for (int i = 0; i < level; i++) {
        printf("    ");
    }
    if (show_inodes)
        printf("[%d] ", entry->inode);
    printf("%s", entry->name);
    if (entry->is_directory)
        printf("/");
    printf("\n");
    if (entry->is_directory) {
        FileEntry *child = entry->child;
        while (child) {
            print_tree(child, level + 1, show_inodes);
            child = child->next;
        }
    }
}

/**
 * @brief Affiche l'arborescence d'un dossier.
 *
 * Si aucun argument n'est fourni, alors arborescence du repertoire courant.
 * Sinon, le chemin donne est resolu et son arborescence est affiche.
 *
 * @param arg Chemin optionnel du repertoire à afficher.
 */
void fs_tree(const char *arg, int indentation) {
	//Définir répertoire
    FileEntry *cible = NULL;
    if (arg == NULL) {
        cible = fs.current;
    } else {
        cible = resolve_path(arg, NULL);
        if (!cible) {
            printf("Repertoire introuvable : %s\n", arg);
            return;
        }
        if (!cible->is_directory) {
            printf("%s\n", cible->name);
            return;
        }
    }
    //Indentation
    int i;
    for(i = 0; i<indentation; i++){
		printf("    ");
	}
	//Afficher nom du dossier
	printf("\033[1;34m%s\033[0m\n", cible->name);

    //Afficher nom des sous éléments
    FileEntry *child = cible->child;
    while (child) {
		//Lien symbolique (pas de récursion pour les dossiers symboliques)
		if(child->is_symbol){
			//Indentation + 1
			for(i = 0; i<=indentation; i++){
				printf("    ");
			}
			printf("\033[1;36m%s -> %s\033[0m\n", child->name, child->nom_origin);
		}
		else{
			//Dossier = appel récursif
			if(child->is_directory){
				//MAJ du dossier courant, sinon ça bug !!!
				fs.current = cible;
				fs_tree(child->name, indentation+1);
			}
			//Fichier
			else{
				//Indentation + 1
				for(i = 0; i<=indentation; i++){
					printf("    ");
				}
				printf("\033[1;32m%s\033[0m\n", child->name);
			}
		}
        child = child->next;
    }
    //Dossier courant remis par défaut
    fs.current = cible;
}

/**
 * @brief Affiche l'arborescence d'un dossier avec les inodes.
 *
 * Si aucun argument n'est fourni, alors arborescence du repertoire courant.
 * Sinon, le chemin donne est resolu et son arborescence est affiche.
 *
 * @param arg Chemin optionnel du repertoire à afficher.
 */
void fs_tree_i(const char *arg, int indentation) {
	//Définir répertoire
    FileEntry *cible = NULL;
    if (arg == NULL) {
        cible = fs.current;
    } else {
        cible = resolve_path(arg, NULL);
        if (!cible) {
            printf("Repertoire introuvable : %s\n", arg);
            return;
        }
        if (!cible->is_directory) {
            printf("%d %s\n", cible->inode, cible->name);
            return;
        }
    }
    
    //Indentation
    int i;
    for(i = 0; i<indentation; i++){
		printf("    ");
	}
	//Afficher nom du dossier
	printf("%d \033[1;34m%s\033[0m\n", cible->inode, cible->name);

    //Afficher nom des sous éléments
    FileEntry *child = cible->child;
    while (child) {
        //Lien symbolique (pas de récursion pour les dossiers symboliques)
		if(child->is_symbol == 1){
			//Indentation + 1
			for(i = 0; i<=indentation; i++){
				printf("    ");
			}
			printf("%d \033[1;36m%s -> %s\033[0m\n", child->inode, child->name, child->nom_origin);
		}
		else if(child->is_symbol == 2){
			//Indentation + 1
			for(i = 0; i<=indentation; i++){
				printf("    ");
			}
			printf("%d \033[1;31m%s -> %s\033[0m\n", child->inode, child->name, child->nom_origin);
		}
		else{
			//Dossier = appel récursif
			if(child->is_directory){
				//MAJ du dossier courant, sinon ça bug !!!
				fs.current = cible;
				fs_tree_i(child->name, indentation+1);
			}
			//Fichier
			else{
				//Indentation + 1
				for(i = 0; i<=indentation; i++){
					printf("    ");
				}
				printf("%d \033[1;32m%s\033[0m\n", child->inode, child->name);
			}
		}
		child = child->next;
    }
    //Dossier courant remis par défaut
    fs.current = cible;
}

void get_perms_text(int perms, char *buf, size_t buf_size) {
    buf[0] = '\0';
    int appended = 0;
    if (perms & 4) {
        strncat(buf, "read", buf_size - strlen(buf) - 1);
        appended = 1;
    }
    if (perms & 2) {
        if (appended) strncat(buf, ", ", buf_size - strlen(buf) - 1);
        strncat(buf, "write", buf_size - strlen(buf) - 1);
        appended = 1;
    }
    if (perms & 1) {
        if (appended) strncat(buf, ", ", buf_size - strlen(buf) - 1);
        strncat(buf, "execute", buf_size - strlen(buf) - 1);
    }
    if (strlen(buf) == 0) {
        strncpy(buf, "none", buf_size - 1);
        buf[buf_size - 1] = '\0';
    }
}

/* --- Fonctions backend (non accessibles directement par l'utilisateur) --- */

void mkfs() {
    if (fs.root)
        free_file_entry(fs.root);
    fs.root = malloc(sizeof(FileEntry));
    fs.root->inode = next_inode++;
    fs.root->is_symbol = 0;
    fs.root->origin = NULL;
    fs.root->name = strdup("/");
    fs.root->is_directory = 1;
    fs.root->size = 0;
    fs.root->content = NULL;
    fs.root->link_count = 1;
    fs.root->perms = 7; // rwx
    fs.root->child = NULL;
    fs.root->next = NULL;
    fs.root->parent = NULL;
    fs.current = fs.root;
    while (open_files) {
        OpenFile *tmp = open_files;
        open_files = open_files->next;
        free(tmp);
    }
    next_fd = 3;
    printf("Systeme de fichiers formate.\n");
}

int fs_open(const char *filename, int flag) {
    FileEntry *entry = find_entry(fs.current, filename);
    if (!entry) {
        // Ne cree pas le fichier ici; il doit être créé via fs_touch
        printf("Fichier introuvable.\n");
        return -1;
    } else if (entry->is_directory) {
        printf("Impossible d'ouvrir un repertoire.\n");
        return -1;
    }

    // Vérification des permissions
    if (flag == 1 || flag == 3) {  // Lecture
        if (!(entry->perms & 4)) {
            printf("Permission refusee : lecture interdite.\n");
            return -1;
        }
    }
    if (flag == 2 || flag == 3) {  // Ecriture
        if (!(entry->perms & 2)) {
            printf("Permission refusee : ecriture interdite.\n");
            return -1;
        }
    }

    OpenFile *of = malloc(sizeof(OpenFile));
    of->fd = next_fd++;
    of->file = entry;
    of->flags = flag;
    of->offset = 0;
    of->next = open_files;
    open_files = of;
    return of->fd;
}

ssize_t fs_write(int fd, const char *data) {
    OpenFile *of = open_files;
    while (of) {
        if (of->fd == fd)
            break;
        of = of->next;
    }
    if (!of) {
        printf("Descripteur invalide.\n");
        return -1;
    }
    if (!(of->flags == 2 || of->flags == 3)) {
        printf("Fichier non ouvert en ecriture.\n");
        return -1;
    }
    if (!(of->file->perms & 2)) {
        printf("Permission refusee : ecriture interdite.\n");
        return -1;
    }
    FileEntry *file = of->file;
    int data_len = strlen(data);
    int new_size = of->offset + data_len;
    if (new_size > file->size) {
        file->content = realloc(file->content, new_size + 1);
        memset(file->content + file->size, 0, new_size - file->size);
        file->size = new_size;
    }
    memcpy(file->content + of->offset, data, data_len);
    of->offset += data_len;
    file->content[file->size] = '\0';
    return data_len;
}

off_t fs_lseek(int fd, int offset) {
    OpenFile *of = open_files;
    while (of) {
        if (of->fd == fd)
            break;
        of = of->next;
    }
    if (!of) {
        printf("Descripteur invalide.\n");
        return -1;
    }
    if (offset < 0 || offset > of->file->size) {
        printf("Offset invalide.\n");
        return -1;
    }
    of->offset = offset;
    return offset;
}

int fs_close(int fd) {
    OpenFile **prev = &open_files;
    OpenFile *of = open_files;
    while (of) {
        if (of->fd == fd) {
            *prev = of->next;
            free(of);
            return 0;
        }
        prev = &of->next;
        of = of->next;
    }
    printf("Descripteur invalide.\n");
    return -1;
}

/* --- Fonctions pour manipuler le systeme de fichiers via l'interface utilisateur --- */

void fs_mkdir(const char *dirname) {
    if (find_entry(fs.current, dirname)) {
        printf("Un repertoire ou fichier portant ce nom existe deja.\n");
        return;
    }
    FileEntry *dir = malloc(sizeof(FileEntry));
    dir->inode = next_inode++;
    dir->is_symbol = 0;
    dir->origin = NULL;
    dir->name = strdup(dirname);
    dir->is_directory = 1;
    dir->size = 0;
    dir->content = NULL;
    dir->link_count = 1;
    dir->perms = 7; // rwx par defaut
    dir->child = NULL;
    dir->next = NULL;
    add_entry(fs.current, dir);
    printf("Repertoire '%s' cree.\n", dirname);
}

void fs_rmdir(const char *dirname) {
    FileEntry *dir = resolve_path(dirname, NULL);
    if (!dir || !dir->is_directory) {
        printf("Repertoire introuvable.\n");
        return;
    }
    if (dir->child != NULL) {
        printf("Le repertoire n'est pas vide.\n");
        return;
    }
    if (!dir->parent) {
        printf("Impossible de supprimer la racine.\n");
        return;
    }
    FileEntry **courant = &dir->parent->child;
    while (*courant) {
        if (*courant == dir) {
            *courant = dir->next;
            free(dir->name);
            free(dir);
            printf("Repertoire '%s' supprime.\n", dirname);
            return;
        }
        courant = &(*courant)->next;
    }
}

void fs_cd(const char *dirname) {
    FileEntry* copie = fs.current;
    if (strcmp(dirname, "..") == 0) {
        if (fs.current->parent)
            fs.current = fs.current->parent;
        else
            fs.current = fs.root;
        char *chemin = build_path(fs.current);
        printf("Repertoire courant change vers '%s'.\n", chemin);
        free(chemin);
        return;
    }
    FileEntry *dir = resolve_path(dirname, NULL);
    if (!dir || !dir->is_directory) {
        printf("Repertoire introuvable.\n");
        return;
    }
    
    if(dir->is_symbol){
		fs.current = dir->origin;
		if (resolve_path(dir->origin->name, NULL) == NULL){
			printf("Le répertoire d'origine n'existe plus.\n");
			dir->is_symbol = 2;
			fs.current = copie;
			return;
		}
		else {
			fs.current = dir->origin;
		}
	}
	else{
		fs.current = dir;
	}
    char *chemin = build_path(fs.current);
    printf("Repertoire courant change vers '%s'.\n", chemin);
    free(chemin);
}

void fs_pwd() {
    char *chemin = build_path(fs.current);
    printf("%s\n", chemin);
    free(chemin);
}

void fs_ls(const char *arg) {
    FileEntry *cible = NULL;
    if (arg == NULL)
        cible = fs.current;
    else {
        cible = resolve_path(arg, NULL);
        if (!cible) {
            printf("Repertoire introuvable : %s\n", arg);
            return;
        }
        if (!cible->is_directory) {
            printf("%s\n", cible->name);
            return;
        }
    }
    FileEntry *child = cible->child;
    while (child) {
        if (child->is_symbol == 1){
            printf("\033[1;36m%s\033[0m  ", child->name);
        }
        else if(child->is_symbol == 2){
			printf("\033[1;31m%s\033[0m  ", child->name);
		}
        else if (child->is_directory){
            printf("\033[1;34m%s\033[0m  ", child->name);
		}
		else{
            printf("\033[1;32m%s\033[0m  ", child->name);
        }
        child = child->next;
    }
    printf("\n");
}

void fs_ls_l(const char *arg) {
    FileEntry *cible = NULL;
    if (arg == NULL)
        cible = fs.current;
    else {
        cible = resolve_path(arg, NULL);
        if (!cible) {
            printf("Repertoire introuvable : %s\n", arg);
            return;
        }
        if (!cible->is_directory) {
            char perms_text[50];
            get_perms_text(cible->perms, perms_text, sizeof(perms_text));
            printf("%c%c%c %-5d %-20s %-5d %s%s\n",
                   (cible->perms & 4) ? 'r' : '-',
                   (cible->perms & 2) ? 'w' : '-',
                   (cible->perms & 1) ? 'x' : '-',
                   cible->inode, perms_text, cible->size,
                   cible->name, cible->is_directory ? "/" : "");
            return;
        }
    }
    FileEntry *child = cible->child;
    while (child) {
		char perms_text[50];
		get_perms_text(child->perms, perms_text, sizeof(perms_text));
        //Lien symbolique mort
        if(child->is_symbol == 2){
			printf("lrwx %d %d \033[1;31m%s->%s\033[0m\n", child->link_count, child->size, child->name, child->nom_origin);
        }
        //Lien symbolique vivant
        else if (child->is_symbol == 1){
			printf("lrwx %d %d \033[1;36m%s->%s\033[0m\n", child->link_count, child->size, child->name, child->nom_origin);
		}
		//Dossier
		else if (child->is_directory){
			printf("d%c%c%c %d %d \033[1;34m%s\033[0m\n", 
				(child->perms & 4) ? 'r' : '-',
                (child->perms & 2) ? 'w' : '-',
                (child->perms & 1) ? 'x' : '-',
                child->link_count, child->size, child->name);
		}
		//Fichier
		else {
			printf("-%c%c%c %d %d \033[1;32m%s\033[0m\n",
				(child->perms & 4) ? 'r' : '-',
                (child->perms & 2) ? 'w' : '-',
                (child->perms & 1) ? 'x' : '-',
                child->link_count, child->size, child->name);
		}
        child = child->next;
    }
}

/*void fs_ls_l(const char *arg) {
    FileEntry *cible = NULL;
    if (arg == NULL)
        cible = fs.current;
    else {
        cible = resolve_path(arg, NULL);
        if (!cible) {
            printf("Repertoire introuvable : %s\n", arg);
            return;
        }
        if (!cible->is_directory) {
            char perms_text[50];
            get_perms_text(cible->perms, perms_text, sizeof(perms_text));
            printf("%c%c%c %-5d %-20s %-5d %s%s\n",
                   (cible->perms & 4) ? 'r' : '-',
                   (cible->perms & 2) ? 'w' : '-',
                   (cible->perms & 1) ? 'x' : '-',
                   cible->inode, perms_text, cible->size,
                   cible->name, cible->is_directory ? "/" : "");
            return;
        }
    }
    FileEntry *child = cible->child;
    while (child) {
        char perms_text[50];
        get_perms_text(child->perms, perms_text, sizeof(perms_text));
        printf("%c%c%c %-5d %-20s %-5d %s%s\n",
               (child->perms & 4) ? 'r' : '-',
               (child->perms & 2) ? 'w' : '-',
               (child->perms & 1) ? 'x' : '-',
               child->inode, perms_text, child->size,
               child->name, child->is_directory ? "/" : "");
        child = child->next;
    }
}*/

/**
 * @brief Liste le contenu d'un repertoire avec les inodes.
 *
 * Si aucun argument n'est fourni, le repertoire courant est liste.
 * Sinon, le chemin donne est resolu et son contenu est affiche.
 *
 * @param arg Chemin optionnel du repertoire a lister.
 */
void fs_ls_i(const char *arg) {
    FileEntry *cible = NULL;
    if (arg == NULL) {
        cible = fs.current;
    } else {
        cible = resolve_path(arg, NULL);
        if (!cible) {
            printf("Repertoire introuvable : %s\n", arg);
            return;
        }
        if (!cible->is_directory) {
            printf("%d %s\n", cible->inode, cible->name);
            return;
        }
    }
    FileEntry *child = cible->child;
    while (child) {
		if (child->is_symbol == 1){
			printf("%d \033[1;36m%s\033[0m  ", child->inode, child->name);
		}
		else if (child->is_symbol == 2){
			printf("%d \033[1;31m%s\033[0m  ", child->inode, child->name);
		}
		else if (child->is_directory){
			printf("%d \033[1;34m%s\033[0m  ", child->inode, child->name);
		}
		else{
			printf("%d \033[1;32m%s\033[0m  ", child->inode, child->name);
		}
        child = child->next;
    }
    printf("\n");
}

void fs_cat(const char *filename) {
	FileEntry* copie = fs.current;
    FileEntry *file = resolve_path(filename, NULL);
    //Inexistant ou répertoire = dehors
    if (!file || file->is_directory) {
        printf("Fichier introuvable ou ce n'est pas un fichier.\n");
        return;
    }
    //Lien symbolique
    if (file->is_symbol) {
		fs.current = file->origin->parent; //Sert à vérifier que le fichier d'origine existe
		//Lien mort
        if (resolve_path(file->origin->name, NULL) == NULL){
			printf("Le fichier d'origine n'existe plus.\n");
			file->is_symbol = 2;
			fs.current = copie;
			return;
		}
		//Lien vivant
		else{
			if (file->origin->content){
				printf("%s\n", file->origin->content);
			}
			file->is_symbol = 1;
			fs.current = copie;
			return;
		}
    }
    //Fichier
    else{
		if (file->content){
			printf("%s\n", file->content);
		}
	}
}

/* 
 * Modification de fs_create : création d'un fichier avec taille par defaut, 
 * sans besoin de fournir la taille par l'utilisateur.
 */
void fs_touch(const char *filename) {
    if (find_entry(fs.current, filename)) {
        printf("Le fichier existe deja.\n");
        return;
    }
    FileEntry *file = malloc(sizeof(FileEntry));
    file->inode = next_inode++;
    file->is_symbol = 0;
    file->origin = NULL;
    file->name = strdup(filename);
    file->is_directory = 0;
    file->size = DEFAULT_FILE_SIZE;
    file->link_count = 1;
    file->perms = 6;  // rw par defaut
    file->child = NULL;
    file->next = NULL;
    file->content = calloc(DEFAULT_FILE_SIZE + 1, sizeof(char));
    add_entry(fs.current, file);
    printf("Fichier '%s' cree avec une taille par defaut de %d octets.\n", filename, DEFAULT_FILE_SIZE);
}

/*
 * Commande write modifiee : prend en argument le nom du fichier et le texte.
 * Elle ouvre le fichier en ecriture, écrit le texte et ferme le fichier automatiquement.
 */
void fs_write_cmd(const char *filename, const char *texte) {
	FileEntry* copie = fs.current;
	FileEntry* file = resolve_path(filename, NULL);
	int fd;
	//Lien symbolique
	if(file->is_symbol){
		fs.current = file->origin->parent; //On se déplace dans le dossier du fichier d'origine sinon ça ne marche pas
		//Lien mort
		if (resolve_path(file->origin->name, NULL) == NULL){
			printf("Le fichier d'origine n'existe plus.\n");
			file->is_symbol = 2;
			fs.current = copie;
			return;
		}
		//Lien vivant
		else{
			fd = fs_open(file->origin->name, 2);
			file->is_symbol = 1;
		}
	}
	//Pas un lien symbolique
	else{
		fd = fs_open(filename, 2);
	}
	//Traitement
    if (fd < 0) {
        printf("Ecriture impossible, fichier introuvable ou permissions insuffisantes.\n");
        return;
    }
    int written = fs_write(fd, texte);
    if (written >= 0)
        printf("Ecriture de %d octets dans '%s'.\n", written, filename);
    fs_close(fd);
    fs.current = copie;
}

/*
 * Commande mv, chmod, link, ln, unlink, rm, fsck restent identiques.
 */

void fs_chmod(const char *perm_str, const char *path) {
    int perm = atoi(perm_str);
    FileEntry *entry = resolve_path(path, NULL);
    if (!entry) {
        printf("Entree introuvable : %s\n", path);
        return;
    }
    //Lien symbolique = dehors
    if(entry->is_symbol == 1|| entry->is_symbol == 2){ //Pas d'espace entre le 1 et la barre, sinon ça compile pas
		printf("Interdiction de modifier les droits d'un lien symbolique\n");
	}
	//Fichier
	else{
		//Permission entre 0 et 7 = impossible de mettre 777777777
		if(perm > -1 && perm < 8){
			entry->perms = perm;
			printf("Les permissions de '%s' sont definies a %d.\n", entry->name, perm);
		}
		else{
			printf("%d n'est pas compris entre 0 et 7.\n", perm);
		}
	}
}

void fs_ln(const char *src, const char *dest) {
    FileEntry *file = resolve_path(src, NULL);
    if (!file || file->is_directory) {
        printf("Fichier source introuvable ou ce n'est pas un fichier.\n");
        return;
    }
    if (find_entry(fs.current, dest)) {
        printf("Le nom de destination existe deja.\n");
        return;
    }
    file->link_count++;
    FileEntry *nouveau_lien = malloc(sizeof(FileEntry));
    nouveau_lien->inode = file->inode; // même inode pour lien physique
    nouveau_lien->is_symbol = 0;
    nouveau_lien->origin = NULL;
    nouveau_lien->name = strdup(dest);
    nouveau_lien->is_directory = 0;
    nouveau_lien->size = file->size;
    nouveau_lien->content = file->content;
    nouveau_lien->link_count = file->link_count;
    nouveau_lien->perms = file->perms;
    nouveau_lien->child = NULL;
    nouveau_lien->next = NULL;
    add_entry(fs.current, nouveau_lien);
    printf("Lien physique '%s' cree pour '%s'.\n", dest, src);
}

void fs_ln_s(const char *src, const char *dest) {
    FileEntry *file = resolve_path(src, NULL);
    if (!file) {
        printf("Source introuvable.\n");
        return;
    }
    if (find_entry(fs.current, dest)) {
        printf("Le nom de destination existe deja.\n");
        return;
    }
    FileEntry *nouveau_lien = malloc(sizeof(FileEntry));
    nouveau_lien->inode = next_inode++;
    nouveau_lien->is_symbol = 1;
    nouveau_lien->origin = file;
    nouveau_lien->nom_origin = build_path(nouveau_lien->origin);
    nouveau_lien->name = strdup(dest);
    nouveau_lien->is_directory = file->is_directory;
    nouveau_lien->size = file->size;
    nouveau_lien->content = NULL;
    nouveau_lien->link_count = 1;
    nouveau_lien->perms = 7;
    nouveau_lien->child = NULL;
    nouveau_lien->next = NULL;
    nouveau_lien->parent = fs.current;
    add_entry(fs.current, nouveau_lien);
    printf("Lien symbolique '%s' cree pour '%s'.\n", dest, src);
}

/*void fs_unlink(const char *filename) {
    FileEntry *file = resolve_path(filename, NULL);
    if (!file || file->is_directory) {
        printf("Fichier introuvable ou ce n'est pas un fichier.\n");
        return;
    }
    if (!file->parent) {
        printf("Impossible de supprimer la racine.\n");
        return;
    }
    FileEntry **courant = &file->parent->child;
    while (*courant) {
        if (*courant == file) {
            *courant = file->next;
            file->link_count--;
            if (file->link_count == 0) {
                free(file->name);
                if (file->content)
                    free(file->content);
                free(file);
            }
            printf("Lien supprime pour '%s'.\n", filename);
            return;
        }
        courant = &(*courant)->next;
    }
}*/

void fs_rm(const char *path) {
    FileEntry *parent = NULL;
    FileEntry *entry = resolve_path(path, &parent);
    if (!entry) {
        printf("Entree introuvable : %s\n", path);
        return;
    }
    if (!parent) {
        printf("Impossible de supprimer la racine.\n");
        return;
    }
    if (entry->is_directory && entry->child != NULL) {
        printf("Le repertoire n'est pas vide : %s\n", path);
        return;
    }
    FileEntry **courant = &parent->child;
    while (*courant) {
        if (*courant == entry) {
            *courant = entry->next;
            free(entry->name);
            if (entry->content)
                free(entry->content);
            free(entry);
            printf("Supprime : %s\n", path);
            return;
        }
        courant = &(*courant)->next;
    }
}

void fs_mv(const char *src, const char *dest) {
    FileEntry *parent = NULL;
    FileEntry *entry = resolve_path(src, &parent);
    if (!entry) {
        printf("Source introuvable : %s\n", src);
        return;
    }
    char *dest_copy = strdup(dest);
    char *last_slash = strrchr(dest_copy, '/');
    FileEntry *new_parent = NULL;
    char *new_name = NULL;
    if (last_slash) {
        *last_slash = '\0';
        new_name = last_slash + 1;
        new_parent = resolve_path(dest_copy, NULL);
        if (!new_parent || !new_parent->is_directory) {
            printf("Destination invalide : %s\n", dest_copy);
            free(dest_copy);
            return;
        }
    } else {
        new_parent = parent;
        new_name = dest_copy;
    }
    FileEntry **cur = &parent->child;
    while (*cur) {
        if (*cur == entry) {
            *cur = entry->next;
            break;
        }
        cur = &(*cur)->next;
    }
    free(entry->name);
    entry->name = strdup(new_name);
    entry->parent = new_parent;
    add_entry(new_parent, entry);
    printf("Deplace '%s' vers '%s'.\n", src, dest);
    free(dest_copy);
}

void fs_fsck() {
    int fichiers = 0, repertoires = 0;
    void fsck_helper(FileEntry *entry) {
        if (!entry) return;
        if (entry->is_directory) {
            repertoires++;
            FileEntry *child = entry->child;
            while (child) {
                fsck_helper(child);
                child = child->next;
            }
        } else {
            fichiers++;
        }
    }
    fsck_helper(fs.root);
    printf("FSCK : Repertoires : %d, Fichiers : %d\n", repertoires, fichiers);
}

/* --- Boucle principale --- */

int main() {
    char commande[512];
    mkfs();  // Formatage initial

    printf("Systeme de fichiers simple. Tapez 'help' pour la liste des commandes.\n");
    while (1) {
        char *chemin = build_path(fs.current);
        printf("\033[1;32mhebcfs\033[0m:\033[1;34m%s\033[0m> ", chemin);
        free(chemin);

        if (!fgets(commande, sizeof(commande), stdin))
            break;
        commande[strcspn(commande, "\n")] = 0;
        char *token = strtok(commande, " ");
        if (!token)
            continue;
        if (strcmp(token, "exit") == 0)
            break;
        else if (strcmp(token, "mkfs") == 0) {
            mkfs();
        }
        else if (strcmp(token, "touch") == 0) {
            char *fichier = strtok(NULL, " ");
            if (!fichier) {
                printf("Usage : touch <fichier>\n");
                continue;
            }
            fs_touch(fichier);
        }
        else if (strcmp(token, "write") == 0) {
            char *fichier = strtok(NULL, " ");
            char *texte = strtok(NULL, "");
            if (!fichier || !texte) {
                printf("Usage : write <fichier> <texte>\n");
                continue;
            }
            fs_write_cmd(fichier, texte);
        }
        else if (strcmp(token, "lseek") == 0) {
            // Optionnel : peut rester accessible si besoin de repositionner le curseur via un script backend.
            char *fd_str = strtok(NULL, " ");
            char *offset_str = strtok(NULL, " ");
            if (!fd_str || !offset_str) {
                printf("Usage : lseek <fd> <offset>\n");
                continue;
            }
            int fd = atoi(fd_str);
            int offset = atoi(offset_str);
            fs_lseek(fd, offset);
        }
        else if (strcmp(token, "mkdir") == 0) {
            char *dir = strtok(NULL, " ");
            if (!dir) {
                printf("Usage : mkdir <repertoire>\n");
                continue;
            }
            fs_mkdir(dir);
        }
        else if (strcmp(token, "rmdir") == 0) {
            char *dir = strtok(NULL, " ");
            if (!dir) {
                printf("Usage : rmdir <repertoire>\n");
                continue;
            }
            fs_rmdir(dir);
        }
        else if (strcmp(token, "cd") == 0) {
            char *dir = strtok(NULL, " ");
            if (!dir) {
                printf("Usage : cd <repertoire>\n");
                continue;
            }
            fs_cd(dir);
        }
        else if (strcmp(token, "pwd") == 0) {
            fs_pwd();
        }
        else if (strcmp(token, "ls") == 0) {
            char *arg = strtok(NULL, " ");
            if (arg && strcmp(arg, "-l") == 0) {
                char *opt = strtok(NULL, " ");
                fs_ls_l(opt);
            } 
            else if (arg && strcmp(arg, "-i") == 0){
				char *opt = strtok(NULL, " ");
                fs_ls_i(opt);
            } 
            else {
                fs_ls(arg);
            }
        }
        else if (strcmp(token, "cat") == 0) {
            char *fichier = strtok(NULL, " ");
            if (!fichier) {
                printf("Usage : cat <fichier>\n");
                continue;
            }
            fs_cat(fichier);
        }
        else if (strcmp(token, "chmod") == 0) {
            char *perm_str = strtok(NULL, " ");
            char *cheminArg = strtok(NULL, " ");
            if (!perm_str || !cheminArg) {
                printf("Usage : chmod <perm> <chemin>\n");
                continue;
            }
            fs_chmod(perm_str, cheminArg);
        }
        else if (strcmp(token, "ln") == 0) {
			int symbolique = 0;
			char *arg = strtok(NULL, " ");
			//Cas du lien symbolique
            if (arg && strcmp(arg, "-s") == 0) {
                symbolique = 1;
            }
            char *src;
            if(symbolique == 0){
				src = arg;
			}
			else{
				src = strtok(NULL, " ");
			}
            char *dest = strtok(NULL, " ");
            if (!src || !dest) {
                printf("Usage : ln [-s] <source> <destination>\n");
                continue;
            }
            if(symbolique == 1){
				fs_ln_s(src, dest);
			}
			else{
				fs_ln(src, dest);
			}
        }
        /*else if (strcmp(token, "unlink") == 0) {
            char *fichier = strtok(NULL, " ");
            if (!fichier) {
                printf("Usage : unlink <fichier>\n");
                continue;
            }
            fs_unlink(fichier);
        }*/
        else if (strcmp(token, "rm") == 0) {
            char *cheminArg = strtok(NULL, " ");
            if (!cheminArg) {
                printf("Usage : rm <chemin>\n");
                continue;
            }
            fs_rm(cheminArg);
        }
        else if (strcmp(token, "mv") == 0) {
            char *src = strtok(NULL, " ");
            char *dest = strtok(NULL, " ");
            if (!src || !dest) {
                printf("Usage : mv <source> <destination>\n");
                continue;
            }
            fs_mv(src, dest);
        }
        else if (strcmp(token, "fsck") == 0) {
            fs_fsck();
        }
        else if (strcmp(token, "tree") == 0) {
            int show_inodes = 0;
            char *arg = strtok(NULL, " ");
            if (arg && strcmp(arg, "-i") == 0) {
                show_inodes = 1;
                arg = strtok(NULL, " ");
            }
            FileEntry *start = (arg) ? resolve_path(arg, NULL) : fs.current;
            if (!start) {
                printf("Chemin introuvable pour tree : %s\n", arg);
            } else if (show_inodes == 0){
                fs_tree(arg, 0);
            }
            else {
				fs_tree_i(arg, 0);
			}
        }
        else if (strcmp(token, "help") == 0) {
            printf("Commandes disponibles :\n");
            printf("  cat <fichier>             : Affiche le contenu d'un fichier\n");
            printf("  cd <repertoire>           : Change le repertoire courant\n");
            printf("  chmod <perm> <chemin>     : Modifie les permissions\n");
            printf("  touch <fichier>           : Cree un fichier avec taille par defaut\n");
            printf("  exit                      : Quitte le programme\n");
            printf("  fsck                      : Affiche des statistiques\n");
            printf("  help                      : Affiche ce message\n");
            printf("  ln <src> <dest>           : Cree un lien physique\n");
            printf("  ln -s <src> <dest>        : Cree un lien symbolique\n");
            printf("  ls [<chemin> | -l [<chemin>]] : Liste le contenu\n");
            printf("  mkdir <repertoire>        : Cree un repertoire\n");
            printf("  mkfs                      : Formate le systeme\n");
            printf("  mv <source> <dest>        : Deplace ou renomme\n");
            printf("  pwd                       : Affiche le chemin courant\n");
            printf("  tree [--inodes] [<chemin>] : Affiche l'arborescence\n");
            //printf("  unlink <fichier>          : Supprime un lien\n");
            printf("  write <fichier> <texte>   : Ecrit dans un fichier\n");
        }
        else {
            printf("Commande inconnue. Tapez 'help' pour afficher la liste des commandes.\n");
        }
    }
    return 0;
}
