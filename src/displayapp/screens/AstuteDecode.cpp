#include "AstuteDecode.h"

void aw_decoder_init(AWDecoder* decoder, receiver_t callback, void* cookie)
{
    decoder->callback = callback;
    decoder->cookie = cookie;
    decoder->header.type = ~0x0;
    decoder->header.size = 0;
    decoder->filled = 0;
    decoder->ptr = decoder->buff;
    decoder->copy = 0; //start from skip or fill
    decoder->offset = 0;
}

int aw_decoder_chunk(AWDecoder* decoder)
{
    size_t left = 0;
    while (true)
    {
        left = decoder->filled - (decoder->ptr - decoder->buff);
        if (left < sizeof(AWHeader)) break; //at least 4 bytes required to proceed

        if (decoder->header.size) //might be expencive to access bits
        {
            uint16_t count = *(uint16_t*)decoder->ptr;
            decoder->ptr += sizeof(uint16_t);
            left -= sizeof(uint16_t);
            if (decoder->copy)
            {
                if (count) { // can be zero to ensure tic-tok switch on every pace
                    size_t pixels_left = left / sizeof(uint16_t); //buffer can have less pixels we need to copy
                    pixels_left = (pixels_left < count) ? pixels_left : count;
                    decoder->callback(decoder->cookie, decoder->offset, pixels_left, (uint16_t*)decoder->ptr);
                    decoder->offset += pixels_left;
                    decoder->ptr += pixels_left * sizeof(uint16_t);
                    count -= pixels_left;
                    left -= pixels_left * sizeof(uint16_t);
                }
                if (count) //if we need to wait for more pixels:
                {
                    memcpy(decoder->buff + sizeof(uint16_t), decoder->ptr, left);
                    decoder->filled = left + sizeof(uint16_t);
                    //write back header with lesser count
                    *(uint16_t*)decoder->buff = count;
                    decoder->ptr = decoder->buff;
                    return 0;
                }
                else
                {
                    decoder->copy = !decoder->copy; //???
                }
            }
            else if (etKEY == decoder->header.type)
            {
                uint16_t pix = *(uint16_t*)decoder->ptr;
                decoder->ptr += sizeof(uint16_t);
                while (count--) {
                    decoder->callback(decoder->cookie, decoder->offset, 1, &pix);
                    decoder->offset += 1;
                }
                decoder->copy = !decoder->copy; //???
            }
            else if (etDIFF == decoder->header.type)
            {
                decoder->offset += count;
                decoder->copy = !decoder->copy; //???
            }
        }
        else
        {
            decoder->header = *(AWHeader*)decoder->ptr;
            decoder->ptr += sizeof(AWHeader);
        }
    };
    if (decoder->buff != decoder->ptr)
    {
        memcpy(decoder->buff, decoder->ptr, left);
        decoder->ptr = decoder->buff;
    }
    decoder->filled = left;
    return 0;
}

void aw_decoder_fini(AWDecoder* decoder)
{
    size_t left = AW_BUFF_SIZE - decoder->filled;
    if (left > sizeof(uint16_t) * 2)
    {
        uint16_t* ptr = (uint16_t*)(decoder->buff + decoder->filled);
        *ptr = 0;
        ++ptr;
        *ptr = 0;
        decoder->filled += sizeof(uint16_t) * 2;
    }
    aw_decoder_chunk(decoder);
}