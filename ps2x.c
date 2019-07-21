/**
 * @file max7219.c
 *
 * ESP-IDF driver for MAX7219/MAX7221
 * Serially Interfaced, 8-Digit LED Display Drivers
 *
 * Ported from esp-open-rtos
 * Copyright (C) 2017, 2018 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ps2x.h"
#include <string.h>
//#include <freertos/FreeRTOS.h>
//#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "ps2x";

#define CLOCK_SPEED_HZ (100000) 

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

static uint8_t enter_config[]={0x01,0x43,0x00,0x01,0x00};
static uint8_t set_mode[]={0x01,0x44,0x00,0x01,0x03,0x00,0x00,0x00,0x00};
static uint8_t set_bytes_large[]={0x01,0x4F,0x00,0xFF,0xFF,0x03,0x00,0x00,0x00};
static uint8_t exit_config[]={0x01,0x43,0x00,0x00,0x5A,0x5A,0x5A,0x5A,0x5A};
static uint8_t enable_rumble[]={0x01,0x4D,0x00,0x00,0x01};
static uint8_t type_read[]={0x01,0x45,0x00,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A};

static bool en_Rumble = false;
static bool en_Pressures = false;
static uint8_t controller_type;

//static gpio_num_t PS2X_CS_PIN;

esp_err_t PS2X_init_device(PS2X_t *dev, spi_host_device_t host, gpio_num_t cs_pin)
{
    CHECK_ARG(dev);

    memset(&dev->spi_cfg, 0, sizeof(dev->spi_cfg));
    dev->spi_cfg.spics_io_num = cs_pin;
    dev->spi_cfg.clock_speed_hz = CLOCK_SPEED_HZ;
    dev->spi_cfg.mode = 2;
    dev->spi_cfg.queue_size = 1;
    dev->spi_cfg.flags = SPI_DEVICE_NO_DUMMY;

/*
    PS2X_CS_PIN = cs_pin;
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<PS2X_CS_PIN);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_level(PS2X_CS_PIN, 1);
*/
    return spi_bus_add_device(host, &dev->spi_cfg, &dev->spi_dev);
}

esp_err_t PS2X_init(PS2X_t *dev)
{
    CHECK_ARG(dev);

    return ESP_OK;
}

static esp_err_t sendCommandString(PS2X_t *dev, uint8_t* tx_buffer, uint8_t* rx_buffer, uint8_t len) {
   spi_transaction_t t;

/*   
   gpio_set_level(PS2X_CS_PIN, 0);
   for (uint8_t index = 0; index < len; index++) {
      memset(&t, 0, sizeof(t));
      t.length = 8;
      t.tx_buffer = tx_buffer + index;
      t.rx_buffer = ((rx_buffer == NULL) ? (NULL) : (rx_buffer + index));
      CHECK(spi_device_transmit(dev->spi_dev, &t));
   }
   gpio_set_level(PS2X_CS_PIN, 1);
*/
   memset(&t, 0, sizeof(t));
   t.length = len * 8;
   t.tx_buffer = tx_buffer;
   t.rx_buffer = rx_buffer;
   CHECK(spi_device_transmit(dev->spi_dev, &t));
   return ESP_OK;
}

void PS2X_set_rumble(bool rumble){
  en_Rumble = rumble;
}

void PS2X_set_pressure(bool pressure){
  en_Pressures = pressure;
}

esp_err_t PS2X_reconfig_gamepad(PS2X_t *dev){
  CHECK(sendCommandString(dev, enter_config, NULL, sizeof(enter_config)));
  CHECK(sendCommandString(dev, set_mode, NULL, sizeof(set_mode)));
  if (en_Rumble)
    CHECK(sendCommandString(dev, enable_rumble, NULL, sizeof(enable_rumble)));
  if (en_Pressures)
    CHECK(sendCommandString(dev, set_bytes_large, NULL, sizeof(set_bytes_large)));
  CHECK(sendCommandString(dev, exit_config, NULL, sizeof(exit_config)));
  return ESP_OK;
}

esp_err_t PS2X_read_gamepad(PS2X_t *dev, PS2X_gamepad_t* ps2x_data, uint8_t motor1, uint8_t motor2) {

//   if(motor2 != 0x00)
//      motor2 = map(motor2,0,255,0x40,0xFF); //noting below 40 will make it spin

   uint8_t DO_data[9] = {0x01, 0x42, 0, motor1, motor2, 0, 0, 0, 0};
   esp_err_t tmp_err_code;

   tmp_err_code = sendCommandString(dev, DO_data, (uint8_t *)ps2x_data, sizeof(DO_data)); //spi_device_transmit(dev->spi_dev, &t);
 
   if (tmp_err_code != ESP_OK) {
      ESP_LOGE(TAG, "spi_device_transmit error");
      return ESP_FAIL;
   }

   if(ps2x_data->data_start != 0x5A) {  
      ESP_LOGE(TAG, "Invalid start flag 0x%02X", ps2x_data->data_start);
      PS2X_reconfig_gamepad(dev);
      return ESP_FAIL;
   }

   if((ps2x_data->ID != 0x41) && (ps2x_data->ID != 0x73)) {  
      ESP_LOGE(TAG, "Invalid ID 0x%02X", ps2x_data->ID);
      PS2X_reconfig_gamepad(dev);
      return ESP_FAIL;
   }

   return ESP_OK;
}

esp_err_t PS2X_config_gamepad(PS2X_t *dev){
  uint8_t temp[sizeof(type_read)];
  PS2X_gamepad_t ps2x_data;

  //new error checking. First, read gamepad a few times to see if it's talking
  PS2X_read_gamepad(dev, &ps2x_data, 0, 0);
  PS2X_read_gamepad(dev, &ps2x_data, 0, 0);

  //see if it talked - see if mode came back. 
  //If still anything but 41, 73 or 79, then it's not talking
  if(ps2x_data.ID != 0x41 && ps2x_data.ID != 0x73 && ps2x_data.ID != 0x79){ 

    ESP_LOGE(TAG, "Controller mode not matched or no controller found");
    ESP_LOGE(TAG, "Expected 0x41, 0x73 or 0x79, but got 0x%02X.", ps2x_data.ID);
    return ESP_FAIL; 
  }

  for(int y = 0; y <= 10; y++) {
    CHECK(sendCommandString(dev, enter_config, NULL, sizeof(enter_config)));
    CHECK(sendCommandString(dev, type_read, temp, sizeof(type_read)));
    controller_type = temp[3];

    CHECK(sendCommandString(dev, set_mode, NULL, sizeof(set_mode)));
    if (en_Rumble)
      CHECK(sendCommandString(dev, enable_rumble, NULL, sizeof(enable_rumble)));
    if (en_Pressures)
      CHECK(sendCommandString(dev, set_bytes_large, NULL, sizeof(set_bytes_large)));
    CHECK(sendCommandString(dev, exit_config, NULL, sizeof(exit_config)));

    PS2X_read_gamepad(dev, &ps2x_data, 0, 0);

    if(en_Pressures){
      if(ps2x_data.ID == 0x79)
        break;
      if(ps2x_data.ID == 0x73)
        return ESP_FAIL;
    }

    if(ps2x_data.ID == 0x73)
      break;

    if(y == 10){
      ESP_LOGE(TAG, "Controller not accepting commands");
      ESP_LOGE(TAG, "mode stil set at 0x%02X.", ps2x_data.ID);
      return ESP_FAIL; //exit function with error
    }
  }

  return ESP_OK;
}


