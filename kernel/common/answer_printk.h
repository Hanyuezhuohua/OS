//
// Created by Wenxin Zheng on 2021/4/21.
//

#ifndef ACMOS_SPR21_ANSWER_PRINTK_H
#define ACMOS_SPR21_ANSWER_PRINTK_H

static void printk_write_string(const char *str) {
    // Homework 1: YOUR CODE HERE
    // this function print string by the const char pointer
    // I think 3 lines of codes are enough, do you think so?
    // It's easy for you, right?
    while (*str) uart_putc(*(str++));
}


static void printk_write_num(int base, unsigned long long n, int neg) {
    // Homework 1: YOUR CODE HERE
    // this function print number `n` in the base of `base` (base > 1)
    // you may need to call `printk_write_string`
    // you do not need to print prefix like "0x", "0"...
    // Remember the most significant digit is printed first.
    // It's easy for you, right?
    if (neg == 1) uart_putc('-');
    char a[MAX_INT_BUFF_SIZE];
    int i = 0;
    while (n != 0){
    	int mod = n % base;
    	n = n / base;
    	if (mod < 10) a[i] = '0' + mod;
    	else a[i] = 'a' + mod - 10;
    	i++;
    }
    if(i == 0) uart_putc('0');
    else for(--i; i >= 0; --i) uart_putc(a[i]);
}

#endif  // ACMOS_SPR21_ANSWER_PRINTK_H
