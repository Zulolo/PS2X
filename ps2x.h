/**
 * @file max7219.h
 *
 * ESP-IDF driver for MAX7219/MAX7221
 * Serially Interfaced, 8-Digit LED Display Drivers
 *
 * Ported from esp-open-rtos
 * Copyright (C) 2017, 2018 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef __PS2X_H__
#define __PS2X_H__

#include <stdint.h>
#include <stdbool.h>
#include <driver/spi_master.h>
#include <driver/gpio.h> // add by nopnop2002

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    spi_device_interface_config_t spi_cfg;
    spi_device_handle_t spi_dev;
} PS2X_t;

typedef struct {
    uint8_t idle;
    uint8_t ID;
    uint8_t data_start;
    struct {
    	uint8_t SELECT : 1;
    	uint8_t L3 : 1;
    	uint8_t R3 : 1;
    	uint8_t START : 1;
    	uint8_t UP : 1;
    	uint8_t RIGHT : 1;
    	uint8_t DOWN : 1;
    	uint8_t LEFT : 1;
    } button_dir;
    struct {
    	uint8_t L2 : 1;
    	uint8_t R2 : 1;
    	uint8_t L1 : 1;
    	uint8_t R1 : 1;
    	uint8_t TRIAN : 1;
    	uint8_t CIRCLE : 1;
    	uint8_t CROSS : 1;
    	uint8_t RECT : 1;
	} button_act;
    uint8_t PSS_RX;
    uint8_t PSS_RY;
    uint8_t PSS_LX;
    uint8_t PSS_LY;
} PS2X_gamepad_t ;

/**
 * Initialize device descriptor
 * @param dev Device descriptor
 * @param host SPI host
 * @param cs_pin CS GPIO number
 * @return `ESP_OK` on success
 */
esp_err_t PS2X_init_device(PS2X_t *dev, spi_host_device_t host, gpio_num_t cs_pin);

/**
 * @brief Initialize gamepad
 * Switch it to normal operation from shutdown mode,
 * set scan limit to the max and clear
 * @param dev Display descriptor
 * @return `ESP_OK` on success
 */
esp_err_t PS2X_init(PS2X_t *dev);

esp_err_t PS2X_read_gamepad(PS2X_t *dev, PS2X_gamepad_t* ps2x_data, uint8_t motor1, uint8_t motor2);

esp_err_t PS2X_config_gamepad(PS2X_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* __PS2X_H__ */
