/**
 * @file main.c
 * @brief Systeme de fichiers simple en C.
 *
 * Ce programme simule un systeme de fichiers en memoire.
 * Il supporte les commandes de base telles que mkfs, open, read, write,
 * lseek, close, mkdir, rmdir, cd, pwd, ls, cat, create, chmod, link, unlink, rm,
 * fsck, help et exit.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 
 /* --- Structures --- */
 
 /**
  * @brief Structure representant un fichier ou repertoire.
  */
 typedef struct FileEntry {
     char *name;                /**< Nom de l'entree */
     int is_directory;          /**< 1 si repertoire, 0 si fichier */
     int size;                  /**< Taille (pour fichiers) */
     char *content;             /**< Contenu (pour fichiers, NULL pour repertoires) */
     int link_count;            /**< Nombre de liens physiques */
     int perms;                 /**< Permissions : 4 = lecture, 2 = ecriture, 1 = execution (ex : 6 = lecture/ecriture, 7 = rwx) */
     struct FileEntry *child;   /**< Premier enfant (pour repertoires) */
     struct FileEntry *next;    /**< Element suivant dans le meme repertoire */
     struct FileEntry *parent;  /**< Repertoire parent (NULL pour la racine) */
 } FileEntry;
 
 /**
  * @brief Structure representant le systeme de fichiers.
  */
 typedef struct FileSystem {
     FileEntry *root;           /**< Racine du systeme de fichiers */
     FileEntry *current;        /**< Repertoire courant */
 } FileSystem;
 
 /**
  * @brief Structure representant un fichier ouvert.
  */
 typedef struct OpenFile {
     int fd;                    /**< Descripteur de fichier unique */
     FileEntry *file;           /**< Pointeur sur l'entree correspondante */
     int flags;                 /**< 1 = lecture, 2 = ecriture, 3 = lecture/ecriture */
     int offset;                /**< Position courante dans le fichier */
     struct OpenFile *next;     /**< Element suivant dans la table des fichiers ouverts */
 } OpenFile;
 
 /* --- Variables globales --- */
 FileSystem fs = { NULL, NULL };
 OpenFile *open_files = NULL;
 int next_fd = 3;  // On reserve 0, 1, 2 pour stdio
 
 /* --- Fonctions utilitaires --- */
 
 /**
  * @brief Libere recursivement une entree et ses enfants.
  * @param entry Pointeur sur l'entree a liberer.
  */
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
 
 /**
  * @brief Recherche une entree parmi les enfants d'un repertoire.
  * @param dir Repertoire dans lequel chercher.
  * @param name Nom de l'entree.
  * @return Pointeur sur l'entree trouvee, ou NULL si introuvable.
  */
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
 
 /**
  * @brief Ajoute une entree a la liste des enfants d'un repertoire.
  * @param dir Repertoire cible.
  * @param entry Entree a ajouter.
  */
 void add_entry(FileEntry *dir, FileEntry *entry) {
     if (!dir || !dir->is_directory)
         return;
     entry->next = dir->child;
     dir->child = entry;
     entry->parent = dir;
 }
 
 /**
  * @brief Construit recursivement le chemin complet depuis la racine jusqu'a une entree.
  * @param entry Pointeur sur l'entree.
  * @return Chaine de caracteres contenant le chemin complet (a liberer par l'appelant).
  */
 char *build_path(FileEntry *entry) {
     if (!entry->parent) {  // Racine
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
 
 /**
  * @brief Resout un chemin (absolu ou relatif).
  * @param path Chemin a resoudre.
  * @param parentOut Adresse d'un pointeur recevant le repertoire parent (ou NULL).
  * @return Pointeur sur l'entree trouvee, ou NULL si introuvable.
  */
 FileEntry* resolve_path(const char *path, FileEntry **parentOut) {
     FileEntry *courant;
     if (path[0] == '/')
         courant = fs.root;
     else
         courant = fs.current;
     
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
 
 /* --- Commandes du systeme de fichiers --- */
 
 /**
  * @brief Formate le systeme de fichiers.
  */
 void mkfs() {
     if (fs.root) {
         free_file_entry(fs.root);
     }
     fs.root = malloc(sizeof(FileEntry));
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
 
 /**
  * @brief Ouvre un fichier.
  * 
  * Si le fichier n'existe pas et que flag vaut 2 ou 3, il est cree.
  *
  * @param filename Nom du fichier.
  * @param flag Mode d'ouverture (1 = lecture, 2 = ecriture, 3 = lecture/ecriture).
  * @return Descripteur de fichier, ou -1 en cas d'erreur.
  */
 int fs_open(const char *filename, int flag) {
     FileEntry *entry = find_entry(fs.current, filename);
     if (!entry) {
         if (flag == 2 || flag == 3) {
             entry = malloc(sizeof(FileEntry));
             entry->name = strdup(filename);
             entry->is_directory = 0;
             entry->size = 0;
             entry->content = NULL;
             entry->link_count = 1;
             entry->perms = 6; // rw par defaut
             entry->child = NULL;
             entry->next = NULL;
             entry->parent = fs.current;
             add_entry(fs.current, entry);
         } else {
             printf("Fichier introuvable.\n");
             return -1;
         }
     } else if (entry->is_directory) {
         printf("Impossible d'ouvrir un repertoire.\n");
         return -1;
     }
     OpenFile *of = malloc(sizeof(OpenFile));
     of->fd = next_fd++;
     of->file = entry;
     of->flags = flag;
     of->offset = 0;
     of->next = open_files;
     open_files = of;
     printf("Fichier '%s' ouvert avec le descripteur %d.\n", filename, of->fd);
     return of->fd;
 }
 
 /**
  * @brief Lit un nombre d'octets depuis un fichier ouvert.
  *
  * Verifie la permission de lecture.
  *
  * @param fd Descripteur de fichier.
  * @param size Nombre d'octets a lire.
  * @return Nombre d'octets lus, ou -1 en cas d'erreur.
  */
 ssize_t fs_read(int fd, int size) {
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
     if (!(of->flags == 1 || of->flags == 3)) {
         printf("Fichier non ouvert en lecture.\n");
         return -1;
     }
     if (!(of->file->perms & 4)) {
         printf("Permission refusee : lecture interdite.\n");
         return -1;
     }
     FileEntry *file = of->file;
     if (of->offset >= file->size) {
         printf("Fin de fichier atteinte.\n");
         return 0;
     }
     int dispo = file->size - of->offset;
     int to_read = (size < dispo) ? size : dispo;
     char *buffer = malloc(to_read + 1);
     memcpy(buffer, file->content + of->offset, to_read);
     buffer[to_read] = '\0';
     printf("%s\n", buffer);
     of->offset += to_read;
     free(buffer);
     return to_read;
 }
 
 /**
  * @brief Ecrit une chaine de caracteres dans un fichier ouvert.
  *
  * Verifie la permission d'ecriture.
  *
  * @param fd Descripteur de fichier.
  * @param data Chaine a ecrire.
  * @return Nombre d'octets ecrits, ou -1 en cas d'erreur.
  */
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
     printf("Ecriture de %d octets.\n", data_len);
     return data_len;
 }
 
 /**
  * @brief Positionne le curseur d'un fichier ouvert a un offset donne.
  *
  * @param fd Descripteur de fichier.
  * @param offset Nouvelle position.
  * @return Nouvelle position, ou -1 en cas d'erreur.
  */
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
     printf("Curseur deplace a %d.\n", offset);
     return offset;
 }
 
 /**
  * @brief Ferme un fichier ouvert.
  *
  * @param fd Descripteur de fichier.
  * @return 0 si reussi, -1 sinon.
  */
 int fs_close(int fd) {
     OpenFile **prev = &open_files;
     OpenFile *of = open_files;
     while (of) {
         if (of->fd == fd) {
             *prev = of->next;
             free(of);
             printf("Fermeture du descripteur %d.\n", fd);
             return 0;
         }
         prev = &of->next;
         of = of->next;
     }
     printf("Descripteur invalide.\n");
     return -1;
 }
 
 /**
  * @brief Cree un repertoire dans le repertoire courant.
  *
  * @param dirname Nom du repertoire.
  */
 void fs_mkdir(const char *dirname) {
     if (find_entry(fs.current, dirname)) {
         printf("Un repertoire ou fichier portant ce nom existe deja.\n");
         return;
     }
     FileEntry *dir = malloc(sizeof(FileEntry));
     dir->name = strdup(dirname);
     dir->is_directory = 1;
     dir->size = 0;
     dir->content = NULL;
     dir->link_count = 1;
     dir->perms = 7; // rwx par defaut pour repertoires
     dir->child = NULL;
     dir->next = NULL;
     add_entry(fs.current, dir);
     printf("Repertoire '%s' cree.\n", dirname);
 }
 
 /**
  * @brief Supprime un repertoire vide.
  *
  * @param dirname Nom du repertoire a supprimer.
  */
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
 
 /**
  * @brief Change le repertoire courant.
  *
  * Supporte "cd .." pour remonter.
  *
  * @param dirname Nom du repertoire.
  */
 void fs_cd(const char *dirname) {
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
     fs.current = dir;
     char *chemin = build_path(fs.current);
     printf("Repertoire courant change vers '%s'.\n", chemin);
     free(chemin);
 }
 
 /**
  * @brief Affiche le chemin complet du repertoire courant.
  */
 void fs_pwd() {
     char *chemin = build_path(fs.current);
     printf("%s\n", chemin);
     free(chemin);
 }
 
 /**
  * @brief Liste le contenu d'un repertoire.
  *
  * Si aucun argument n'est fourni, le repertoire courant est liste.
  * Sinon, le chemin donne est resolu et son contenu est affiche.
  *
  * @param arg Chemin optionnel du repertoire a lister.
  */
 void fs_ls(const char *arg) {
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
     FileEntry *child = cible->child;
     while (child) {
         printf("%s%s  ", child->name, child->is_directory ? "/" : "");
         child = child->next;
     }
     printf("\n");
 }
 
 /**
  * @brief Affiche le contenu d'un fichier.
  *
  * @param filename Nom du fichier.
  */
 void fs_cat(const char *filename) {
     FileEntry *file = resolve_path(filename, NULL);
     if (!file || file->is_directory) {
         printf("Fichier introuvable ou c'est un repertoire.\n");
         return;
     }
     if (file->content)
         printf("%s\n", file->content);
     else
         printf("\n");
 }
 
 /**
  * @brief Cree un fichier de taille donnee (initialise a zero).
  *
  * @param filename Nom du fichier.
  * @param size Taille en octets.
  */
 void fs_create(const char *filename, int size) {
     if (find_entry(fs.current, filename)) {
         printf("Le fichier existe deja.\n");
         return;
     }
     FileEntry *file = malloc(sizeof(FileEntry));
     file->name = strdup(filename);
     file->is_directory = 0;
     file->size = size;
     file->link_count = 1;
     file->perms = 6;  // rw par defaut
     file->child = NULL;
     file->next = NULL;
     if (size > 0) {
         file->content = calloc(size + 1, sizeof(char));
     } else {
         file->content = NULL;
     }
     add_entry(fs.current, file);
     printf("Fichier '%s' cree avec une taille de %d octets.\n", filename, size);
 }
 
 /**
  * @brief Change les permissions d'une entree.
  *
  * @param perm_str Chaine representant les permissions (ex : "0", "4", "6", "7").
  * @param path Chemin de l'entree.
  */
 void fs_chmod(const char *perm_str, const char *path) {
     int perm = atoi(perm_str);
     FileEntry *entry = resolve_path(path, NULL);
     if (!entry) {
         printf("Entree introuvable : %s\n", path);
         return;
     }
     entry->perms = perm;
     printf("Les permissions de '%s' sont definies a %d.\n", entry->name, perm);
 }
 
 /**
  * @brief Cree un lien physique pour un fichier.
  *
  * @param src Chemin source.
  * @param dest Nom du lien cree.
  */
 void fs_link(const char *src, const char *dest) {
     FileEntry *file = resolve_path(src, NULL);
     if (!file || file->is_directory) {
         printf("Fichier source introuvable ou c'est un repertoire.\n");
         return;
     }
     if (find_entry(fs.current, dest)) {
         printf("Le nom de destination existe deja.\n");
         return;
     }
     file->link_count++;
     FileEntry *nouveau_lien = malloc(sizeof(FileEntry));
     nouveau_lien->name = strdup(dest);
     nouveau_lien->is_directory = 0;
     nouveau_lien->size = file->size;
     nouveau_lien->content = file->content;  // Partage du meme contenu
     nouveau_lien->link_count = file->link_count;
     nouveau_lien->perms = file->perms;
     nouveau_lien->child = NULL;
     nouveau_lien->next = NULL;
     add_entry(fs.current, nouveau_lien);
     printf("Lien physique '%s' cree pour '%s'.\n", dest, src);
 }
 
 /**
  * @brief Supprime un lien ; si aucun lien ne subsiste, supprime le fichier.
  *
  * @param filename Nom du fichier.
  */
 void fs_unlink(const char *filename) {
     FileEntry *file = resolve_path(filename, NULL);
     if (!file || file->is_directory) {
         printf("Fichier introuvable ou c'est un repertoire.\n");
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
                 if(file->content)
                     free(file->content);
                 free(file);
             }
             printf("Lien supprime pour '%s'.\n", filename);
             return;
         }
         courant = &(*courant)->next;
     }
 }
 
 /**
  * @brief Supprime un fichier ou repertoire vide a partir d'un chemin.
  *
  * @param path Chemin de l'entree a supprimer.
  */
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
 
 /**
  * @brief Affiche des statistiques sur le systeme de fichiers.
  */
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
 
 /**
  * @brief Fonction principale.
  *
  * Lance le systeme de fichiers et attend les commandes de l'utilisateur.
  *
  * @return int 0 en cas de succes.
  */
 int main() {
     char commande[512];
     mkfs();  // Formatage initial du systeme de fichiers
 
     printf("Systeme de fichiers simple. Tapez 'help' pour la liste des commandes.\n");
     while (1) {
         char *chemin = build_path(fs.current);
         printf("hebcfs:%s> ", chemin);
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
         else if (strcmp(token, "open") == 0) {
             char *fichier = strtok(NULL, " ");
             char *flag_str = strtok(NULL, " ");
             if (!fichier || !flag_str) {
                 printf("Usage : open <fichier> <flag>\n");
                 continue;
             }
             int flag = atoi(flag_str);
             fs_open(fichier, flag);
         }
         else if (strcmp(token, "read") == 0) {
             char *fd_str = strtok(NULL, " ");
             char *taille_str = strtok(NULL, " ");
             if (!fd_str || !taille_str) {
                 printf("Usage : read <fd> <taille>\n");
                 continue;
             }
             int fd = atoi(fd_str);
             int taille = atoi(taille_str);
             fs_read(fd, taille);
         }
         else if (strcmp(token, "write") == 0) {
             char *fd_str = strtok(NULL, " ");
             char *texte = strtok(NULL, "");
             if (!fd_str || !texte) {
                 printf("Usage : write <fd> <texte>\n");
                 continue;
             }
             int fd = atoi(fd_str);
             fs_write(fd, texte);
         }
         else if (strcmp(token, "lseek") == 0) {
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
         else if (strcmp(token, "close") == 0) {
             char *fd_str = strtok(NULL, " ");
             if (!fd_str) {
                 printf("Usage : close <fd>\n");
                 continue;
             }
             int fd = atoi(fd_str);
             fs_close(fd);
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
             fs_ls(arg);
         }
         else if (strcmp(token, "cat") == 0) {
             char *fichier = strtok(NULL, " ");
             if (!fichier) {
                 printf("Usage : cat <fichier>\n");
                 continue;
             }
             fs_cat(fichier);
         }
         else if (strcmp(token, "create") == 0) {
             char *fichier = strtok(NULL, " ");
             char *taille_str = strtok(NULL, " ");
             if (!fichier || !taille_str) {
                 printf("Usage : create <fichier> <taille>\n");
                 continue;
             }
             int taille = atoi(taille_str);
             fs_create(fichier, taille);
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
         else if (strcmp(token, "link") == 0) {
             char *src = strtok(NULL, " ");
             char *dest = strtok(NULL, " ");
             if (!src || !dest) {
                 printf("Usage : link <source> <destination>\n");
                 continue;
             }
             fs_link(src, dest);
         }
         else if (strcmp(token, "unlink") == 0) {
             char *fichier = strtok(NULL, " ");
             if (!fichier) {
                 printf("Usage : unlink <fichier>\n");
                 continue;
             }
             fs_unlink(fichier);
         }
         else if (strcmp(token, "rm") == 0) {
             char *cheminArg = strtok(NULL, " ");
             if (!cheminArg) {
                 printf("Usage : rm <chemin>\n");
                 continue;
             }
             fs_rm(cheminArg);
         }
         else if (strcmp(token, "fsck") == 0) {
             fs_fsck();
         }
         else if (strcmp(token, "help") == 0) {
             printf("Commandes disponibles :\n");
             printf("  mkfs                      : Formate le systeme de fichiers\n");
             printf("  open <fichier> <flag>     : Ouvre un fichier (flag : 1=lecture, 2=ecriture, 3=lecture/ecriture)\n");
             printf("  read <fd> <taille>        : Lit des octets depuis un fichier\n");
             printf("  write <fd> <texte>        : Ecrit dans un fichier\n");
             printf("  lseek <fd> <offset>       : Positionne le curseur\n");
             printf("  close <fd>                : Ferme un fichier\n");
             printf("  mkdir <repertoire>        : Cree un repertoire\n");
             printf("  rmdir <repertoire>        : Supprime un repertoire vide\n");
             printf("  cd <repertoire>           : Change le repertoire courant (cd .. pour remonter)\n");
             printf("  pwd                       : Affiche le chemin complet courant\n");
             printf("  ls [<chemin>]             : Liste le contenu d'un repertoire\n");
             printf("  cat <fichier>             : Affiche le contenu d'un fichier\n");
             printf("  create <fichier> <taille>   : Cree un fichier de taille donnee\n");
             printf("  chmod <perm> <chemin>     : Modifie les permissions (ex : 0, 4, 6, 7)\n");
             printf("  link <src> <dest>         : Cree un lien physique\n");
             printf("  unlink <fichier>          : Supprime un lien\n");
             printf("  rm <chemin>               : Supprime un fichier ou repertoire vide\n");
             printf("  fsck                      : Affiche des statistiques du systeme\n");
             printf("  help                      : Affiche ce message\n");
             printf("  exit                      : Quitte le programme\n");
         }
         else {
             printf("Commande inconnue. Tapez 'help' pour la liste des commandes.\n");
         }
     }
     return 0;
 } 
