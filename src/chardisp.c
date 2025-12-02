#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "chardisp.h"


// Make sure to set these in main.c
extern const int SPI_DISP_SCK; extern const int SPI_DISP_CSn; extern const int SPI_DISP_TX;

/***************************************************************** */

// "chardisp" stands for character display, which can be an LCD or OLED
void init_chardisp_pins() {
    gpio_set_function(SPI_DISP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_TX,  GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_CSn, GPIO_FUNC_SPI);

    spi_init(spi0, 10000);
    spi_set_format(spi0, 9, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    // fill in
}

void send_spi_cmd(spi_inst_t* spi, uint16_t value) {
    uint16_t mask = 0x01FFu; //9 bits
    value &= mask;

        while (!spi_is_writable(spi)){

        }
        while(spi_is_busy(spi)){

        }

        spi_get_hw(spi)->dr = value;

    // fill in
}

void send_spi_data(spi_inst_t* spi, uint16_t value) {
    uint16_t data = 0x0100u;
    send_spi_cmd(spi, (value & 0xFFu) | data);
    // fill in
}

void cd_init() {
    sleep_ms(1);

    send_spi_cmd(spi0, 0b00111100);
    sleep_us(40);


    send_spi_cmd(spi0, 0b00001100);
    sleep_us(40);

    send_spi_cmd(spi0, 0b00000001);
    sleep_ms(2);

    send_spi_cmd(spi0, 0b00000110);
    sleep_us(40);
    // fill in
}

void cd_display1(const char *str) {
    send_spi_cmd(spi0, 0x80 | 0x00);

    for (int i = 0; i < 16 && str[i] != '\0'; ++i) {
        send_spi_data(spi0, (uint8_t)str[i]);
    }
}

void cd_display2(const char *str) {
    send_spi_cmd(spi0, 0x80 | 0x40);

    for (int i = 0; i < 16 && str[i] != '\0'; ++i) {
        send_spi_data(spi0, (uint8_t)str[i]);
    }
}

/***************************************************************** */