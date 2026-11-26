#include <stdint.h>
#include "stm32f10x.h"   

#define F_CLK     72000000UL   
#define BAUD      9600UL
#define BUF_SIZE  32

void UART_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    GPIOA->CRH &= ~(0xFUL << 4);
    GPIOA->CRH |=  (0x9UL << 4);

    GPIOA->CRH &= ~(0xFUL << 8);
    GPIOA->CRH |=  (0x8UL << 8);
    GPIOA->ODR |=  (1UL << 10);

    USART1->BRR = (uint16_t)(F_CLK / BAUD);
  
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void UART_Write(uint8_t data)
{
    while ((USART1->SR & USART_SR_TXE) == 0);   
    USART1->DR = data;
}

uint8_t UART_Read(void)
{
    while ((USART1->SR & USART_SR_RXNE) == 0); 
    return (uint8_t)USART1->DR;
}

static void UART_WriteString(const char *s)
{
    while (*s) UART_Write((uint8_t)*s++);
}

static void UART_WriteInt(int32_t value)
{
    char buf[12];
    int i = 0;
    uint32_t u;

    if (value < 0) {
        UART_Write('-');
        u = (uint32_t)(-(int64_t)value);
    } else {
        u = (uint32_t)value;
    }

    if (u == 0) buf[i++] = '0';
    while (u > 0) {
        buf[i++] = (char)('0' + u % 10);
        u /= 10;
    }
    while (i > 0) UART_Write((uint8_t)buf[--i]);
}

static void read_line(char *buf, uint8_t size)
{
    uint8_t len = 0;
    for (;;) {
        uint8_t c = UART_Read();

        if (c == '\r' || c == '\n') {               
            UART_WriteString("\r\n");
            buf[len] = '\0';
            return;
        }
        if ((c == 0x08 || c == 0x7F) && len > 0) {    
            len--;
            UART_WriteString("\b \b");
        } else if (c >= 0x20 && c < 0x7F && len < size - 1) {
            buf[len++] = (char)c;
            UART_Write(c);                            
        }

    }
}

static const char *skip_spaces(const char *p)
{
    while (*p == ' ') p++;
    return p;
}

static int parse_int(const char **pp, int32_t *out)
{
    const char *p = skip_spaces(*pp);
    int neg = 0;
    int32_t v = 0;

    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }
    *out = neg ? -v : v;
    *pp = p;
    return 1;
}

int main(void)
{
    char line[BUF_SIZE];

    UART_Init();
    UART_WriteString("Calculator. Format: <a> <op> <b>  (op: + - * /)\r\n");

    for (;;) {
        const char *p = line;
        int32_t a, b, res = 0;
        char op;

        UART_WriteString("> ");
        read_line(line, BUF_SIZE);

        if (line[0] == '\0') continue;  

        if (!parse_int(&p, &a)) { UART_WriteString("Error: bad operand 1\r\n"); continue; }

        p = skip_spaces(p);
        op = *p;
        if (op != '+' && op != '-' && op != '*' && op != '/') {
            UART_WriteString("Error: bad operation\r\n");
            continue;
        }
        p++;

        if (!parse_int(&p, &b)) { UART_WriteString("Error: bad operand 2\r\n"); continue; }

        switch (op) {
            case '+': res = a + b; break;
            case '-': res = a - b; break;
            case '*': res = a * b; break;
            case '/':
                if (b == 0) { UART_WriteString("Error: division by zero\r\n"); continue; }
                res = a / b;
                break;
        }

        UART_WriteString("= ");
        UART_WriteInt(res);
        UART_WriteString("\r\n");
    }
}
