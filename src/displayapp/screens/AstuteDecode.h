#pragma once

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

typedef struct {
    receiver_t callback;
    void* cookie;
    AWHeader header;
    char mem[AW_BUFF_SIZE];
    char* buff;
    aw_rgb565_t* palette;
    size_t filled;
    char* ptr;
    EOperation operation;
    size_t offset; //in pixels, where are we
    size_t expect;
    aw_rgb565_t long_fill;
    unsigned char bpp;
} AWDecoder;


inline void aw_decoder_init(AWDecoder* decoder, receiver_t callback, void* cookie)
{
    memset(decoder, 0, sizeof(AWDecoder));

    decoder->callback = callback;
    decoder->cookie = cookie;
    decoder->buff = decoder->mem;
    decoder->ptr = decoder->buff;
    decoder->operation = eoHeader;
    decoder->expect = sizeof(AWHeader);
}

inline double aw_ceil(double num) {
    int integerPart = (int)num;
    double fractionalPart = num - integerPart;

    if (fractionalPart > 0)
        return integerPart + 1;
    else
        return integerPart;
}

inline int aw_decoder_chunk(AWDecoder* decoder)
{
    size_t bytes_left = 0;
    while (true)
    {
        bytes_left = decoder->filled - (decoder->ptr - decoder->buff);
        if (bytes_left < decoder->expect)
            break;

        switch (decoder->operation) //might be expencive to access bits
        {
        case eoHeader:
        {
            decoder->header = *(AWHeader*)decoder->ptr;
            decoder->ptr += sizeof(AWHeader);
            decoder->bpp = 1 << decoder->header.bpp;
            if (decoder->bpp < 16)
            {
                decoder->operation = eoPalette;
                decoder->expect = 2 << decoder->bpp;
            }
            else
            {
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
            size_t to_copy = bytes_left < size ? bytes_left : size;
            memcpy(decoder->palette, decoder->ptr, to_copy);
            decoder->ptr += to_copy;
            bytes_left -= to_copy;
            decoder->buff = decoder->mem + size;
            memcpy(decoder->buff, decoder->ptr, bytes_left);
            decoder->ptr = decoder->buff;
            decoder->filled = bytes_left;
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
                                decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                                decoder->offset += 1;
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
                            decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                            decoder->offset += 1;
                        }
                        count -= pixels_left;
                        bytes_left -= pixels_left;
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
                while (count--) {
                    decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                    decoder->offset += 1;
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
                while (count--) {
                    decoder->callback(decoder->cookie, decoder->offset, 1, &decoder->long_fill);
                    decoder->offset += 1;
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

inline void aw_decoder_fini(AWDecoder* decoder)
{
    size_t bytes_left = AW_BUFF_SIZE - decoder->filled;
    if (bytes_left > sizeof(aw_counter_t) + sizeof(aw_rgb565_t))
    {
        aw_counter_t* ptr = (aw_counter_t*)(decoder->buff + decoder->filled);
        *ptr = 0;
        decoder->filled += sizeof(aw_counter_t) + sizeof(aw_rgb565_t);
        decoder->expect = decoder->filled - (decoder->ptr - decoder->buff);
    }
    aw_decoder_chunk(decoder);
}