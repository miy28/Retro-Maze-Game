/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)


int TUX__INIT(struct tty_struct* tty);
int TUX__BUTTONS(struct tty_struct* tty, unsigned long arg);
int TUX__SET_LED(struct tty_struct* tty, unsigned long arg);

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];
	
	switch(a) {
		case MTCP_BIOC_EVENT:
			packets[0] = b; // fill in the packet for tux ioctls
			packets[1] = c;
		case MTCP_RESET:
			TUX__INIT(tty); // reset with saved buffer
			tuxctl_ldisc_put(tty, buffer_SAVE, 6);
			tuxctl_ldisc_put(tty, buffer_SAVE, 6);
			// printk("%02x\n%02x\n%02x\n%02x\n%02x\n%02x\n", buffer_SAVE[0], buffer_SAVE[1], buffer_SAVE[2], buffer_SAVE[3], buffer_SAVE[4], buffer_SAVE[5]);
			break;
		case MTCP_ACK:
			ack = 0; //reset the acknowledge bit
			break;
		default:
			break;
	}
		

	

    /*printk("packet : %x %x %x\n", a, b, c); */
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT:
		return TUX__INIT(tty);
	case TUX_BUTTONS:
		return TUX__BUTTONS(tty, arg);
	case TUX_SET_LED:
		return TUX__SET_LED(tty, arg);
	case TUX_LED_ACK:
		return -EINVAL;
	case TUX_LED_REQUEST:
		return -EINVAL;
	case TUX_READ_LED:
		return -EINVAL;
	default:
	    return -EINVAL;
    }
}

/* 
 * TUX__INIT
 *   DESCRIPTION: 	Initialize the tux driver using buffer with 
 					correct signal calls
 *   INPUTS: 		tty --- struct ptr tty
 *   OUTPUTS: 		0
 */
int TUX__INIT(struct tty_struct* tty){

	buffer_INIT[0] = MTCP_BIOC_ON;
	buffer_INIT[1] = MTCP_LED_SET;

	tuxctl_ldisc_put(tty, buffer_INIT, 2);

	ack = 0;

	return 0;
}

/* 
 * TUX__BUTTONS
 *   DESCRIPTION: 	Handles tux controller input, converts into proper 
 					direction byte data and sends to userspace
 *   INPUTS: 		tty --- struct ptr tty
					arg --- input bits
 *   OUTPUTS: 		0
 */
int TUX__BUTTONS(struct tty_struct* tty, unsigned long arg){
	
	unsigned char bit0, bit1, bit2, bit3, bit4, bit5, bit6, bit7;
	unsigned long send = 0;

	if(!arg){
		return -EINVAL;
	}

	bit0 = packets[0] & 0x01; //load packets element with respective mask to isolate the bit
	bit1 = packets[0] & 0x02;
	bit2 = packets[0] & 0x04;
	bit3 = packets[0] & 0x08;
	
	bit4 = packets[1] & 0x01;
	bit5 = (packets[1] & 0x04) >> 1; // swap places from the position we were given
	bit6 = (packets[1] & 0x02) << 1;
	bit7 = packets[1] & 0x08;

	send = (bit4|bit5|bit6|bit7) << 4; // load in the proper higher bits, then move them over
	send = send|bit0|bit1|bit2|bit3; // load in the proper lower bits
	
	copy_to_user((unsigned long*)arg, &send, sizeof(send));
	return 0;

}

/* 
 * TUX__SET_LED
 *   DESCRIPTION: 	Writes given character info into tux LED display format
 *   INPUTS: 		tty --- struct ptr tty
					arg --- input bits
 *   OUTPUTS: 		0
 */
int TUX__SET_LED(struct tty_struct* tty, unsigned long arg){
	
	unsigned char led_key[16] = {0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD, 0xED, 0x86, 0xEF, 0xAE, 0xEE, 0x6D, 0xE1, 0x4F, 0xE9, 0xE8}; // lookup table for
	
	unsigned long to_display, led_byte, decimal_byte;
	int i, display_bit, led_bit, decimal_bit;

	buffer_SET_LED[0] = MTCP_LED_SET; // signal to take SET_LED
	buffer_SET_LED[1] = 0xF; // write to all 4 led's

	if(ack == 1) //acknowledge signal
		return -EINVAL;

	ack = 1;

	to_display = arg & TO_DISPLAY_MASK; // sectioning off the input arg
	led_byte = arg & LED_BYTE_MASK;
	decimal_byte = arg & DECIMAL_BYTE_MASK;

	

	for(i = 0; i < 4; i++){
		//logic for handling lower 16 bits
		display_bit = (to_display >> i*4) & 0xF; // bit shift 4 bits into the mask every iteration
		buffer_SET_LED[i+2] = led_key[display_bit]; 

		//logic for handling low 4 bits of third byte
		led_bit = (led_byte >> i) & 0x00010000; // bit shift 1 bits into the mask every iteration
		if(!led_bit){
			buffer_SET_LED[i+2] = 0;
		}

		//logic for handling low 4 bits of highest byte
		decimal_bit = (decimal_byte >> i) & 0x01000000; 
		if(decimal_bit)
			buffer_SET_LED[i+2] += 16; // binary of 16 adds 1 to the decimal bit

	}


	for(i = 0; i < 6; i++){ //fill temp buffer with current LED display
		buffer_SAVE[i] = buffer_SET_LED[i];
	}

	tuxctl_ldisc_put(tty, buffer_SET_LED, 6);
	tuxctl_ldisc_put(tty, buffer_SET_LED, 6);

    // printk("%02x\n%02x\n%02x\n%02x\n%02x\n%02x\n", buffer_SET_LED[0], buffer_SET_LED[1], buffer_SET_LED[2], buffer_SET_LED[3], buffer_SET_LED[4], buffer_SET_LED[5]);

	return 0;  
}
