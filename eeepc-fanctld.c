/*
 *   eeepc-fanctld
 *   Copyright 2012 Pau Oliva Fora <pof@eslack.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sensors/sensors.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>

#define VERSION "0.1"

#ifndef FALSE
#define FALSE   (0)
#endif

#ifndef TRUE
#define TRUE    (!FALSE)
#endif

/* update period in seconds */
#define POLLTIME 8

/* cpu temperature sensor */
#define CPU_TEMP "temp1"

#define HWMON_DIR	"/sys/devices/platform/eeepc/hwmon"
#define PWM_ID		"eeepc"

char base_path[PATH_MAX];
char fanctl[PATH_MAX];
char pwm1_enable[PATH_MAX];

typedef struct {
	const sensors_chip_name *chip_name_temp;
	const sensors_chip_name *chip_name_fan;
	unsigned int number_temp;
	unsigned int number_fan;
} sensor_data;

sensor_data sd;

int get_fan() {

	double val;
	sensors_get_value(sd.chip_name_fan, sd.number_fan, &val);
	return (int)val;
}

int set_fan(int value) {

	char buf[10];
	int fd;

	fd = open(fanctl, O_WRONLY);
	if (fd < 0 ) {
		fprintf(stderr,"ERROR: Could not write fan file: %s\n", fanctl);
		return FALSE;
	}

	sprintf( buf, "%d", value );
	if (write(fd, buf, strlen(buf)) < 1) {
		perror("Something wrong happening while writing fan file");
		close(fd);
		return FALSE;
	}
	close(fd);

	return TRUE;

}

int pwm_enable(int value) {

	char buf[10];
	int fd;

	fd = open(pwm1_enable, O_WRONLY);
	if (fd < 0 ) {
		fprintf(stderr,"Error: Could not write pwm file: %s\n", pwm1_enable);
		return FALSE;
	}

	sprintf( buf, "%d", value );
	if (write(fd, buf, strlen(buf)) < 1) {
		perror("Something wrong happening while writing pwm file");
		close(fd);
		return FALSE;
	}
	close(fd);

	return TRUE;

}

int get_temp() {

	double val;
	sensors_get_value(sd.chip_name_temp, sd.number_temp, &val);
	return (int)val;
}

int init_sensor_data() {

	const sensors_chip_name *chip_name;
	const sensors_feature *feature;
	const sensors_subfeature *subfeature;
	char *label;
	int fan_enabled=FALSE, temperature_enabled=FALSE;
	int a,b,c;

	a=0;
	while ( (chip_name = sensors_get_detected_chips(NULL, &a)) ) {

		b=0;
		while ( (feature = sensors_get_features(chip_name, &b)) ) {

			c=0;
			while ( (subfeature = sensors_get_all_subfeatures(chip_name, feature, &c)) ) {
				label = sensors_get_label(chip_name, feature);
				if ( strcmp(CPU_TEMP, label)==0 ) {
					printf ("Found sensor: %s (cpu temperature)\n", CPU_TEMP);
					sd.chip_name_temp=chip_name;
					sd.number_temp=subfeature->number;
					temperature_enabled=TRUE;
					break;
				}
				if (subfeature->type == SENSORS_SUBFEATURE_FAN_INPUT) {
					printf ("Found FAN sensor\n");
					sd.chip_name_fan=chip_name;
					sd.number_fan=subfeature->number;
					fan_enabled=TRUE;
					break;
				}
			}
		}
	}

	if (fan_enabled==TRUE && temperature_enabled==TRUE)
		return TRUE;
	return FALSE;
}

