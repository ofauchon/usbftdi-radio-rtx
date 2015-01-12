/* 
Phoenix plugs,remote decoder
Olivier Fauchon 2015

* JE SUIS CHARLIE 
* WE ARE  CHARLIE

This program is distributed under the GPL
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <ftdi.h>




// Turns unsigned char value to bitstring (eg: 3 = 00000011); 
void byte_to_char(unsigned char c, char* res){
  int k=0; 
  for (k=0;k<8;k++){
	res[k]= (c & 0x1) ? '_':'-'; 
  }
}



#define FTDI_RXBUFLEN 200	// FTDI RXBUF lentrh
#define FTDI_RXBAUD 1000	// FTDI RX Baudrate

char* d_buffer;			// Pointer to the beginning of the buffer
unsigned int d_buffer_len;	// Total length of the buffer
unsigned int d_edge_state;	// State of the signal (unknown, rising|falling_done 
unsigned int state_counter;     // Counts #samples between signal transitions

#define STATE_UNKNOWN 0 	// We are in unknown state
#define STATE_RISING_DONE 1	// A rising Edge was detected
#define STATE_FALLING_DONE 2	// A falling edge was detected


#define BITSTACKLEN 30		// Bitstack is in fact the signal edges transition history		
int bitstack[BITSTACKLEN]; 
unsigned int bitstack_pos; 



// Try to decode phoenix plugs/remote protocol
void protodecode_phoenix(){
	int i; 
	if (bitstack[0]>-350 && bitstack[0]<-310){
		printf(":"); 
		for (i=0; i<bitstack_pos; i++) printf("%+05d ",bitstack[i]);
		printf("  "); 

		for (i=1; i<bitstack_pos; i=i+2) {
			int b1; 
			b1=bitstack[i]; 
			//b2=bitstack[i*2+1]; 
			if (b1>5 && b1<15) printf("0"); 
			if (b1>25 && b1<90) printf("1"); 
		}
		printf("\n"); 
	}	
}



void decoder_init()
{
	// The buffer is NULL 
	d_buffer = 0 ; 
	d_buffer_len = 0; 

	d_edge_state = STATE_UNKNOWN; 
	state_counter=0; 
 
	// Zero the bitstack 
	memset(bitstack, 0, BITSTACKLEN ); 
	bitstack_pos=0; 
}

void decoder_feed(char* data, unsigned int data_size)
{
	//printf("decoder_feed: New %u bytes\n", data_size); 
	unsigned int is_low, is_high, i; 

	// all-0 or all-1 samples might be dropped.
	for (i=0; i<data_size; i++) {
		if (data[i]==0) is_high=0; 
		if (data[i]==1) is_low=0; 
	}
	// Drop the sample if we are in unknown state and there is no transitions inside
	if ( (d_edge_state == STATE_UNKNOWN)  && (is_low || is_high) ) {
		printf("decoder_feed: is_low:%u is_high:%u, discarding\n", is_low, is_high);
		return; 
	}

	// Let's add the sample to the buffer. 
	char* ret= (char*) realloc(d_buffer, d_buffer_len + data_size); 
	if (ret == NULL ) { printf("decoder_feed: realloc() failed"); exit(1); } 

	d_buffer=ret; 
	memcpy( d_buffer + d_buffer_len , data, data_size);  
	d_buffer_len += data_size;
	
}

// 
void decoder_processbit(int val)
{
	int i; 
	// if buffer is full, shift values and drop oldest
	if (bitstack_pos==BITSTACKLEN-1) {
		for (i=0; i<BITSTACKLEN; i++) bitstack[i]=bitstack[i+1];
	}
	// Save current value 
	bitstack[bitstack_pos]= val; 
	
	// buffer is not full, move the cursor.
	if (bitstack_pos < (BITSTACKLEN-1) ) bitstack_pos++; 


	protodecode_phoenix();


}

// Detects and measuer signal transitions (and call _processbit, every time) 
void decoder_findtransitions()
{
	//printf("decoder_decode: Decoding %u\n", d_buffer_len);
	unsigned int i; 
	for (i=0; i<d_buffer_len-1 ; i++) {
		state_counter++; 
		if (d_buffer[i] <  d_buffer[i+1] ) 
		{ // Rising 
			if (d_edge_state==STATE_FALLING_DONE) {
		//		printf("Lo level for %d cycles\n", state_counter); 
				decoder_processbit( -((int) state_counter) ); 
				state_counter=0; 
			}
			d_edge_state=STATE_RISING_DONE; 
		} 
		else 
		if (d_buffer[i] >  d_buffer[i+1] ) 
		{  // Falling
			if (d_edge_state==STATE_RISING_DONE) {
		//		printf("Hi level for %u cycles\n", state_counter); 
				decoder_processbit(  (int)state_counter);
			}
			d_edge_state=STATE_FALLING_DONE; 
		}

	}

	memcpy(d_buffer, d_buffer+d_buffer_len, d_buffer_len); 
	d_buffer=realloc(d_buffer, 0); 
	d_buffer_len=0; 

}















int main(int argc, char **argv)
{
    struct ftdi_context *ftdi;
    int f;
    unsigned char buf[FTDI_RXBUFLEN];
    int retval = 0;

    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    f = ftdi_usb_open(ftdi, 0x0403, 0x6001);

    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        retval = 1;
        goto done;
    }

    printf("ftdi open succeeded: %d\n",f);

    printf("enabling bitbang mode\n");
    // All ports are Input 
    if (ftdi_set_bitmode(ftdi, 0x00, BITMODE_BITBANG)<0){
       printf("ERROR: can't set bitmode\n");  
       return EXIT_FAILURE; 
    }
    printf("set baudrate\n");
    if (ftdi_set_baudrate(ftdi, 1000)<0){
       printf("ERROR: can't set baudrate 9600\n");  
       return EXIT_FAILURE; 
    }

    sleep(1);

    decoder_init(); 



    // Read loop
    for (;;) {

        f = ftdi_read_data(ftdi, buf, FTDI_RXBUFLEN); 
        if (f < 0)
        {
             fprintf(stderr,"read failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
        }

	if (f>0) {
		decoder_feed( (char*) buf,f); 
		decoder_findtransitions();
		
		} 
	}

    printf("disabling bitbang mode\n");
    ftdi_disable_bitbang(ftdi);

    ftdi_usb_close(ftdi);
done:
    ftdi_free(ftdi);

    return retval;
}





