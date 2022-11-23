/*
 ============================================================================
 Name        : gpio.c
 Version     : 1.0
 Copyright   : Your copyright notice
 Description : simple gpio file handling functions

 ============================================================================
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<string.h>

#include "gpio.h"


/************************************ UTILITIES ***************************************/
int StringToInt (char *s, int n)
{
    int num = 0;
    int i = 0;

    // Iterate till length of the string
    for (i = 0; ((i < n) & (s[i] != '\0')); i++)
    {
        num = num * 10 + (s[i] - 48);
    }

    return (num);
}


/*
 *  GPIO export pin
 *
 */
int gpio_export(uint32_t gpio_num)
{
	int fd, len;
	char buf[SOME_BYTES];

	fd = open(SYS_GPIO_PATH "/export", O_WRONLY);
	if (fd < 0) {
		perror(" error opening export file\n");
		return fd;
	}

	len = snprintf(buf, sizeof(buf), "%d", gpio_num);
	write(fd, buf, len);
	close(fd);

	return 0;
}

/*
 *  GPIO configure direction
 *  dir_value : 1 means 'out' , 0 means "in"
 */
int gpio_configure_dir(uint32_t gpio_num, uint8_t dir_value)
{
    int fd;
    char buf[SOME_BYTES];

    snprintf(buf, sizeof(buf), SYS_GPIO_PATH "/gpio%d/direction", gpio_num);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio direction configure\n");
        return fd;
    }

    if (dir_value)
        write(fd, "out", 4); //3+1  +1 for NULL character
    else
        write(fd, "in", 3);

    close(fd);
    return 0;
}

/*
 *  GPIO write value
 *  out_value : can be either 0 or 1
 */
int gpio_write_value(uint32_t gpio_num, uint8_t out_value)
{
    int fd;
    char buf[SOME_BYTES];

    snprintf(buf, sizeof(buf), SYS_GPIO_PATH "/gpio%d/value", gpio_num);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio write value\n");
        return fd;
    }

    if (out_value)
        write(fd, "1", 2);
    else
        write(fd, "0", 2);

    close(fd);
    return 0;
}

/*
 *  GPIO read value
 */
uint8_t gpio_read_value(uint32_t gpio_num)
{
    int fd;
    uint8_t read_value=0;
    char buf[SOME_BYTES];

    snprintf(buf, sizeof(buf), SYS_GPIO_PATH "/gpio%d/value", gpio_num);
    //printf ("\n\rReading from file %s", buf);
    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        perror("gpio read value\n");
        return fd;
    }

    read(fd, &read_value, 1);

    close(fd);
    return (read_value - 0x30);
}


/*
 *  GPIO read value
 */
uint16_t adc_read_value(uint32_t adc_num)
{
    int fd;
    uint16_t analogValue = 0;
    char readBuff[8];
    char buf[SOME_BYTES];

    memset (readBuff, '\0', sizeof(readBuff));
    snprintf(buf, sizeof(buf), SYS_FS_ADC_PATH "/in_voltage%d_raw", adc_num);
    //printf ("\n\rReading ADC value from file %s", buf);
    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        perror("adc read value\n");
        return fd;
    }

    read(fd, &readBuff, 4);
    //analogValue = atoi(readBuff);

    analogValue = StringToInt (readBuff, 8);
    //printf ("\n\rADC Value = %s", readBuff);

    close(fd);
    return (analogValue);
}

/*
 *  GPIO configure the edge of trigger
 *  edge : rising, falling
 */
int gpio_configure_edge(uint32_t gpio_num, char *edge)
{
    int fd;
    char buf[SOME_BYTES];

    snprintf(buf, sizeof(buf), SYS_GPIO_PATH "/gpio%d/edge", gpio_num);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio configure edge\n");
        return fd;
    }

    write(fd, edge, strlen(edge) + 1);
    close(fd);
    return 0;
}

/*
 *  Open the sys fs file corresponding to gpio number
 */
int gpio_file_open(uint32_t gpio_num)
{
    int fd;
    char buf[SOME_BYTES];

    snprintf(buf, sizeof(buf), SYS_GPIO_PATH "/gpio%d/value", gpio_num);

    fd = open(buf, O_RDONLY | O_NONBLOCK );
    if (fd < 0) {
        perror("gpio file open\n");
    }
    return fd;
}

/*
 *  close a file
 */
int gpio_file_close(int fd)
{
    return close(fd);
}

/*This function writes the tigger values for the given "led_no"
 * returns 0 if success, else -1
 */
int write_trigger_values(uint8_t led_no, char *value)
{
	int fd,ret=0;
	char buf[SOME_BYTES];

	/* we are concatenating  2 strings and storing that in to 'buf'  */
	snprintf(buf,sizeof(buf),SYS_FS_LEDS_PATH "/beaglebone:green:usr%d/trigger",led_no);

	/* open the file in write mode */
	/*Returns the file descriptor for the new file. The file descriptor returned is always the smallest integer greater than zero that is still available. If a negative value is returned, then there was an error opening the file.*/
	fd = open(buf, O_WRONLY );
	if (fd <= 0) {
		perror(" write trigger error\n");
		return -1;
	}

	/* Write the 'value' in to the file designated by 'fd' */
	/*Returns the number of bytes that were written.
	  If value is negative, then the system call returned an error.
	*/
	ret = write(fd, (void*)value, strlen(value) );
	if ( ret <= 0)
	{
		printf("trigger value write error\n");
		return -1;
	}

	close(fd);

	return 0;

}
