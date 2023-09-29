/***************************************************
 * ABCcas - by Robert Juhasz, 2008
 * Added more stuff 2023 by Tony Rogvall
 *
 * usage: abccas2 [<options>] [<file>[.bas|.bac|.*]]
 * OPTIONS
 *         -h             display help and exit
 *         -v             verbose
 *         -k             translate \n to \r
 *         -b 700|2400    baud rate (700)
 *         -r >1400       audio file sample rate (11200)
 *         -f wav|au|raw  audio format (wav)
 *         -o <filename>  audio output filename (stdout)
 *
 * generates <file>[.bac|.bas].[wav|au] (-o option only) 
 * which can be loaded by ABC80 (LOAD CAS:)
 * Filename in the transmitted data is based on original 
 * filename (uppercase of first 8 char in basename)
 ****************************************************/
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "wav.h"
#include "au.h"

#define STX  0x02
#define ETX  0x03
#define SYNC 0x16

#define AUDIO_FORMAT_UNDEF -1
#define AUDIO_FORMAT_RAW   0
#define AUDIO_FORMAT_WAV   1
#define AUDIO_FORMAT_AU    2
#define DEFAULT_AUDIO_FORMAT AUDIO_FORMAT_WAV

#define DEFAULT_SAMPLE_RATE 11200
#define DEFAULT_BAUD        700
#define BITS_PER_CHANNEL    16
#define NUM_CHANNELS        1
#define LOW_LEVEL -0.504
#define HIGH_LEVEL 0.678

#if BITS_PER_CHANNEL == 8
typedef uint8_t sample_t;
#elif BITS_PER_CHANNEL == 16
typedef uint16_t sample_t;
#else
#error "BIT_PER_CHANNEL not defined"
#endif

typedef struct {
    uint8_t header[3];
    uint8_t name[8];
    uint8_t ext[3];  // "BAS" | "BAC" etc
    uint8_t pad[256-(3+8+3)];  // zero!
} name_block_t;

typedef struct {
    uint8_t  pad;
    uint16_t blcnt;     // 16 bit block counter
    uint8_t  data[253];
} data_block_t;

// uint8_t block[256];
char* progname = "abccas2";
char outname[FILENAME_MAX+1];

char name[8]="TESTTT  ";
char ext[3]="BAC";
int baud = DEFAULT_BAUD;   // baud
int sample_rate = DEFAULT_SAMPLE_RATE;   // initial sample rate
int hbitsz = (DEFAULT_SAMPLE_RATE/DEFAULT_BAUD)/2;
int bitsz  = DEFAULT_SAMPLE_RATE/DEFAULT_BAUD;

int verbose = 0;

#define MAX_HBITSZ 128

//
// output sqaure wave samples
// ABC 80  = 700 baud
// ABC 800 = 700/2400 baud
//
//
// |         |  = 0
// |         | 
// +----+----+
//
// +---------+
// |         |  = 0
// |         | 
//
// |    +----+  = 1
// |    |    | 
// +----+    +
//
// +----+    +  = 1
// |    |    |
// +    +----+
//
//
typedef struct bstate {
    int bx;
    sample_t low;    
    sample_t high;
} bstate_t;

bstate_t bit_state = { .bx = 1 };

sample_t high_samples[MAX_HBITSZ];
sample_t low_samples[MAX_HBITSZ];

void init_bits(bstate_t* bst, sample_t low, sample_t high)
{
    int i;

    bst->low = low;
    bst->high = high;

    for (i = 0; i < MAX_HBITSZ; i++) {
	low_samples[i] = low;
	high_samples[i] = high;
    }
}

void transmit_bit(bstate_t* bst, int bit, FILE *fout)
{
    bst->bx = !bst->bx;
    fwrite(bst->bx ? high_samples : low_samples,sizeof(sample_t),hbitsz,fout);
    
    if (bit) // send "1"
	bst->bx = !bst->bx;
    fwrite(bst->bx ? high_samples : low_samples,sizeof(sample_t),hbitsz,fout);
}

void transmit_byte(uint8_t b, FILE *fout)
{
    bstate_t* bst = &bit_state;
    int i, mask = 0x01;

    for (i=0; i<8; i++) {
	transmit_bit(bst, mask & b, fout);
	mask <<= 1; // next bit
    }
}

void transmit_uint16_le(uint16_t w, FILE* fout)
{
    transmit_byte(w, fout);
    transmit_byte(w >> 8, fout);
}

uint16_t checksum16(uint8_t* ptr, size_t len)
{
    uint16_t csum = 0;
    while(len--)
	csum += *ptr++;
    return csum;
}

