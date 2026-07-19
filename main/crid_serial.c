#include "crid_serial.h"
#include <stdio.h>
#include <stdarg.h>
#include "driver/uart.h"
#define UART_DEBUG_NUM UART_NUM_1
#define UART_DEBUG_TX_PIN 4
#define UART_DEBUG_RX_PIN 5
#define UART_DEBUG_BAUD 115200
#define UART_DEBUG_BUF_SIZE 1024
void crid_serial_init(void){uart_config_t c={.baud_rate=UART_DEBUG_BAUD,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE};uart_param_config(UART_DEBUG_NUM,&c);uart_set_pin(UART_DEBUG_NUM,UART_DEBUG_TX_PIN,UART_DEBUG_RX_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);uart_driver_install(UART_DEBUG_NUM,UART_DEBUG_BUF_SIZE,0,0,NULL,0);}
void crid_serial_write(const char *data,size_t len){uart_write_bytes(UART_DEBUG_NUM,data,len);}
void crid_serial_printf(const char *fmt,...){char buf[256];va_list a;va_start(a,fmt);int n=vsnprintf(buf,sizeof(buf),fmt,a);va_end(a);if(n>0)uart_write_bytes(UART_DEBUG_NUM,buf,n);}
