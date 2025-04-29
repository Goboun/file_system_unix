
# 📂 Projet C – Système de Fichiers Simulé

Ce projet est une simulation simple d’un système de fichiers, codé en langage C. Il permet d'exécuter des commandes système courantes comme `cat`, `cd`, `chmod`, etc., via un programme en ligne de commande. Il utilise un **Makefile** pour simplifier la compilation, l'exécution et le nettoyage des fichiers intermédiaires générés durant le processus de développement.

---

## 🧩 Introduction

L'objectif de ce projet est de créer un programme interactif qui imite certaines des fonctionnalités d'un système de fichiers Unix. Il inclut des commandes qui permettent à l'utilisateur de manipuler des fichiers et des répertoires, de changer les permissions, de visualiser des statistiques, et plus encore.

---

## ⚙️ Compilation et Exécution

### Prérequis

Avant de commencer, assurez-vous que les outils suivants sont installés sur votre système :

- **gcc** (compilateur C) : Utilisé pour compiler le code source en exécutable.
- **make** : Outil utilisé pour automatiser le processus de compilation et de gestion des dépendances à travers le `Makefile`.

### Étapes d'utilisation

1. **Cloner ou télécharger le projet**  
   Si ce projet est disponible sur un dépôt Git, vous pouvez le cloner avec la commande suivante :
   
   ```bash
   git clone <url-du-dépôt>
   ```
   
   Sinon, téléchargez simplement les fichiers du projet.

2. **Compiler le projet**  
   Ouvrez un terminal dans le répertoire du projet et exécutez la commande suivante pour compiler les fichiers source et générer l'exécutable :
   
   ```bash
   make
   ```
   
   Cette commande va :
   - Compiler `fonctions.c` en `fonctions.o`
   - Compiler `main.c` en `main.o`
   - Générer l'exécutable `main`

3. **Lancer le programme**  
   Après compilation, lancez le programme avec :

   ```bash
   make run
   ```

   Cela exécutera l'exécutable `main` et ouvrira l'interface interactive.

4. **Nettoyer les fichiers intermédiaires**  
   Pour supprimer les fichiers objets (`*.o`), exécutez :

   ```bash
   make clear
   ```

   Cela laisse l'exécutable `main` intact tout en nettoyant les fichiers temporaires.

---

## 🧾 Contenu du Makefile

Voici le contenu du `Makefile` utilisé pour ce projet :

```make
all : fonctions.o main.o main

fonctions.o : fonctions.c fonctions.h structures.h
	gcc -c fonctions.c

main.o : main.c fonctions.o structures.h
	gcc -c main.c

main : main.o fonctions.o structures.h
	gcc -o main main.o fonctions.o

run :
	./main

clear :
	rm -f *.o
```

- **`make`** (par défaut alias `make all`) : Compile l’ensemble du projet et génère l’exécutable `main`.  
- **`make run`** : Exécute le programme interactif.  
- **`make clear`** : Supprime tous les fichiers objets (`*.o`).

---

## 🧮 Commandes disponibles dans l'interface utilisateur

Une fois le programme lancé, utilisez les commandes suivantes pour interagir :

| Commande                                  | Description                                          |
|-------------------------------------------|------------------------------------------------------|
| `cat <fichier>`                           | Affiche le contenu d'un fichier                      |
| `cd <repertoire>`                         | Change le répertoire courant                         |
| `chmod <perm> <chemin>`                   | Modifie les permissions d'un fichier ou répertoire   |
| `exit`                                    | Quitte le programme                                  |
| `fsck`                                    | Affiche des statistiques sur le système de fichiers  |
| `help`                                    | Affiche ce message d'aide                            |
| `ln <src> <dest>`                         | Crée un lien physique entre deux fichiers            |
| `ln -s <src> <dest>`                      | Crée un lien symbolique entre deux fichiers          |
| `ls [<chemin>]` ou `ls -l [<chemin>]`     | Liste le contenu d’un dossier (`-l` pour détails)    |
| `mkdir <repertoire>`                      | Crée un nouveau répertoire                           |
| `mkfs`                                    | Formate le système de fichiers                       |
| `mv <source> <dest>`                      | Déplace ou renomme un fichier ou un répertoire       |
| `pwd`                                     | Affiche le répertoire courant                        |
| `touch <fichier>`                         | Crée un fichier vide ou met à jour sa date           |
| `tree [--inodes] [<chemin>]`              | Affiche l’arborescence du système (`--inodes` option)|
| `write <fichier> <texte>`                 | Écrit du texte dans un fichier                       |

> 💡 **Astuce :** Tapez `help` à tout moment pour afficher cette liste.

---

## 🏷️ Auteur

Développé par **[Ton Nom]**.  
Licence : [Nom de la licence].

---

## 🚀 Améliorations futures

- Gestion avancée des permissions et des propriétaires.  
- Support de la suppression de fichiers (`unlink`).  
- Intégration de journaux (logging) pour les opérations.

---

## 📄 Licence

Ce projet est sous la licence [Nom de la licence] – voir le fichier `LICENSE` pour plus de détails.
