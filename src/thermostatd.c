#include "thermostatd.h"

/** TODO
 * Boutons programmables
 * Tubes nommés
 * Sockets
 * Fichier configuration
 */

static pthread_t thr_sonde;
static pthread_t thr_boutons;

static pthread_mutex_t mutex_aff;
static pthread_mutex_t mutex_rel;
static pthread_mutex_t mutex_tmes;
static pthread_mutex_t mutex_tprog;

static temperature temp_mesure; // Temperature mesuree
static int temp_programme; // Température programee
static enum state_r etat_relais; // Etats des relais

/**
 * parachute - Quitte le programme proprement
 * Description: Il est très important de sortir du programme correctement en
 * stoppant tout dispositif de chauffage ou de refroidissement.
 */
void parachute(void)
{
    pthread_cancel(thr_sonde);
    pthread_cancel(thr_boutons);
    int fd_chauff = open(C_CHAUFF, O_WRONLY);
    int fd_congel = open(C_CONGEL, O_WRONLY);
    write(fd_chauff, "0", 2);
    write(fd_congel, "0", 2);
    close(fd_chauff);
    close(fd_congel);
    AFF_WRITE("____");
}

/**
 * exit_reboot - quitte le programme et redémarre la machine
 * En cas d'erreur urgente, on redémarre la machine
 */
void exit_reboot(void)
{
    const char *reboot_argv[] = {"reboot", NULL};
    parachute();
    execv("/sbin/reboot", reboot_argv);
}

/**
 * update_relais - Met a jour les relais
 * @mode: M_CHAUFF, M_FROID ou M_STOP
 * description: change le mode pour chauffer, refroidir ou stopper
 */
static void update_relais(enum state_r  mode)
{
    /* Bloquage de la ressource */
    pthread_mutex_lock(&mutex_rel);

    /* Ouverture des fichiers spéciaux */
    int fd_chauff = open(C_CHAUFF, O_WRONLY);
    int fd_congel = open(C_CONGEL, O_WRONLY);
    if (fd_chauff < 0 || fd_congel < 0) {
	syslog(LOG_CRIT|LOG_USER, "Impossible d'ouvrir les fichiers spéciaux des relais\n");
	AFF_WRITE("EP06");
	exit_reboot();
    }

    /* Activation des relais */
    switch (mode) {
	case R_CHAUF:
	    lseek(fd_chauff,0,SEEK_SET); write(fd_chauff,"1",2);
	    lseek(fd_congel,0,SEEK_SET); write(fd_congel,"0",2);
	    break;
	case R_FROID:
	    lseek(fd_chauff,0,SEEK_SET); write(fd_chauff,"0",2);
	    lseek(fd_congel,0,SEEK_SET); write(fd_congel,"1",2);
	    break;
	default:
	    lseek(fd_chauff,0,SEEK_SET); write(fd_chauff,"0",2);
	    lseek(fd_congel,0,SEEK_SET); write(fd_congel,"0",2);
    }
    etat_relais = mode;

    /* Fermeture des fichiers spéciaux */
    close(fd_chauff);
    close(fd_congel);

    /* Débloquage de la ressource */
    pthread_mutex_unlock(&mutex_rel);
}

/**
 * set_tprog - Définit la température programmée
 * @tprog: température programmée
 * L'appel peut blocker car utilise des ressources partagé
 */
static void set_tprog(int tprog)
{
    /* Vérification des limites */
    if (tprog >= TEMPARATURE_MAX || tprog <= TEMPARATURE_MIN) {
	return ;
    }

    /* Affectation et persistance */
    pthread_mutex_lock(&mutex_tprog);
    temp_programme = tprog;
    FILE *f_tprog = fopen(C_TEMP_PROG, "w");
    if (f_tprog) {
	fprintf(f_tprog, "%d", tprog);
	fclose(f_tprog);
    }
    pthread_mutex_unlock(&mutex_tprog);

    /* Mise a jour des relais */
    int tmes; GET_TMES(&tmes);
    if (tmes > tprog) update_relais(R_FROID);
    else if (tmes < tprog) update_relais(R_CHAUF);
    else update_relais(R_STOP);
}

/**
 * app_sonde - application pour la sonde
 * @arg: argument pour le thread
 * description: met à jour périodiquement (toutes les secondes) la température mesuré
 * reboot si la temperature est invalide.
 */
