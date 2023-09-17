// https://picheta.me/obfuscator
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum EType
{
    etKEY,
    etDIFF,
    etRMB
};

typedef uint8_t aw_counter_t;
typedef uint16_t aw_rgb565_t;

#define AW_CAP 0b101101
typedef struct
{
    uint32_t cap : 6; //AW_CAP
    uint32_t bpp : 3; //1, 2, 4, 8, 16 as power of 2, so real values are 0 to 4
    uint32_t type : 3; //EType
    uint32_t size : 20;
} AWHeader;


typedef void (*receiver_t)(void* cookie, size_t offset, size_t count, aw_rgb565_t* pix);

#ifndef AW_BUFF_SIZE
    #define AW_BUFF_SIZE 1024
#endif

enum EOperation {
    eoFillSkip,
    eoCopy,
    eoHeader,
    eoPalette,
    eoLongSkipFill //uint16_t
};

static unsigned char* GetAWMem()
{
    static unsigned char* AWDecoderMem = NULL;
    if (!AWDecoderMem)
    {
      AWDecoderMem = (unsigned char*) malloc(AW_BUFF_SIZE);
      unsigned char QRCode[] = { 0, 0, 0, 254, 144, 63, 130, 186, 32, 186, 180, 46, 186, 182, 46, 186, 136, 46, 130, 190, 32, 254, 170, 63, 0, 8, 0, 190, 87, 21, 118, 67, 58, 140, 253, 20, 4, 87, 31, 158, 154, 6, 0, 6, 47, 254, 106, 25, 130, 104, 12, 186, 194, 4, 186, 70, 31, 186, 58, 12, 130, 82, 13, 254, 170, 20 };
      memcpy(AWDecoderMem, QRCode, sizeof(QRCode));
    }
    return AWDecoderMem;
}

typedef struct {
    receiver_t callback;
    void* cookie;
    AWHeader header;
    char* buff;
    char* mem;
    aw_rgb565_t* palette;
    size_t filled;
    char* ptr;
    enum EOperation operation;
    size_t offset; //in pixels, where are we
    size_t expect;
    aw_rgb565_t long_fill;
    unsigned char bpp;
} AWDecoder;


inline void aw_decoder_init(AWDecoder* decoder, receiver_t callback, void* cookie)
{
    char* buff = 0;
    int reinit = AW_CAP == decoder->header.cap;
    if (reinit)
        buff = decoder->buff;
    memset(decoder, 0, sizeof(AWDecoder));
    decoder->mem = (char*)GetAWMem();
    decoder->buff = decoder->mem;
    decoder->ptr = decoder->buff;
    decoder->callback = callback;
    decoder->cookie = cookie;
    decoder->operation = eoHeader;
    decoder->expect = sizeof(AWHeader);
    if (reinit)
    {
        decoder->buff = buff;
        decoder->ptr = decoder->buff;
        decoder->palette = (aw_rgb565_t*)decoder->mem;
    }
}

inline double aw_ceil(double num) {
    int integerPart = (int)num;
    double fractionalPart = num - integerPart;

    if (fractionalPart > 0)
        return integerPart + 1;
    else
        return integerPart;
}

inline int aw_decoder_qr(AWDecoder* decoder)
{
    unsigned char* ptr = GetAWMem();
    if (72 + 240 > AW_BUFF_SIZE) return -1;
    unsigned char* end = ptr + 72;
    size_t offset = 0;
    while (ptr < end) {
        aw_rgb565_t* line = (aw_rgb565_t*)end;
        int count = 3;
        while(count--)
        {
            unsigned char byte = *ptr++;
            for (int i = 0; i < 8; ++i)
            {
                aw_rgb565_t color = (byte & 1) ? 0xFFFF : 0x0000;
                //OutputDebugStringA((byte & 1) ? "@" : " ");
                for (int j = 0; j < 10; ++j)
                {
                    *line++ = color;
                }
                byte >>= 1;
            }
        }
        //OutputDebugStringA("\n");
        for (int i = 0; i < 10; ++i)
        {
            decoder->callback(decoder->cookie, offset, 240, (aw_rgb565_t*)end);
            offset += 240;
        }
    }
    return offset;
}

