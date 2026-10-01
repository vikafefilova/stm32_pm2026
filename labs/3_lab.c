#include <stdint.h>
#include "stm32f10x.h"

/*
 * Светодиод: PC13 (на bluepill горит при ЛОГИЧЕСКОМ НУЛЕ, см. схему: +3.3V -> LED -> R -> C13)
 * Кнопка:    PA0 -> GND, внутренняя подтяжка к питанию (pull-up)
 *
 * Каждое нажатие кнопки удваивает частоту мигания:
 * 1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 64 -> 1/64 -> 1/32 -> ... -> 1/2 -> 1 Гц -> ...
 * Частота f = 2^k Гц, k = -6..6. После включения k = 0 (1 Гц).
 */

#define TICK_US         100     // шаг основного цикла, мкс
#define DEBOUNCE_TICKS  200     // 200 * 100 мкс = 20 мс - защита от дребезга
#define K_MIN           (-6)    // 1/64 Гц
#define K_MAX           6       // 64 Гц

/* Задержка в микросекундах (из презентации, слайд 10).
 * noinline - чтобы метки _loop/_exit не продублировались при встраивании функции. */
__attribute__((noinline)) void delay_us(uint32_t us) { // ~8 тактов/итерация
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
        :: "r"(9 * us)          // для 72 МГц
        : "cc"
    );
}

/* Полупериод (время "горит" или "не горит") в мкс для частоты 2^k Гц.
 * Период T = 1/f = 1 000 000 мкс / 2^k, полупериод = 500 000 / 2^k. */
static uint32_t half_period_us(int8_t k) {
    if (k >= 0) return 500000UL >> k;      // 64 Гц -> 7812 мкс
    else        return 500000UL << (-k);   // 1/64 Гц -> 32 000 000 мкс (32 с)
}

int main(void)
{
    /* 1. Тактирование портов A и C */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;

    /* 2. PC13 - выход push-pull, 10 МГц (MODE13 = 01, CNF13 = 00) */
    GPIOC->CRH = GPIOC->CRH & ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13) | GPIO_CRH_MODE13_0;

    /* 3. PA0 - вход с подтяжкой (MODE0 = 00, CNF0 = 10).
     *    Вывод 0 находится в младшем регистре CRL (выводы 0..7), а не в CRH. */
    GPIOA->CRL = GPIOA->CRL & ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0) | GPIO_CRL_CNF0_1;
    GPIOA->ODR |= GPIO_ODR_ODR0;           // ODR0 = 1 -> подтяжка именно ВВЕРХ (pull-up)

    int8_t   k        = 0;                 // текущая частота 2^k Гц, старт: 1 Гц
    uint32_t half     = half_period_us(k);
    uint32_t elapsed  = 0;                 // сколько мкс прошло в текущем полупериоде

    uint8_t  btn_stable = 0;               // подтверждённое состояние кнопки: 0 - отпущена, 1 - нажата
    uint16_t btn_cnt    = 0;               // счётчик для антидребезга

    while (1)
    {
        delay_us(TICK_US);                 // один "тик" = 100 мкс

        /* --- Мигание без блокировки --- */
        elapsed += TICK_US;
        if (elapsed >= half) {
            elapsed = 0;
            GPIOC->ODR ^= GPIO_ODR_ODR13;  // переключаем светодиод
        }

        /* --- Опрос кнопки с антидребезгом --- */
        uint8_t btn_raw = !(GPIOA->IDR & GPIO_IDR_IDR0);   // 1 = нажата (PA0 замкнут на GND)

        if (btn_raw != btn_stable) {
            // состояние отличается от подтверждённого - ждём, пока оно продержится 20 мс
            if (++btn_cnt >= DEBOUNCE_TICKS) {
                btn_cnt    = 0;
                btn_stable = btn_raw;

                if (btn_stable) {          // событие "нажатие" (фронт отпущена -> нажата)
                    k = (k < K_MAX) ? (int8_t)(k + 1) : K_MIN;
                    half    = half_period_us(k);
                    elapsed = 0;           // новый полупериод начинаем сразу
                }
            }
        } else {
            btn_cnt = 0;                   // дребезг или шум - сбрасываем счётчик
        }
    }
}