static void *app_sonde(void *arg)
{
    /* Initialisation des paramètres du thread */
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0) {
	syslog(LOG_CRIT|LOG_USER, "Erreur sonde: pthread_setcancelstate\n");
	AFF_WRITE("ES01");
	exit(EXIT_FAILURE);
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL) != 0) {
	syslog(LOG_CRIT|LOG_USER, "Erreur sonde: pthread_setcanceltype\n");
	AFF_WRITE("ES02");
	exit(EXIT_FAILURE);
    }
    sonde_init();

    /* Boucle principale */
    while (1) {
	/* Prise de la température */
	pthread_mutex_lock(&mutex_tmes);
	temp_mesure = sonde_get_temparature();
	if (!temp_mesure.success) break;
	int tmes = temp_mesure.value;
	pthread_mutex_unlock(&mutex_tmes);

	/* Mise a jour des relais */
	int tprog; GET_TPROG(&tprog);
	enum state_r etatr; GET_RSTATE(&etatr);
	if (tmes > (tprog + TEMPERATURE_MARGE)) {
	    update_relais(R_FROID);
	} else if (tmes < (tprog - TEMPERATURE_MARGE)) {
	    update_relais(R_CHAUF);
	} else if ((etatr==R_CHAUF && (tmes>=tprog)) || (etatr==R_FROID && (tmes<=tprog))) {
	    update_relais(R_STOP);
	}
	
	/* Afficher la temperature */
	if(pthread_mutex_trylock(&mutex_aff) == 0) {
	    /*FILE *f_aff = fopen(C_AFF, "w");
	    if (f_aff) {
		fprintf(f_aff, "%3dC", tmes);
		fclose(f_aff);
	    }*/
	    char mess[5];
	    sprintf(mess, "%3dC", tmes);
	    AFF_WRITE(mess);
	    pthread_mutex_unlock(&mutex_aff);
	}
	sleep(TIMEOUT_TEMP);
    }

    /* La temperature mesurée n'est pas valide */
    syslog(LOG_EMERG|LOG_USER, "Impossible de prendre la temperature, reboot!\n");
    exit_reboot();
    return NULL;
}

/**
 * app_boutons - application pour les boutons
 * @arg: argument pour le thread
 * description: détecte l'appui sur les boutons et règle la température programmé
 */
static void *app_boutons(void *arg)
{
    /* Initialisation des paramètres du thread */
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0) {
	syslog(LOG_CRIT|LOG_USER, "Erreur boutons: pthread_setcancelstate\n");
	AFF_WRITE("EP01");
	exit(EXIT_FAILURE);
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL) != 0) {
	syslog(LOG_CRIT|LOG_USER, "Erreur boutons: pthread_setcanceltype\n");
	AFF_WRITE("EP02");
	exit(EXIT_FAILURE);
    }

    /* Ouverture des fichiers spéciaux */
    int fd_bp = open(C_BT_UP, O_RDONLY);
    int fd_bm = open(C_BT_DOWN, O_RDONLY);
    if (fd_bp < 0 || fd_bm < 0) {
	syslog(LOG_CRIT|LOG_USER, "Impossible d'ouvrir les fichiers spéciaux pour les boutons\n");
	AFF_WRITE("EP03");
	exit(EXIT_FAILURE);
    }

    /* Set de descripteurs de fichiers */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_bp, &fds);
    FD_SET(fd_bm, &fds);
    int ndfs = (fd_bp > fd_bm) ? fd_bp+1 : fd_bm+1;

    /* Timeout */
    struct timeval timeout_ref;
    timeout_ref.tv_sec = TIMEOUT_AFF;
    timeout_ref.tv_usec = 0;
    struct timeval *time_out = NULL;
    time_t last_push = time(NULL) - TIMEOUT_AFF;

    /* Boucle principale */
    while (1) {
	FD_ZERO(&fds);
	FD_SET(fd_bp, &fds);
	FD_SET(fd_bm, &fds);
	ndfs = (fd_bp > fd_bm) ? fd_bp+1 : fd_bm+1;
	/* Attente passive */
	if (select(ndfs, NULL, NULL, &fds, time_out) < 0) {
	    syslog(LOG_CRIT|LOG_USER, "Erreur de select sur les boutons\n");
	    AFF_WRITE("EP04");
	    exit(EXIT_FAILURE);
	}

	/* Si appui sur un bouton */
	int fd;
	if (FD_ISSET(fd_bp,&fds) || FD_ISSET(fd_bm,&fds)) {
	    if (FD_ISSET(fd_bp,&fds)) fd=fd_bp;
	    else fd=fd_bm;
	    /* Lecture du fichier */
	    char buffer[2];
	    lseek(fd, 0, SEEK_SET);
	    if (read(fd, &buffer, 2) != 2) {
		syslog(LOG_CRIT|LOG_USER, "Erreur de lecture du bouton.\n");
		AFF_WRITE("EP05");
		exit(EXIT_FAILURE);
	    }

	    /* Si timeout non depasse */
	    if (time(NULL) < last_push + TIMEOUT_AFF) {
		/* Modification de la temperature programme*/
		if (buffer[0] == '0'){
		    int tprog; GET_TPROG(&tprog);
		    if (fd == fd_bp) tprog++;
		    else if (fd == fd_bm) tprog--;
		    set_tprog(tprog);
		}

	    }

	    /* Sinon, premier appui depuis le timeout */
	    else {
		/* On prend le verrou */
		pthread_mutex_lock(&mutex_aff);
	    }

	    /* Affichage de la température programmée */
	    int tprog; GET_TPROG(&tprog);
	    /*FILE *f_aff = fopen(C_AFF, "w");
	    if (f_aff) {
		fprintf(f_aff, "%3dP", tprog);
		fclose(f_aff);
	    }*/
	    char mess[5];
	    sprintf(mess, "%3dP", tprog);
	    AFF_WRITE(mess);

	    /* Réinitialisaion du timeout et du dernier appuis */
	    timeout_ref.tv_sec = TIMEOUT_AFF;
	    timeout_ref.tv_usec = 0;
	    time_out = &timeout_ref;
	    last_push = time(NULL);
	} 
	
	/* Il n'y a pas eu d'appuis sur un bouton */
	else {
	    /* Affichage de la mesure */
	    int tmes; GET_TMES(&tmes);
	    /*FILE *f_aff = fopen(C_AFF, "w");
	    if (f_aff) {
		fprintf(f_aff, "%3dC", tmes);
		fclose(f_aff);
	    }*/
	    char mess[5];
	    sprintf(mess, "%3dC", tmes);
	    AFF_WRITE(mess);


	    /* On libère le verrou */
	    pthread_mutex_unlock(&mutex_aff);

	    /* Définition du timeout à NULL */
	    time_out = NULL;
	}

	/* Attente "anti-rebond" des boutons */
	usleep(300);
    }
}