inline int aw_decoder_chunk(AWDecoder* decoder)
{
    size_t bytes_left = 0;
    while (1)
    {
        bytes_left = decoder->filled - (decoder->ptr - decoder->buff);
        if (bytes_left < decoder->expect)
            break;

        switch (decoder->operation) //might be expencive to access bits
        {
        case eoHeader:
        {
            decoder->header = *(AWHeader*)decoder->ptr;
            if (decoder->header.cap != AW_CAP)
                return -1;
            decoder->ptr += sizeof(AWHeader);
            decoder->bpp = 1 << decoder->header.bpp;
            if (decoder->bpp < 16)
            {
                decoder->operation = eoPalette;
                decoder->expect = sizeof(aw_counter_t) + (2 << decoder->bpp);
            }
            else
            {
                decoder->palette = 0;
                decoder->operation = eoFillSkip;
                decoder->expect = sizeof(aw_counter_t) + aw_ceil(decoder->bpp / 8.);
            }
            break;
        }
        case eoPalette:
        {
            decoder->palette = (aw_rgb565_t*)decoder->mem;
            size_t size = *(aw_counter_t*)decoder->ptr * sizeof(aw_rgb565_t);
            decoder->ptr += sizeof(aw_counter_t);
            bytes_left -= sizeof(aw_counter_t);
            if (!size) //full palette size is 256 and doesn't fit in 0..255 range
                size = sizeof(aw_rgb565_t) * (1 << decoder->bpp);
            if (sizeof(aw_rgb565_t) != size) //special case for "keep old palette"
            {
                size_t to_copy = bytes_left < size ? bytes_left : size;
                memcpy(decoder->palette, decoder->ptr, to_copy);
                decoder->ptr += to_copy;
                bytes_left -= to_copy;
                decoder->buff = decoder->mem + size;
                memcpy(decoder->buff, decoder->ptr, bytes_left);
                decoder->ptr = decoder->buff;
                decoder->filled = bytes_left;
            }
            decoder->operation = eoFillSkip;
            decoder->expect = sizeof(aw_counter_t) + aw_ceil(decoder->bpp / 8.);
            break;
        }
        case eoCopy:
        {
            aw_counter_t count = *(aw_counter_t*)decoder->ptr;
            decoder->ptr += sizeof(aw_counter_t);
            bytes_left -= sizeof(aw_counter_t);
            if (count) { // can be zero to ensure tic-tok switch on every pace
                if (decoder->bpp < 16)
                {
                    size_t pixels_left = bytes_left * 8 / decoder->bpp; //buffer can have less pixels we need to copy
                    pixels_left = (pixels_left < count) ? pixels_left : count;
                    size_t tail_size = (AW_BUFF_SIZE - decoder->filled) / sizeof(aw_rgb565_t);
                    aw_rgb565_t* tail = (tail_size > count) ? (aw_rgb565_t*)(decoder->buff + decoder->filled) : 0;
                    size_t written_tail = 0;
                    if (decoder->bpp < 8)
                    {
                        for (size_t i = 0; i < aw_ceil(pixels_left * decoder->bpp / 8.); ++i)
                        {
                            uint8_t index = *(uint8_t*)decoder->ptr;
                            decoder->ptr += sizeof(uint8_t);
                            for (size_t j = 0; j < (8 / decoder->bpp) && count; ++j)
                            {
                                uint8_t val = index & ((1 << decoder->bpp) - 1);
                                index >>= decoder->bpp;
                                aw_rgb565_t pix = decoder->palette[val];
                                if (tail)
                                    tail[written_tail++] = pix;
                                else {
                                    decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                                    decoder->offset += 1;
                                }
                                --count;
                            }
                            --bytes_left;
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < pixels_left; ++i) {
                            uint8_t index = *(uint8_t*)decoder->ptr;
                            decoder->ptr += sizeof(uint8_t);
                            aw_rgb565_t pix = decoder->palette[index];
                            if (tail)
                                tail[written_tail++] = pix;
                            else {
                                decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                                decoder->offset += 1;
                            }
                        }
                        count -= pixels_left;
                        bytes_left -= pixels_left;
                    }
                    if (tail)
                    {
                        decoder->callback(decoder->cookie, decoder->offset, written_tail, tail);
                        decoder->offset += written_tail;
                    }
                } else {
                    size_t pixels_left = bytes_left / sizeof(aw_rgb565_t); //buffer can have less pixels we need to copy
                    pixels_left = (pixels_left < count) ? pixels_left : count;
                    decoder->callback(decoder->cookie, decoder->offset, pixels_left, (aw_rgb565_t*)decoder->ptr);
                    decoder->offset += pixels_left;
                    decoder->ptr += pixels_left * sizeof(aw_rgb565_t);
                    count -= pixels_left;
                    bytes_left -= pixels_left * sizeof(aw_rgb565_t);
                }
            }
            if (count) //if we need to wait for more pixels:
            {
                memcpy(decoder->buff + sizeof(aw_counter_t), decoder->ptr, bytes_left);
                decoder->filled = bytes_left + sizeof(aw_counter_t);
                //write back header with lesser count
                *(aw_counter_t*)decoder->buff = count;
                decoder->ptr = decoder->buff;
                decoder->expect = aw_ceil(count * decoder->bpp / 8.);
                if (decoder->expect > AW_BUFF_SIZE - decoder->filled)
                    decoder->expect = AW_BUFF_SIZE - decoder->filled;
                return 0;
            }
            decoder->operation = eoFillSkip;
            decoder->expect = sizeof(aw_counter_t) + aw_ceil(decoder->bpp / 8.);
            break;
        }; 
        case eoFillSkip:
        {
            aw_counter_t count = *(aw_counter_t*)decoder->ptr;
            decoder->ptr += sizeof(aw_counter_t);
            if (count)
            {
                if (etKEY == decoder->header.type)
                {
                    aw_rgb565_t pix;
                    if (decoder->bpp < 16)
                    {
                        uint8_t index = *(uint8_t*)decoder->ptr;
                        decoder->ptr += sizeof(uint8_t);
                        pix = decoder->palette[index];
                    }
                    else
                    {
                        pix = *(aw_rgb565_t*)decoder->ptr;
                        decoder->ptr += sizeof(aw_rgb565_t);
                    }
                    if (255 == count)
                    {
                        decoder->long_fill = pix;
                        decoder->operation = eoLongSkipFill;
                        decoder->expect = sizeof(aw_rgb565_t);
                        continue;
                    }

                    size_t tail_size = (AW_BUFF_SIZE - decoder->filled) / sizeof(aw_rgb565_t);
                    aw_rgb565_t* tail = (tail_size > count) ? (aw_rgb565_t*)(decoder->buff + decoder->filled) : 0;
                    size_t written_tail = 0;

                    if (tail) {
                        while (count--)
                            tail[written_tail++] = pix;
                        if (written_tail)
                        {
                            decoder->callback(decoder->cookie, decoder->offset, written_tail, tail);
                            decoder->offset += written_tail;
                        }
                    }
                    else {
                        while (count--) {
                            decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                            decoder->offset += 1;
                        }
                    }
                }
                else if (etDIFF == decoder->header.type)
                {
                    if (255 == count)
                    {
                        decoder->operation = eoLongSkipFill;
                        decoder->expect = sizeof(aw_rgb565_t);
                        continue;
                    }
                    else
                        decoder->offset += count;
                }
            }
            decoder->operation = eoCopy;
            decoder->expect = sizeof(aw_counter_t) + aw_ceil(decoder->bpp / 8.);
            break;
        }
        case eoLongSkipFill:
        {
            aw_rgb565_t count = *(aw_rgb565_t*)decoder->ptr;
            decoder->ptr += sizeof(aw_rgb565_t);
            if (etKEY == decoder->header.type)
            {
                size_t tail_size = (AW_BUFF_SIZE - decoder->filled) / sizeof(aw_rgb565_t);

                if (tail_size > 1) {
                    aw_rgb565_t* tail = (aw_rgb565_t*)(decoder->buff + decoder->filled);
                    while (count)
                    {
                        size_t written_tail = 0;
                        size_t to_fill = count < tail_size ? count : tail_size;
                        while (to_fill--)
                            tail[written_tail++] = decoder->long_fill;
                        decoder->callback(decoder->cookie, decoder->offset, written_tail, tail);
                        decoder->offset += written_tail;
                        count -= written_tail;
                    }
                }
                else {
                    while (count--) {
                        decoder->callback(decoder->cookie, decoder->offset, 1, &decoder->long_fill);
                        decoder->offset += 1;
                    }
                }
            } else
                decoder->offset += count;
            decoder->operation = eoFillSkip;
            decoder->expect = sizeof(aw_counter_t) + aw_ceil(decoder->bpp / 8.);
        }
        }
    };
    if (decoder->buff != decoder->ptr)
    {
        memcpy(decoder->buff, decoder->ptr, bytes_left);
        decoder->ptr = decoder->buff;
    }
    decoder->filled = bytes_left;
    return 0;
}

inline int aw_decoder_fini(AWDecoder* decoder)
{
    if (!decoder->offset) //empty run, maybe QR
        return 0;
    size_t bytes_left = AW_BUFF_SIZE - decoder->filled;
    const size_t noop = sizeof(aw_counter_t) + sizeof(aw_rgb565_t);
    if (bytes_left > noop)
    {
        aw_counter_t* ptr = (aw_counter_t*)(decoder->buff + decoder->filled);
        decoder->filled += noop;
        memset(ptr, 0, noop);
        decoder->expect = decoder->filled - (decoder->ptr - decoder->buff);
    }
    return aw_decoder_chunk(decoder);
}