#include "sonde.h"
#include <syslog.h>

static int fd_sonde;

/* Initialise la sonde */
void sonde_init(void)
{
    /* Obtention de l'accès au bus i2c */
    fd_sonde = open(DS1621_I2C_BUS, O_RDWR);
    if (fd_sonde < 0) {
	syslog(LOG_CRIT|LOG_USER,"Erreur d'accès au bus i2c.\n" DS1621_I2C_BUS);
	AFF_WRITE("ES01");
	exit(EXIT_FAILURE);
    }

    /* Fixe l'adresse esclave avec qui communiquer */
    if (ioctl(fd_sonde, I2C_SLAVE, DS1621_SLAVE_ADDR) < 0) {
	syslog(LOG_CRIT|LOG_USER,"Esclave introuvable.\n");
	AFF_WRITE("ES02");
	exit(EXIT_FAILURE);
    }

    /* Lecture de la configuration */
    int config = i2c_smbus_read_byte_data(fd_sonde, DS1621_ACCESS_CONFIG);
    if (config < 0) {
	syslog(LOG_CRIT|LOG_USER,"Impossible de lire la configuration de la sonde.\n");
	AFF_WRITE("ES03");
	exit(EXIT_FAILURE);
    }

    /* Active le mode one shot */
    config |= DS1621_ONE_SHOT;

    /* Réécriture de la configuration */
    if (i2c_smbus_write_byte_data(fd_sonde, DS1621_ACCESS_CONFIG, config) < 0) {
	syslog(LOG_CRIT|LOG_USER,"Impossible d'écrire la configuration.\n");
	AFF_WRITE("ES04");
	exit(EXIT_FAILURE);
    }
}

/* Prend la température */
temperature sonde_get_temparature(void)
{
    temperature temp_err = {.success = 0, .value = 0};
    
    /* Démarre l'acquisition */
    if (i2c_smbus_write_byte(fd_sonde, DS1621_START_CONVERT) < 0) {
	syslog(LOG_CRIT|LOG_USER, "Impossible de demarrer l'acquisition.\n");
	AFF_WRITE("ESO5");
	return temp_err;
    }

    int value;
    int attempts = 0;

    /* Attend que l'acquisition soit terminée */
    while (1) {
	value = i2c_smbus_read_byte_data(fd_sonde, DS1621_ACCESS_CONFIG);
	if (value < 0) {
	    syslog(LOG_CRIT|LOG_USER, "Attente acqusition: Impossible de lire la configuration\n");
	    AFF_WRITE("ES06");
	    return temp_err;
	}

	if ((value & DS1621_DONE) == 0) {
	    attempts++;
	    if (attempts > 20) {
		syslog(LOG_CRIT|LOG_USER, "Attente acqusition: Impossible de lire la température\n");
		AFF_WRITE("ESO7");
		return temp_err;
	    }
	} else {
	    break;
	}
	sleep(1);
    }

    /* Lecture de la température */
    value = i2c_smbus_read_byte_data(fd_sonde, DS1621_READ_TEMPERATURE);
    if (value < 0) {
	syslog(LOG_CRIT|LOG_USER, "Erreur de lecture de la température\n");
	AFF_WRITE("ES08");
	return temp_err;
    }

    if (value > 127) {
	value -= 256;
    }

    return (temperature){1, value};
}
