#ifndef __AU_H__
#define __AU_H__

// Define the magic number - audio file format
#define AU_MAGIC ((uint32_t) 0x2e736e64)  // ".snd"

// Define the encoding fields
#define AU_ENCODING_MULAW_8     1   // 8-bit ISDN u-law
#define AU_ENCODING_LINEAR_8    2   // 8-bit linear PCM 
#define AU_ENCODING_LINEAR_16   3   // 16-bit linear PCM 
#define AU_ENCODING_LINEAR_24   4   // 24-bit linear PCM 
#define AU_ENCODING_LINEAR_32   5   // 32-bit linear PCM 
#define AU_ENCODING_FLOAT       6   // 32-bit IEEE floating point 
#define AU_ENCODING_DOUBLE      7   // 64-bit IEEE floating point 
#define AU_ENCODING_ADPCM_G721  23  // 4-bit CCITT g.721 ADPCM 
#define AU_ENCODING_ADPCM_G722  24  // CCITT g.722 ADPCM 
#define AU_ENCODING_ADPCM_G723_3 25 // CCITT g.723 3-bit ADPCM 
#define AU_ENCODING_ADPCM_G723_5 26 // CCITT g.723 5-bit ADPCM 
#define AU_ENCODING_ALAW_8       27 // 8-bit ISDN A-law 

typedef struct
{
    uint32_t  magic;          // magic number 
    uint32_t  data_offset;    // size of this header  (minimum 28)
    uint32_t  data_size;      // length of data (optional) 0xffffffff = unknown
    uint32_t  encoding;       // data encoding format 
    uint32_t  sample_rate;    // samples per second 
    uint32_t  channels;       // number of interleaved channels
    uint8_t   annot[4];       // minimum annotation, even if not used
    uint8_t   annod[];        // rest of annotation data
} au_header_t;

static inline int write_au_header(FILE* f, au_header_t* ptr)
{
    write_32be(f, ptr->magic);
    write_32be(f, ptr->data_offset);
    write_32be(f, ptr->data_size);
    write_32be(f, ptr->encoding);
    write_32be(f, ptr->sample_rate);
    write_32be(f, ptr->channels);
    fwrite(ptr->annot, sizeof(uint8_t),
	   4 + (ptr->data_offset - sizeof(au_header_t)), f); 
}

#endif


