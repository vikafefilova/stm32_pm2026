#include <stdint.h>
#include "stm32f10x.h"

#define TICK_US         100     
#define DEBOUNCE_TICKS  200    
#define K_MIN           (-6)   
#define K_MAX           6      

__attribute__((noinline)) void delay_us(uint32_t us) { 
    __asm volatile (
        "push {r0}\r\n"
        "mov R0, %0\r\n"
        "_loop:\r\n"
            "cmp R0, #0\r\n"
            "beq _exit\r\n"
            "sub R0, R0, #1\r\n"
            "nop\r\n"
            "b _loop\r\n"
        "_exit:\r\n"
        "pop {r0}\r\n"
        :: "r"(9 * us)    
        : "cc"
    );
}


static uint32_t half_period_us(int8_t k) {
    if (k >= 0) return 500000UL >> k;     
    else        return 500000UL << (-k);  
}

int main(void)
{
   
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;

    GPIOC->CRH = GPIOC->CRH & ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13) | GPIO_CRH_MODE13_0;

    GPIOA->CRL = GPIOA->CRL & ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0) | GPIO_CRL_CNF0_1;
    GPIOA->ODR |= GPIO_ODR_ODR0;          

    int8_t   k        = 0;                 
    uint32_t half     = half_period_us(k);
    uint32_t elapsed  = 0;                 

    uint8_t  btn_stable = 0;           
    uint16_t btn_cnt    = 0;              

    while (1)
    {
        delay_us(TICK_US);       

        elapsed += TICK_US;
        if (elapsed >= half) {
            elapsed = 0;
            GPIOC->ODR ^= GPIO_ODR_ODR13; 
        }

        uint8_t btn_raw = !(GPIOA->IDR & GPIO_IDR_IDR0);  

        if (btn_raw != btn_stable) {
            if (++btn_cnt >= DEBOUNCE_TICKS) {
                btn_cnt    = 0;
                btn_stable = btn_raw;

                if (btn_stable) {         
                    k = (k < K_MAX) ? (int8_t)(k + 1) : K_MIN;
                    half    = half_period_us(k);
                    elapsed = 0;        
                }
            }
        } else {
            btn_cnt = 0;                  
        }
    }
}