// write 32 + 3 + 1 + 256 + 1 + 2
//       0  sync stx  data etx checksum
//          
void transmit_block(uint8_t* buf, FILE *fout)
{
    int i;
    uint16_t csum;
    
    for (i=0; i<32; i++) transmit_byte(0,fout);           // 32 0 bytes
    for (i=0; i<3; i++)  transmit_byte(SYNC,fout);        // 3 sync bytes 16H
    transmit_byte(STX, fout);
    // output the block
    for (i=0; i<256; i++)
	transmit_byte(buf[i], fout);
    transmit_byte(ETX,fout);
    
    // calculate the checksum
    csum = checksum16(buf, 256);
    // csum includes ETX char!!! (as correctly stated in Mikrodatorns ABC)
    csum += ETX;

    transmit_uint16_le(csum, fout);
}

// 3 + 8 + 3
// <<0xff,0xff,0xff,F,I,L,E,N,A,M,E,'B','A','C', 0:

void transmit_name_block(FILE *fout)
{
    name_block_t block;

    memset(block.header, 0xff, sizeof(block.header));
    memcpy(block.name,   name, sizeof(name));
    memcpy(block.ext,    ext,  sizeof(ext));
    memset(block.pad,    0,    sizeof(block.pad));
    transmit_block((uint8_t*) &block, fout);
}

void transmit_data_block(int cnt, char* buf, size_t len, FILE *fout)
{
    data_block_t block;

    block.pad = 0;
    block.blcnt = cnt;
    if (len >= sizeof(block.data)) {
	memcpy(&block.data, buf, sizeof(block.data));
    }
    else {
	memcpy(&block.data, buf, len);
	memset(&block.data+len, 0, sizeof(block.data)-len);
    }
    little16(&block.blcnt);
    transmit_block((uint8_t*) &block, fout);
    if (verbose)
	fprintf(stderr, "%s: output block len=%ld #%d\n", progname, len, cnt);
}

void transmit_data_blocks(char* buf, size_t len, FILE *fout)
{
    int cnt = 0;
    
    while (len > 0) {
	transmit_data_block(cnt++, buf, len, fout);
	len = (len >= 253) ? len-253 : 0;
    }
}

void write_wav(FILE *f, int numsamp, int bits_per_channel, int num_channels)
{
    int srate;
    uint32_t totlen;
    uint16_t frame_size = (bits_per_channel*num_channels+7)/8;
    wav_header_t wav;

    if (numsamp < 0)
	totlen = 0xffffffff;
    else
	totlen = (5*4 + 24)+numsamp*frame_size;

    write_tag(f, WAV_ID_RIFF);
    write_32le(f, totlen);
    
    write_tag(f, WAV_ID_WAVE);
    write_tag(f, WAV_ID_FMT);
    write_32le(f, sizeof(wav_header_t));

    wav.AudioFormat = WAVE_FORMAT_PCM;   // PCM
    wav.NumChannels = num_channels;      // Mono=1 | Stereo=2
    wav.SampleRate  = sample_rate; // Sample Rate (Binary, in Hz)
    wav.ByteRate    = sample_rate; // Byte Rate = sample rates since mono 8-bit
    wav.FrameSize   = frame_size;
    wav.BitsPerChannel = bits_per_channel;
    write_wav_header(f, &wav);

    // "data" + length:32 + data...
    write_tag(f, WAV_ID_DATA);
    write_32le(f, numsamp*frame_size);
}

void write_au(FILE *f, int numsamp, int bits_per_channel, int num_channels)
{
    int srate;
    uint32_t numbytes;
    uint16_t frame_size = (bits_per_channel*num_channels+7)/8;    
    au_header_t au;

    if (numsamp == -1)
	numbytes = 0xffffffff;  // unknown
    else
	numbytes = numsamp*frame_size;

    au.magic = AU_MAGIC;
    au.data_offset = 28;   // minimum
    au.data_size   = numbytes;
    switch(bits_per_channel) {
    case 8: au.encoding = AU_ENCODING_LINEAR_8; break;
    case 16: au.encoding = AU_ENCODING_LINEAR_16; break;
    }
    au.sample_rate = sample_rate;
    au.channels    = num_channels;
    strcpy(au.annot, "ABC");
    
    write_au_header(f, &au);
}

