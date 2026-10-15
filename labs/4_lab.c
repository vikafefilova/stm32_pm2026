#include "stm32/stm32f10x.h"
#include <stdint.h>

static void delay(volatile uint32_t n) { while (n--) { __NOP(); } }

void SPI1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE5 | GPIO_CRL_CNF5 | GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
    GPIOA->CRL |=  (GPIO_CRL_MODE5 | GPIO_CRL_MODE7);

    GPIOA->ODR &= ~(GPIO_ODR_ODR5 | GPIO_ODR_ODR7);
}

void SPI1_Write(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) {
            GPIOA->ODR |= GPIO_ODR_ODR7;
        } else {
            GPIOA->ODR &= ~GPIO_ODR_ODR7;
        }
        data <<= 1;

        GPIOA->ODR |= GPIO_ODR_ODR5;
        GPIOA->ODR &= ~GPIO_ODR_ODR5;
    }
}

static void display_cmd(uint8_t cmd)
{
    GPIOA->ODR &= ~GPIO_ODR_ODR1;
    GPIOA->ODR &= ~GPIO_ODR_ODR4;
    SPI1_Write(cmd);
    GPIOA->ODR |= GPIO_ODR_ODR4;
}

static void display_data(uint8_t data)
{
    GPIOA->ODR |= GPIO_ODR_ODR1;
    GPIOA->ODR &= ~GPIO_ODR_ODR4;
    SPI1_Write(data);
    GPIOA->ODR |= GPIO_ODR_ODR4;
}

static void gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
                    GPIO_CRL_MODE2 | GPIO_CRL_CNF2 |
                    GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOA->CRL |=  (GPIO_CRL_MODE1 | GPIO_CRL_MODE2 | GPIO_CRL_MODE4);
    GPIOA->ODR |= GPIO_ODR_ODR4 | GPIO_ODR_ODR2;
}

static void display_init(void)
{
    GPIOA->ODR &= ~GPIO_ODR_ODR2;
    delay(1000);
    GPIOA->ODR |= GPIO_ODR_ODR2;
    delay(1000);

    display_cmd(0xAE); 
    display_cmd(0x20); display_cmd(0x00);                
    display_cmd(0x21); display_cmd(0); display_cmd(127); 
    display_cmd(0x22); display_cmd(0); display_cmd(7);  
    display_cmd(0x81); display_cmd(0x7F);            
    display_cmd(0xA1);         
    display_cmd(0xA8); display_cmd(0x3F);               
    display_cmd(0xC8);           
    display_cmd(0xD3); display_cmd(0x00);              
    display_cmd(0xD5); display_cmd(0x80);              
    display_cmd(0xD9); display_cmd(0xF1);             
    display_cmd(0xDA); display_cmd(0x12);             
    display_cmd(0xDB); display_cmd(0x40);               
    display_cmd(0x8D); display_cmd(0x14);              
    display_cmd(0xA4);             
    display_cmd(0xA6);              
    display_cmd(0xAF); 
}

static void display_chessboard(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        display_cmd(0xB0 + page);
        display_cmd(0x00);
        display_cmd(0x10);
        for (uint8_t col = 0; col < 128; col++) {
            uint8_t cell_x = col / 8;
            display_data(((page + cell_x) & 1) ? 0xFF : 0x00);
        }
    }
}

int main(void)
{
    gpio_init();
    SPI1_Init();
    display_init();
    display_chessboard();
    while (1) { }
}