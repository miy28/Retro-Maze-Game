// All necessary declarations for the Tux Controller driver must be in this file

#ifndef TUXCTL_H
#define TUXCTL_H

#define TUX_SET_LED _IOR('E', 0x10, unsigned long)
#define TUX_READ_LED _IOW('E', 0x11, unsigned long*)
#define TUX_BUTTONS _IOW('E', 0x12, unsigned long*)
#define TUX_INIT _IO('E', 0x13)
#define TUX_LED_REQUEST _IO('E', 0x14)
#define TUX_LED_ACK _IO('E', 0x15)

#define TO_DISPLAY_MASK 0x0000FFFF
#define LED_BYTE_MASK 0x000F0000
#define DECIMAL_BYTE_MASK 0x0F000000

unsigned char buffer_INIT[2];
char buffer_BUTTONS[2];
unsigned char buffer_SET_LED[6];

unsigned char packets[2];
unsigned char buffer_SAVE[6];

int ack = 0;

#endif
