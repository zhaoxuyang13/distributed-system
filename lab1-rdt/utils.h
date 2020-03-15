#ifndef _UTILS_H_
#define _UTILS_H_

const int WINDOW_SIZE = 10;
const double TIME_OUT = 0.3;
const int SEQUNCE_SIZE = 128;

typedef unsigned int seq_nr_t;

static bool between(seq_nr_t a, seq_nr_t b, seq_nr_t c){
    if( ((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a))) 
        return true;
    else 
        return false;
}

static void inc(seq_nr_t &num, int size){
    num = (num + 1) % size;
}





static void             init_crc16_tab( void );

static bool             crc_tab16_init          = false;
static uint16_t         crc_tab16[256];


/*
 * static void init_crc16_tab( void );
 *
 * For optimal performance uses the CRC16 routine a lookup table with values
 * that can be used directly in the XOR arithmetic in the algorithm. This
 * lookup table is calculated by the init_crc16_tab() routine, the first time
 * the CRC function is called.
 */
#define		CRC_POLY_16		0xA001
static void init_crc16_tab( void ) {

	uint16_t i;
	uint16_t j;
	uint16_t crc;
	uint16_t c;

	for (i=0; i<256; i++) {

		crc = 0;
		c   = i;

		for (j=0; j<8; j++) {

			if ( (crc ^ c) & 0x0001 ) crc = ( crc >> 1 ) ^ CRC_POLY_16;
			else                      crc =   crc >> 1;

			c = c >> 1;
		}

		crc_tab16[i] = crc;
	}

	crc_tab16_init = true;

}  /* init_crc16_tab */

/*
 * uint16_t crc_16( const unsigned char *input_str, size_t num_bytes );
 *
 * The function crc_16() calculates the 16 bits CRC16 in one pass for a byte
 * string of which the beginning has been passed to the function. The number of
 * bytes to check is also a parameter. The number of the bytes in the string is
 * limited by the constant SIZE_MAX.
 */

#define		CRC_START_16		0x0000
static uint16_t crc_16( const unsigned char *input_str, size_t num_bytes ) {
	// num_bytes %= RDT_PKTSIZE + 2;
	uint16_t crc;
	const unsigned char *ptr;
	size_t a;

	if ( ! crc_tab16_init ) init_crc16_tab();

	crc = CRC_START_16;
	ptr = input_str;

	if ( ptr != NULL ) for (a=0; a<num_bytes; a++) {

		crc = (crc >> 8) ^ crc_tab16[ (crc ^ (uint16_t) *ptr++) & 0x00FF ];
	}

	return crc;

}  /* crc_16 */



#endif