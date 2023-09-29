#ifndef __WAV_H__
#define __WAV_H__

#include <stdint.h>

//#define ALSA_PCM_NEW_HW_PARAMS_API
//#include <alsa/asoundlib.h>

/* AIFF Definitions */

#define IFF_ID_FORM ((uint32_t)0x464f524d) /* "FORM" */
#define IFF_ID_AIFF ((uint32_t)0x41494646) /* "AIFF" */
#define IFF_ID_AIFC ((uint32_t)0x41494643) /* "AIFC" */
#define IFF_ID_COMM ((uint32_t)0x434f4d4d) /* "COMM" */
#define IFF_ID_SSND ((uint32_t)0x53534e44) /* "SSND" */
#define IFF_ID_MPEG ((uint32_t)0x4d504547) /* "MPEG" */
#define IFF_ID_NONE ((uint32_t)0x4e4f4e45) /* "NONE" *//* AIFF-C data format */
#define IFF_ID_2CBE ((uint32_t)0x74776f73) /* "twos" *//* AIFF-C data format */
#define IFF_ID_2CLE ((uint32_t)0x736f7774) /* "sowt" *//* AIFF-C data format */
#define WAV_ID_RIFF ((uint32_t)0x52494646) /* "RIFF" */
#define WAV_ID_WAVE ((uint32_t)0x57415645) /* "WAVE" */
#define WAV_ID_FMT  ((uint32_t)0x666d7420)  /* "fmt " */
#define WAV_ID_DATA ((uint32_t)0x64617461) /* "data" */

#define WAVE_FORMAT_PCM        ((uint16_t)0x0001)
#define WAVE_FORMAT_IEEE_FLOAT ((uint16_t)0x0003)
#define WAVE_FORMAT_ALAW       ((uint16_t)0x0006)
#define WAVE_FORMAT_ULAW       ((uint16_t)0x0007)
#define WAVE_FORMAT_EXTENSIBLE ((uint16_t)0xFFFE)

typedef struct
{
    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t FrameSize;        // when packed (BitsPerSample*Channels+7)/8
    uint16_t BitsPerChannel;
} wav_header_t;

typedef struct
{
    uint16_t cbSize;
    uint16_t ValidBitsPerChannel;
    uint32_t ChannelMask;
    uint16_t AudioFormat;
} xwav_header_t;

static inline uint32_t wav_get_bytes_per_frame(wav_header_t* hdr)
{
    return hdr->NumChannels*((hdr->BitsPerChannel+7)/8);
}

static inline void swap(uint8_t* ptr1, uint8_t* ptr2)
{
    uint8_t t = *ptr1;
    *ptr1 = *ptr2;
    *ptr2 = t;
}

static inline void swap16(void* data)
{
    swap((uint8_t*)data, (uint8_t*)data+1);
}

static inline void swap32(void* data)
{
    swap((uint8_t*)data, (uint8_t*)data+3);
    swap((uint8_t*)data+1, (uint8_t*)data+2);
}

static inline void little16(void* data)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    swap16(data);
#endif
}

static inline void little32(void* data)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    swap32(data);
#endif
}

static inline void big16(void* data)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    swap16(data);
#endif
}

static inline void big32(void* data)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    swap32(data);
#endif
}

static inline uint32_t read_u16le(FILE* f)
{
    uint16_t x;
    if (fread(&x, sizeof(x), 1, f) == sizeof(x)) {
	little16((uint8_t*)&x);
	return x;
    }
    return 0;
}

static inline uint32_t read_u32le(FILE* f)
{
    uint32_t x;
    if (fread(&x, sizeof(x), 1, f) == sizeof(x)) {
	little32((uint8_t*)&x);
	return x;
    }
    return 0;
}

static inline int write_u32le(FILE* f, uint32_t x)
{
    little32((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == sizeof(x));
}

static inline int write_u16le(FILE* f, uint16_t x)
{
    little16((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == 4);
}

static inline int write_32le(FILE* f, int32_t x)
{
    little32((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == sizeof(x));
}

static inline int write_16le(FILE* f, int16_t x)
{
    little16((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == sizeof(x));
}


static inline int write_u32be(FILE* f, uint32_t x)
{
    big32((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == sizeof(x));
}

static inline int write_u16be(FILE* f, uint16_t x)
{
    big16((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == 4);
}

static inline int write_32be(FILE* f, int32_t x)
{
    big32((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == sizeof(x));
}

static inline int write_16be(FILE* f, int16_t x)
{
    big16((uint8_t*)&x);
    return (fwrite(&x, sizeof(x), 1, f) == sizeof(x));
}


static inline void print_tag(FILE* f, uint32_t tag)
{
    fprintf(f, "%c%c%c%c",
	    (tag>>24)&0xff,
	    (tag>>16)&0xff,
	    (tag>>8)&0xff,
	    (tag&0xff));
}

static inline int read_tag(FILE* f, uint32_t* tag)
{
    int n;
    if ((n = fread(tag, sizeof(*tag), 1, f)) == 4) {
	big32(tag);
    }
    return n;
}

static inline int write_tag(FILE* f, uint32_t tag)
{
    big32(&tag);
    return (fwrite(&tag, sizeof(tag), 1, f) ==  4);
}

static inline int write_wav_header(FILE* f, wav_header_t* ptr)
{
    write_16le(f, ptr->AudioFormat);
    write_16le(f, ptr->NumChannels);
    write_32le(f, ptr->SampleRate);
    write_32le(f, ptr->ByteRate);
    write_16le(f, ptr->FrameSize);
    write_16le(f, ptr->BitsPerChannel);
}


#endif