void find_eeepc()
{

	/* this function has been borrowed from macfanctld      */
	/* Copyright(C) 2010  Mikael Strom <mikael@sesamiq.com> */

	DIR *fd_dir;
	int ret;

	base_path[0] = 0;

	fd_dir = opendir(HWMON_DIR);

	if(fd_dir != NULL)
	{
		struct dirent *dir_entry;

		while((dir_entry = readdir(fd_dir)) != NULL && base_path[0] == 0)
		{
			if(dir_entry->d_name[0] != '.')
			{
				char name_path[PATH_MAX];
				int fd_name;

				sprintf(name_path, "%s/%s/name", HWMON_DIR, dir_entry->d_name);

				fd_name = open(name_path, O_RDONLY);

				if(fd_name > -1)
				{
					char name[sizeof(PWM_ID)];

					ret = read(fd_name, name, sizeof(PWM_ID) - 1);

					close(fd_name);

					if(ret == sizeof(PWM_ID) - 1)
					{
						if(strncmp(name, PWM_ID, sizeof(PWM_ID) - 1) == 0)
						{
							char *dev_path;
							char *last_slash = strrchr(name_path, '/');

							if(last_slash != NULL)
							{
								*last_slash = 0;

								dev_path = realpath(name_path, NULL);

								if(dev_path != NULL)
								{
									strncpy(base_path, dev_path, sizeof(base_path) - 1);
									base_path[sizeof(base_path) - 1] = 0;
									free(dev_path);
								}
							}
						}
					}
				}
			}
		}
		closedir(fd_dir);
	}

	if(base_path[0] == 0)
	{
		printf("Error: Can't find a eeepc device. Try booting with acpi_osi=\"Linux\".\n");
		exit(1);
	}

	sprintf(fanctl, "%s/pwm1", base_path);
	sprintf(pwm1_enable, "%s/pwm1_enable", base_path);

	printf("Found eeepc at %s\n", base_path);
}

void signal_handler(int sig) {

	(void) sig;
	// set automatic fan control before exit
	pwm_enable(2);
	printf("Killed!\n");
	exit(1);
}

void signal_installer() {
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGCHLD, signal_handler);
	signal(SIGABRT, signal_handler);
}

int main (int argc, char **argv) {

	int res, c, verbose=FALSE;
	int fan, temp, newpwm, oldpwm=0;
	pid_t pid;
	uid_t uid;

	printf("eeepc-fanctld v%s - (c)2012 Pau Oliva Fora <pof@eslack.org>\n",VERSION);

	while ((c = getopt(argc, argv, "vh?")) != EOF) {
		switch(c) {
			case 'v':
				verbose=TRUE;
				break;
			case 'h':
				printf("usage: eeepc-fanctld [-v]\n");
				return 0;
				break;
			default:
				fprintf(stderr,"usage: eeepc-fanctld [-v]\n");
				return 1;
				break;
		}
	}


	uid = getuid();
	if (uid != 0) {
		fprintf(stderr, "Error: this daemon must be run as root user.\n");
		return 1;
	}

	find_eeepc();

	sensors_init(NULL);
	if (!init_sensor_data()) {
		fprintf(stderr, "Error: Could not find needed sensors\n");
		return 1;
	}

	if (!pwm_enable(1)) {
		fprintf(stderr, "Error: could not enable pwm\n");
		return 1;
	}

	if (!verbose) {
		if ((pid = fork()) < 0) exit(1);
		else if (pid != 0) exit(0);
		/* daemon running here */
		setsid();
		res=chdir("/");
		if (res != 0) {
			perror("Error: Could not chdir");
			exit(1);
		}
		umask(0);
	} 

	signal_installer();

	while(1) {

		temp=get_temp();

		if (verbose) {
			fan=get_fan();
			printf("FAN: %d RPM, TEMP: %dºC\n", fan, temp);
		}

		if (temp < 55) newpwm=0;
		else if(temp < 60) newpwm=(10*255)/100;
		else if(temp < 65) newpwm=(20*255)/100;
		else if(temp < 67) newpwm=(30*255)/100;
		else if(temp < 69) newpwm=(40*255)/100;
		else if(temp < 74) newpwm=(50*255)/100;
		else if(temp < 77) newpwm=(60*255)/100;
		else if(temp < 81) newpwm=(65*255)/100;
		else if(temp < 85) newpwm=(70*255)/100;
		else if(temp < 91) newpwm=(80*255)/100;
		else newpwm=255;

		if (newpwm != oldpwm) {
			if(verbose) printf("Changing fan speed from: %d to %d\n", oldpwm, newpwm);
			set_fan(newpwm);
			oldpwm=newpwm;
		}

		usleep(POLLTIME*1000000);

	}

	return 0;
}
