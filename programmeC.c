#include "def.h"
#include <jni.h>

#include <unistd.h>

#include <sys/sem.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

/******************************************************************/

#define ENTIER 4
#define CHEMIN "./def.h"

#define MUTEX  0

/******************************************************************/

typedef union  {
    int val;                    /* value for SETVAL */
    struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
    unsigned short int *array;  /* array for GETALL, SETALL */
        struct seminfo *__buf;      /* buffer for IPC_INFO */
} semval ;

// fonctions définies dans programmeC, l'utilsateur n'a pas besoin n'y avoir accès
int sem_creation(int *semid, int nb_semaphores) ;
int sem_initialisation(int semid, int num_semaphore, int nbr_jetons) ;

int sem_destruction(int semid) ;
int sem_recup(int *semid, int taille) ;

void twisk_entrer(int file) ;
void twisk_quitter(int file) ;

/*******************************************************************/

/* variables globales, car partagées dans les fonctions appelées par java */
/* avec un nom unique pour ne pas interférer avec les variables globales éventuelles des clients pour la simulation */

int twisk_nbClients ;
int twisk_nbEtapes ;
int twisk_nbSemaphores ;

int *twisk_pEtats ;

int twisk_idmp ;

/* seul ids garde son nom car il est également utilisé dans le code C des clients */
int ids ;

/* ----------------------------------------------------------------------------------------------- */

int sem_creation(int *semid, int nb_semaphores) {
  key_t cle = ftok(CHEMIN, ENTIER) ;

  if (cle == -1) {
    perror("erreur lors de la génération de la clé") ;
    exit(1) ;
  }

  *semid = semget(cle, nb_semaphores, IPC_CREAT | 0644) ;
  return *semid ;
}

int sem_initialisation(int semid, int num_semaphore, int nbr_jetons) {
  semval sem ;

  sem.val = 1 ;
  return (semctl(semid, 0, SETVAL, sem)) ;
}

int sem_destruction(int semid) {
  return (semctl(semid, IPC_RMID, 0)) ;
}

int sem_recup(int *semid, int taille) {
   key_t cle = ftok(CHEMIN, ENTIER) ;

  if (cle == -1) {
    perror("erreur lors de la génération de la clé") ;
    exit(1) ;
  }

  *semid = semget(cle, taille, 0) ;
  return *semid ;
}

int P(int semid, int numero) {
    struct sembuf smbf;

    smbf.sem_num = numero ;        /* Numéro du sémaphore */
    smbf.sem_op = -1 ;             /* P : on décrémente le nombre de jetons */
    smbf.sem_flg = 0 ;

    return (semop(semid, &smbf, 1)) ;
}

int V(int semid, int numero) {
    struct sembuf smbf ;

    smbf.sem_num = numero ;        /* Numéro du sémaphore */
    smbf.sem_op = 1 ;              /* V : on incrémente le nombre de jetons */
    smbf.sem_flg = 0 ;

    return (semop(semid, &smbf, 1)) ;
}

/* ----------------------------------------------------------------------------------------------- */
void delai(int temps, int delta) {

  int bi, bs ;
  int n, nbSec ;

  bi = temps - delta ;
  bs = temps + delta ;
  if (bi < 0) bi = 0 ;

  n = bs - bi ;

  nbSec = (rand()/ (float)RAND_MAX) * n ;
  nbSec += bi ;
  // printf("client : %d, dodo : %d\n", getpid(), nbSec) : fflush(stdout) ;
  sleep(nbSec) ;
}

/* ----------------------------------------------------------------------------------------------- */
void twisk_entrer(int file) {
   int nb ;

   nb = *(twisk_pEtats + file * (twisk_nbClients + 1)) ;

   nb++ ;

   *(twisk_pEtats + file * (twisk_nbClients + 1)) = nb ;
   *(twisk_pEtats + file * (twisk_nbClients + 1) + nb) = (int) getpid() ;
}

/* ----------------------------------------------------------------------------------------------- */
void twisk_quitter(int file) {
   int j, nb, indice ;
   int tableau[twisk_nbClients] ;

   nb = *(twisk_pEtats + file * (twisk_nbClients + 1)) ;

   /* on enlève le processus pid de la file d'attente */
   indice = 0 ;
   for (j = 1 ; j <= nb ; j++) {
        if (*(twisk_pEtats + file * (twisk_nbClients + 1) + j) != getpid())
            tableau[indice++] = *(twisk_pEtats + file * (twisk_nbClients + 1) + j) ;
   }

   nb -- ;
   *(twisk_pEtats + file * (twisk_nbClients + 1)) = nb ;

    for (j = 1 ; j <= nb ; j++) {
         *(twisk_pEtats + file * (twisk_nbClients + 1) + j) = tableau[j-1] ;
    }
}

