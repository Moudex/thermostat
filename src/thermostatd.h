#ifndef _THERMOSTATD_H
#define _THERMOSTATD_H	1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>	/* execv, write, read */
#include <pthread.h>	/* thread, mutex */
#include <time.h>	/* time */
#include <fcntl.h>	/* open */
#include <sys/types.h>	/* open, select */
#include <sys/time.h>	/* timeval */
#include <sys/stat.h>	/* open */
#include <syslog.h>	/* syslog */
#include <signal.h>	/* signal */

#include "sonde.h"

#define TEMPARATURE_MAX	    (70)
#define TEMPARATURE_MIN	    (0-30)
#define TEMPERATURE_MARGE   (1)

#define TIMEOUT_TEMP	(1)
#define TIMEOUT_AFF	(4)

#define C_TEMP_PROG	"/opt/thermostatd/temp_prog"
#define C_AFF		"/dev/aff4x7seg"
#define C_CHAUFF	"/sys/class/gpio/gpio20/value"
#define C_CONGEL	"/sys/class/gpio/gpio21/value"
#define C_BT_UP		"/sys/class/gpio/gpio16/value"
#define C_BT_DOWN	"/sys/class/gpio/gpio26/value"


/* Types */
enum state_r {R_CHAUF, R_FROID, R_STOP };

/**
 * AFF_WRITE_SECURE - Ecris un message sur l'afficheur
 * @message: message à afficher
 * Cet appel peut bloquer car utilise une ressource partagée avec un mutex.
 */
#define AFF_WRITE_SECURE(message) \
    do {						    \
	pthread_mutex_lock(&mutex_aff);			    \
	int __fd_aff = open("/dev/aff4x7seg", O_WRONLY);    \
	write(__fd_aff, message, strlen(message)+1);	    \
	close(__fd_aff);				    \
	pthread_mutex_unlock(&mutex_aff);		    \
    } while(0)

/** AFF_WRITE - Ecris un message sur l'afficheur
 * @message: message à afficher
 * Cet appel n'est pas sécurisé, à utiliser seulement pour afficher des codes d'erreurs.
 */
#ifndef AFF_WRITE
#define AFF_WRITE(message) \
    do {						    \
	int __fd_aff = open("/dev/aff4x7seg", O_WRONLY);    \
	write(__fd_aff, message, strlen(message)+1);	    \
	close(__fd_aff);				    \
    } while(0)
#endif

/**
 * GET_TMES - Accesseur sécurisé sur la température mesurée
 * @mest: pointeur résultat
 */
#define GET_TMES(mest) \
    do {						    \
	pthread_mutex_lock(&mutex_tmes);		    \
	*mest = temp_mesure.value;			    \
	pthread_mutex_unlock(&mutex_tmes);		    \
    } while(0)

/**
 * GET_TPROG - Accesseur sécurisée sur la température programmé
 * @progt: pointeur résultat
 */
#define GET_TPROG(progt) \
    do {						    \
	pthread_mutex_lock(&mutex_tprog);		    \
	*progt = temp_programme;			    \
	pthread_mutex_unlock(&mutex_tprog);		    \
    } while(0)

/**
 * GET_RSTATE - Accesseur sécurisé sur l'etat des relais
 * @rstate: pointeur resultat
 */
#define GET_RSTATE(rstate) \
    do {						    \
	pthread_mutex_lock(&mutex_rel);			    \
	*rstate = etat_relais;				    \
	pthread_mutex_unlock(&mutex_rel);		    \
    } while(0)


#endif
