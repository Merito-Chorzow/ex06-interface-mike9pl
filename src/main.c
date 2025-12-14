#include <stdio.h>
#include "shell.h"

int main(void){
    shell_t sh; shell_init(&sh);

    // proste użycie: get → set → stat
    shell_rx_bytes(&sh, "get\r\n");
    for(int i=0;i<5;i++) shell_tick(&sh);

    shell_rx_bytes(&sh, "set 0.42\r\n");
    for(int i=0;i<5;i++) shell_tick(&sh);

    shell_rx_bytes(&sh, "stat\r\n");
    for(int i=0;i<5;i++) shell_tick(&sh);

    shell_rx_bytes(&sh, "echo hello world\r\n");
    for(int i=0;i<5;i++) shell_tick(&sh);

    // Burst > RB_SIZE — zasymuluj przepełnienie
    for(int i=0;i<200;i++) shell_rx_bytes(&sh, "noop\r\n");
    for(int i=0;i<20;i++) shell_tick(&sh);

    // końcowy status
    shell_rx_bytes(&sh, "get\r\n");
    for(int i=0;i<5;i++) shell_tick(&sh);

    return 0;
}