void usage()
{
    fprintf(stderr, "usage: %s [<options>] [<file>[.bas|.bac|other]]\n",
	    progname);
    fprintf(stderr, "OPTIONS\n");
    fprintf(stderr, "    -h             help\n");
    fprintf(stderr, "    -v             verbose\n");    
    fprintf(stderr, "    -k             konvert \\n to \\r\n");
    fprintf(stderr, "    -b (700)|2400  baud rate\n");
    fprintf(stderr, "    -r >1400       sample rate\n");
    fprintf(stderr, "    -f wav|au|raw  audio format\n");
    fprintf(stderr, "    -o <filename>  audio output filename\n");
    exit(1);
}

// replace \n with \r and remove multiple \r\r.. with one \r
size_t konvert_line(char* ptr, size_t len)
{
    int c, oldc = 0;
    char* ptr0 = ptr;
    char* fptr;
    
    fptr = ptr;
    while(len--) {
	c = *ptr++;
	if (c == '\n')
	    c = '\r';
	if (c == '\r') {
	    if (oldc != '\r')
		*fptr++ = c;
	}
	else { // c != '\r'
	    *fptr++ = c;
	}
	oldc = c;
    }
    return fptr-ptr0;
}


int main(char argc, char *argv[])
{
    int filelen=-1;
    int numblk,numsamp,numbyte,blockcnt;
    // struct stat filestat;
    FILE* fin = stdin;
    FILE* fout = stdout;
    char* ptr;
    char* fptr;
    int i;
    int konv = 0;
    int opt;
    int rate0 = DEFAULT_SAMPLE_RATE;
    int br;
    int audio_format = DEFAULT_AUDIO_FORMAT;
    char* input_filename = "*stdin*";
    char* output_filename = NULL;

    while ((opt = getopt(argc, argv, "f:o:vhkb:r:")) != -1) {
	switch(opt) {
	case 'h':
	    usage();
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'k':
	    konv = 1;
	    break;
	case 'r':
	    rate0 = atoi(optarg);
	    if (rate0 < 1400)
		usage();
	    break;
	case 'b':
	    baud = atoi(optarg);
	    if ((baud != 700) && (baud != 2400))
		usage();
	    break;
	case 'f':
	    if (strcmp(optarg, "wav") == 0)
		audio_format = AUDIO_FORMAT_WAV;
	    else if (strcmp(optarg, "au") == 0)
		audio_format = AUDIO_FORMAT_AU;
	    else if (strcmp(optarg, "raw") == 0)
		audio_format = AUDIO_FORMAT_RAW;
	    else
		usage();
	    break;
	case 'o':
	    if (strlen(optarg) >= FILENAME_MAX) {
		fprintf(stderr, "%s: filename too long (max=%d)\n",
			progname, FILENAME_MAX);
		exit(1);
	    }
	    strncpy(outname, optarg, sizeof(outname));
	    output_filename = outname;
	    break;
	default:
	    usage();
	}
    }

    br = (rate0+baud-1)/baud;
    if (br & 1) br++;           // make even
    hbitsz = br/2;              // half bit size
    bitsz  = hbitsz*2;          // bitsize
    sample_rate = br*baud;      // adjust rate

    if (output_filename != NULL) {
	if ((ptr = strrchr(output_filename, '.')) != NULL) {
	    if (audio_format == AUDIO_FORMAT_UNDEF) {
		if (strcasecmp(ptr, ".wav") == 0)
		    audio_format = AUDIO_FORMAT_WAV;
		else if (strcasecmp(ptr, ".au") == 0)
		    audio_format = AUDIO_FORMAT_AU;
		else
		    audio_format = AUDIO_FORMAT_RAW;
	    }
	}
	if ((ptr == NULL) || ((fptr = strrchr(ptr, '/')) == NULL))
	    fptr = output_filename;
	else
	    fptr++;
    }

    if (optind < argc) {
	input_filename = argv[optind];
	if ((ptr = strrchr(input_filename, '.')) != NULL) {
	    if (strcasecmp(ptr, ".bas") == 0) {
		memcpy(ext, "BAS", 3);
		konv = 1;
	    }
	    else if (strcasecmp(ptr, ".bac") == 0)
		memcpy(ext, "BAC", 3);
	    else {
		memcpy(ext, "BAC", 3);  // default!
		konv = 1;
	    }
	}
	if ((ptr == NULL) || ((fptr = strrchr(ptr, '/')) == NULL))
	    fptr = argv[optind];
	else
	    fptr++;
	// set casette file name from real filename
	memset(name, ' ', 8);
	for (i = 0; (fptr[i] != '.') && (i < 8); i++)
	    name[i] = toupper(fptr[i]);
	if ((fin=fopen(input_filename,"rb")) == NULL) {
	    fprintf(stderr, "%s: unable to open file %s (%s)\n",
		    progname, input_filename, strerror(errno));
	    exit(1);
	}
    }

    if (output_filename != NULL) {
	fout = fopen(output_filename,"wb");
    }

    if (verbose) {
	fprintf(stderr, "%s: baud=%d, rate'=%d, rate=%d\n",
		progname, baud, rate0, sample_rate);
	fprintf(stderr, "         bitsz=%d, hbitsz=%d\n", bitsz, hbitsz);
	fprintf(stderr, "         input_filename = %s\n", input_filename);
	fprintf(stderr, "         output_filename = %s\n", output_filename);
	fprintf(stderr, "         cassete name = %.8s.%s\n", name, ext);
	fprintf(stderr, "         audio_format = %d\n", audio_format);
    }
    
    if ((hbitsz < 1) || (hbitsz > MAX_HBITSZ)) {
	fprintf(stderr, "rate / baud out of range\n");
	exit(1);
    }
    // level is -0.504 ... 0.678
    if (audio_format == AUDIO_FORMAT_WAV) {     
#if BITS_PER_CHANNEL == 8  // u8
	sample_t l = (sample_t)(LOW_LEVEL*0x7f)+0x80;
	sample_t h = (sample_t)(HIGH_LEVEL*0x7f)+0x80;
	init_bits(&bit_state, l, h);
#elif BITS_PER_CHANNEL == 16 // s16_le
	sample_t l = (sample_t)(LOW_LEVEL*0x7fff);
	sample_t h = (sample_t)(HIGH_LEVEL*0x7fff);
	little16(&l);
	little16(&h);
	init_bits(&bit_state, l, h);
#endif
    }
    else if (audio_format == AUDIO_FORMAT_AU) { // s8 | s16_be
#if BITS_PER_CHANNEL == 8
	sample_t l = (sample_t)(LOW_LEVEL*0x7f);
	sample_t h = (sample_t)(HIGH_LEVEL*0x7f);
	init_bits(&bit_state, l, h);
#elif BITS_PER_CHANNEL == 16
	sample_t l = (sample_t)(LOW_LEVEL*0x7fff);
	sample_t h = (sample_t)(HIGH_LEVEL*0x7fff);
	big16(&l);
	big16(&h);
	init_bits(&bit_state, l, h);
#endif
    }

    // read the file into a buffer
    if ((audio_format == AUDIO_FORMAT_WAV) && (filelen == -1)) {
	char filebuf[64*1024];
	uint16_t frame_size = (BITS_PER_CHANNEL*NUM_CHANNELS+7)/8;
	
	filelen = fread(filebuf, sizeof(char), sizeof(filebuf), fin);
	if (verbose)
	    fprintf(stderr, "input filelen = %d\n", filelen);
	if (konv)
	    filelen = konvert_line(filebuf, filelen);
	numblk=filelen / 253;
	if (filelen % 253) numblk++;    // if not exact add block
	numblk++;                       // add one for name block
	numsamp = numblk*(32+3+1+256+1+2)*8*bitsz;   // number of samples
	numbyte = numsamp*frame_size;

	if (verbose)
	    fprintf(stderr, "Size:%d Blk:%d Byte:%d Samp:%d\n",
		    filelen,numblk,numbyte,numsamp);
	if (audio_format == AUDIO_FORMAT_WAV)
	    write_wav(fout,numsamp,BITS_PER_CHANNEL,NUM_CHANNELS);
	else if (audio_format == AUDIO_FORMAT_AU)
	    write_au(fout,numsamp,BITS_PER_CHANNEL,NUM_CHANNELS);
	else
	    ;
	transmit_name_block(fout);
	transmit_data_blocks(filebuf, filelen, fout);
    }
    else {
	char blkbuf[253];	
	size_t len;
	int cnt = 0;

	if (audio_format == AUDIO_FORMAT_WAV)
	    write_wav(fout,-1,BITS_PER_CHANNEL,NUM_CHANNELS);
	else if (audio_format == AUDIO_FORMAT_AU)
	    write_au(fout,-1,BITS_PER_CHANNEL,NUM_CHANNELS);
	else
	    ;
	transmit_name_block(fout);

	filelen = 0;
	while ((len = fread(blkbuf, sizeof(char), sizeof(blkbuf), fin))) {
	    if (konv)
		len = konvert_line(blkbuf, len);
	    transmit_data_block(cnt++, blkbuf, len, fout);
	    filelen += len;
	}
	if (verbose)
	    fprintf(stderr, "input filelen = %d\n", filelen);
    }
    if (fout != stdout)
	fclose(fout);
    if (fin != stdin)
	fclose(fin);
    exit(0);
}
