#ifndef SONDE_H
#define SONDE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define DS1621_I2C_BUS		"/dev/i2c-1"
#define DS1621_SLAVE_ADDR	0x4F
#define DS1621_ACCESS_CONFIG	0xAC
#define DS1621_START_CONVERT	0xEE
#define DS1621_STOP_CONVERT	0x22
#define DS1621_READ_TEMPERATURE	0xAA
#define DS1621_ONE_SHOT		0x01
#define DS1621_DONE		0x80

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

typedef struct {
    char success;
    int value;
} temperature;

/* Initialise la sonde */
void sonde_init(void);

/* Prend la température */
temperature sonde_get_temparature(void);

#endif