int main(void)
{
    /* Interception des signaux */
    atexit(parachute);
    signal(SIGHUP, exit);
    signal(SIGINT, exit);
    signal(SIGQUIT, exit);
    signal(SIGSTOP, exit);
    signal(SIGTERM, exit);
    signal(SIGABRT, exit);
    signal(SIGKILL, exit);

    /* Affiche un message de lancement */
    AFF_WRITE("----");

    /* Lecture de la température programée */
    FILE *f_tprog = fopen(C_TEMP_PROG, "r");
    if (f_tprog) {
	fscanf(f_tprog, "%d", &temp_programme);
	fclose(f_tprog);
    } else {
	syslog(LOG_CRIT|LOG_USER, "Impossible de lire le fichier de température\n");
	AFF_WRITE("EP07");
	exit(EXIT_FAILURE);
    }

    /* Initialisation des verrous */
    pthread_mutex_init(&mutex_aff, NULL);
    pthread_mutex_init(&mutex_rel, NULL);
    pthread_mutex_init(&mutex_tmes, NULL);
    pthread_mutex_init(&mutex_tprog, NULL);

    /* Démarrage du thread sonde */
    if (pthread_create(&thr_sonde, NULL, app_sonde, NULL) != 0) {
	syslog(LOG_CRIT|LOG_USER, "Impossible de créer le thread sonde.\n");
	AFF_WRITE("EP07");
	exit(EXIT_FAILURE);
    }
    
    /* Attente de la température mesuree */
    while (1) {
	pthread_mutex_lock(&mutex_tmes);
	if (temp_mesure.success) {
	    pthread_mutex_unlock(&mutex_tmes);
	    break;
	}
	pthread_mutex_unlock(&mutex_tmes);
	sleep(1);
    }

    /* Démarrage du thread Boutons */
    if (pthread_create(&thr_boutons, NULL, app_boutons, NULL) !=0) {
	syslog(LOG_CRIT|LOG_USER, "Impossible de créer le tread boutons.\n");
	AFF_WRITE("EP10");
	exit(EXIT_FAILURE);
    }

    pthread_join(thr_sonde, NULL);
    pthread_join(thr_boutons, NULL);

}
