/*
 * gpio.h
 *
 *  Created on: 15-Oct-2022
 *     Author: Praveen
 */

#ifndef GPIO_DRIVER_H_
#define GPIO_DRIVER_H_


#define SOME_BYTES          100

/* This is the path corresponds to GPIOs in the 'sys' directory */
#define SYS_GPIO_PATH       "/sys/class/gpio"

// Output Pins; LED
/* This is the path corresponds to USER LEDS in the 'sys' directory */
#define SYS_FS_LEDS_PATH 	"/sys/class/leds"
#define SYS_FS_ADC_PATH		"/sys/bus/iio/devices/iio:device0"

#define HIGH_VALUE          1
#define LOW_VALUE           0

#define GPIO_DIR_OUT        HIGH_VALUE
#define GPIO_DIR_IN         LOW_VALUE

#define GPIO_LOW_VALUE      LOW_VALUE
#define GPIO_HIGH_VALUE     HIGH_VALUE

//public function prototypes .
int gpio_export(uint32_t gpio_num);
int gpio_configure_dir(uint32_t gpio_num, uint8_t dir_value);
int gpio_write_value(uint32_t gpio_num, uint8_t out_value);
uint8_t gpio_read_value(uint32_t gpio_num);
int gpio_configure_edge(uint32_t gpio_num, char *edge);
int gpio_file_open(uint32_t gpio_num);
int gpio_file_close(int fd);
int write_trigger_values(uint8_t led_no, char *value);
uint16_t adc_read_value(uint32_t adc_num);

#endif /* GPIO_DRIVER_H_ */
