#include "stm32/stm32f10x.h"
#include <stdint.h>

#define CS_PIN   4 
#define A0_PIN   0   
#define RST_PIN  1   

#define CS_LOW()   (GPIOA->BSRR = (1u << (CS_PIN + 16)))
#define CS_HIGH()  (GPIOA->BSRR = (1u << CS_PIN))
#define A0_CMD()   (GPIOB->BSRR = (1u << (A0_PIN + 16)))
#define A0_DATA()  (GPIOB->BSRR = (1u << A0_PIN))
#define RST_LOW()  (GPIOB->BSRR = (1u << (RST_PIN + 16)))
#define RST_HIGH() (GPIOB->BSRR = (1u << RST_PIN))


#define F_TIMER  72000000UL
#define TIM_PSC  ((F_TIMER / 1000UL) - 1)  
#define TIM_ARR  (15000 - 1)              


static const uint8_t digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, 
    {0x00, 0x42, 0x7F, 0x40, 0x00}, 
    {0x42, 0x61, 0x51, 0x49, 0x46}, 
    {0x21, 0x41, 0x45, 0x4B, 0x31}, 
    {0x18, 0x14, 0x12, 0x7F, 0x10}, 
    {0x27, 0x45, 0x45, 0x45, 0x39}, 
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03}, 
    {0x36, 0x49, 0x49, 0x49, 0x36}, 
    {0x06, 0x49, 0x49, 0x29, 0x1E}  
};

#define SCALE      4    
#define DIGIT_COL  54  
#define DIGIT_PAGE 2   

static volatile uint8_t counter = 0;

static void spi_send(uint8_t b)
{
    while (!(SPI1->SR & SPI_SR_TXE)) {}
    SPI1->DR = b;
    while (SPI1->SR & SPI_SR_BSY) {}
}

static void cmd(uint8_t c)
{
    A0_CMD();
    CS_LOW();
    spi_send(c);
    CS_HIGH();
}

static void dat(uint8_t d)
{
    A0_DATA();
    CS_LOW();
    spi_send(d);
    CS_HIGH();
}

static void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 800; i++) { __asm volatile ("nop"); }
}

static void set_pos(uint8_t page, uint8_t col)
{
    cmd(0xB0 | page);           
    cmd(0x10 | (col >> 4));    
    cmd(0x00 | (col & 0x0F)); 
}

static void display_clear(void)
{
    for (int p = 0; p < 8; p++) {
        set_pos(p, 0);
        for (int i = 0; i < 128; i++) dat(0x00);
    }
}

static void display_init(void)
{
    RST_LOW();  delay_ms(10);
    RST_HIGH(); delay_ms(10);

    cmd(0xAE);             
    cmd(0xD5); cmd(0x80);   
    cmd(0xA8); cmd(0x3F);   
    cmd(0xD3); cmd(0x00);  
    cmd(0x40);             
    cmd(0x8D); cmd(0x14);   
    cmd(0x20); cmd(0x02);   
    cmd(0xA1);              
    cmd(0xC8);             
    cmd(0xDA); cmd(0x12);   
    cmd(0x81); cmd(0xCF);  
    cmd(0xD9); cmd(0xF1);
    cmd(0xDB); cmd(0x40);
    cmd(0xA4);              
    cmd(0xA6);             
    cmd(0xAF);             
}

static uint32_t expand_column(uint8_t col)
{
    uint32_t v = 0;
    for (int bit = 0; bit < 7; bit++) {
        if (col & (1u << bit)) {
            v |= ((1u << SCALE) - 1u) << (bit * SCALE);
        }
    }
    return v;
}

static void draw_digit(uint8_t n)
{
    for (int page = 0; page < 4; page++) {        
        set_pos(DIGIT_PAGE + page, DIGIT_COL);
        for (int c = 0; c < 5; c++) {
            uint8_t b = (uint8_t)(expand_column(digits[n][c]) >> (8 * page));
            for (int k = 0; k < SCALE; k++) dat(b); 
        }
        for (int k = 0; k < SCALE; k++) dat(0x00);  
    }
}

static void gpio_spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_SPI1EN;

    GPIOA->CRL &= ~((0xFu << 16) | (0xFu << 20) | (0xFu << 28));
    GPIOA->CRL |=  ((0x3u << 16) | (0xBu << 20) | (0xBu << 28));

    GPIOB->CRL &= ~((0xFu << 0) | (0xFu << 4));
    GPIOB->CRL |=  ((0x3u << 0) | (0x3u << 4));

    CS_HIGH();

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1;
    SPI1->CR1 |= SPI_CR1_SPE;
}

static void timer1_init(void)
{
    RCC->APB2ENR  |= RCC_APB2ENR_TIM1EN;
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;    
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;

    TIM1->PSC = TIM_PSC;
    TIM1->ARR = TIM_ARR;

    TIM1->EGR |= TIM_EGR_UG;                  
    TIM1->SR  &= ~TIM_SR_UIF;                

    TIM1->DIER |= TIM_DIER_UIE;              
    NVIC_ClearPendingIRQ(TIM1_UP_IRQn);
    NVIC_EnableIRQ(TIM1_UP_IRQn);              

    TIM1->CR1 |= TIM_CR1_CEN;                 
}

void TIM1_UP_IRQHandler(void)
{
    if (TIM1->SR & TIM_SR_UIF) {
        TIM1->SR &= ~TIM_SR_UIF;               

        counter = (counter + 1) % 10;          
        draw_digit(counter);
    }
}

int main(void)
{
    gpio_spi_init();
    display_init();
    display_clear();

    draw_digit(0);         
    timer1_init();        

    while (1) {
        __asm volatile ("nop");
    }
}