/* ----------------------------------------------------------------------------------------------- */

 /* ids : identifiant du tableau de sémaphores */
 void initialisation_des_services(int ids, int nbServices, int* tab) {

      semval leSemaphore ;
      int i ;

      // printf("nombre de sémaphores : %d\n", nbServices) ; fflush(stdout) ;

        for (i = 0 ; i < nbServices ; i++)
        {
           leSemaphore.val = tab[i] ;
           semctl(ids, i+1, SETVAL, leSemaphore);
           // printf("init de %d à %d\n", (i+1), leSemaphore.val) ;
           fflush(stdout) ;
        }
}

/* ----------------------------------------------------------------------------------------------- */

int creation_dun_client(int c)
{
    pid_t pid ;
    pid = fork();
    switch (pid)
      {
      case (pid_t) -1 :
            perror("Création impossible\n");
            exit(-1);
      case (pid_t) 0 :
            /* début de l'exécution du code du client */
            srand(getpid()) ;
            simulation(ids) ;
      }

      return (int) pid ;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// ces fonctions permettent de cacher l'usage du sémaphore d'exclusion mutuelle de protection du
// segment de mémoire partagé
/* -------------------------------------------------------------------------------------------------------------------------------- */
void entrer(int destination)
{
   P(ids, MUTEX) ;
       twisk_entrer(destination) ;
   V(ids, MUTEX) ;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
void transfert(int source, int destination)
{
   P(ids, MUTEX) ;
       twisk_quitter(source) ;
       twisk_entrer(destination) ;
   V(ids, MUTEX) ;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
JNIEXPORT void JNICALL Java_twisk_simulation_SimulationC_nettoyage (JNIEnv *env, jobject obj)
{
  int c, status ;

  for (c = 0 ; c < twisk_nbClients ; c++) {
      wait(&status) ;
  }

  /* destruction des sémaphores */
  semctl(ids, IPC_RMID, 0)  ;

  /* détachement du segment */
  shmdt(twisk_pEtats) ;

  /* destruction du segment */
  shmctl(twisk_idmp, IPC_RMID, 0) ;

  return ;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
/*
Fonctions ajoutées pour qu'elles puissent être directement appelées depuis un main... 12 novembre 2018
*/
/* -------------------------------------------------------------------------------------------------------------------------------- */
// retourne le tableau des numéros des processus (pid) créés
int* start_simulation(int nbEtapes, int nbServices, int nbClients, int *tabJetonsServices)
{
  semval leSemaphore ;
  int i, c ;

  twisk_nbEtapes = nbEtapes ;
  twisk_nbSemaphores = nbServices + 1 ;
  twisk_nbClients = nbClients ;

  int *tabPid ;
  tabPid = (int *) malloc (sizeof(int) * nbClients) ;

  /* création du segment de mémoire partagée stockant les différents compteurs de clients */
  twisk_idmp = shmget(IPC_PRIVATE, 1 * sizeof(int) * (nbClients + 1) * nbEtapes, IPC_CREAT | 0644) ;
  if (twisk_idmp == -1) {
    perror("Erreur lors de la création du segment de mémoire partagée") ;
    exit(1) ;
  }

  /* attachement du segment à une adresse */
  twisk_pEtats = shmat(twisk_idmp, NULL, 0) ;

  /* initialisation des compteurs dans chaque étape */
  for(i = 0 ; i < nbEtapes ; i++) {
     *(twisk_pEtats + i * (nbClients + 1)) = 0 ;
  }

  /* création des sémaphores particuliers à la simulation et du sémaphore d'exclusion mutuelle pour protéger le segment de
     mémoire partagé */
  ids = semget(IPC_PRIVATE, twisk_nbSemaphores, IPC_CREAT | 0644) ;
  if (ids == -1) {
    perror("Erreur lors de la création du sémaphore") ;
    exit(1) ;
  }

  /* initialisation des sémaphores */
  initialisation_des_services(ids, nbServices, tabJetonsServices) ;

  /* initialisation du sémaphore d'exclusion mutuelle qui protège l'accès à la ressource partagée */
  leSemaphore.val = 1 ;
  semctl(ids, MUTEX, SETVAL, leSemaphore);

  /* création des clients */
  for (c = 0 ; c < nbClients ; c++) {
       tabPid[c] = creation_dun_client(c) ;
  }

  return tabPid ;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// retourne la matrice de l'état du graphe
//    tableau[i][0] : nombre total de clients à l'étape i ;
//    tableau[i][j], j != 0 : numéro de processus du jème client à l'étape i
int* ou_sont_les_clients(int nbEtapes, int nbClients)
{
     int i, j ;
     int* tableau;
     tableau = (int *) malloc(sizeof(int) * (nbEtapes * (nbClients + 1)));

     P(ids, MUTEX) ;

        for (i = 0 ; i < nbEtapes ; i++) {
             for (j = 0 ; j < (nbClients + 1) ; j++) {
                *(tableau + (i*(nbClients + 1)) + j) = *(twisk_pEtats + (i*(nbClients + 1)) + j) ;
             }
        }

     V(ids, MUTEX) ;

     return tableau ;
} 