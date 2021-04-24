//
// Created by Wenxin Zheng on 2021/4/21.
//

#ifndef ACMOS_SPR21_ANSWER_PRINTK_H
#define ACMOS_SPR21_ANSWER_PRINTK_H

static void printk_write_string(const char *str) {
    while (*str) { uart_putc(*str); str++; }
}

static void printk_write_num(int base, unsigned long long n, int neg) {
    const int MAX_LEN = 25;
    unsigned int stack[MAX_LEN], top = 0, cnt = 0;
    char str[MAX_LEN];
    if (neg) str[cnt++] = '-';
    while (n != 0) { stack[top++] = n % base; n /= base; }
    while (top) {
    	int x = stack[--top];
    	if (x >= 0 && x <= 9) str[cnt++] = '0' + x;
    	else str[cnt++] = 'a' + (x - 10);
    }
    str[cnt] = '\0';
    printk_write_string(str);
}

#endif  // ACMOS_SPR21_ANSWER_PRINTK_H
