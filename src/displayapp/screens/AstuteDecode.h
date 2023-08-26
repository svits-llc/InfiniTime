#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

enum EType
{
    etKEY,
    etDIFF,
    etRMB
};

typedef struct
{
    uint32_t reserved : 8;
    uint32_t type : 4;
    uint32_t size : 20;
} AWHeader;


typedef void (*receiver_t)(void* cookie, size_t offset, size_t count, uint16_t* pix);

#ifndef AW_BUFF_SIZE
    #define AW_BUFF_SIZE 256
#endif

typedef struct {
    receiver_t callback;
    void* cookie;
    AWHeader header;
    char buff[AW_BUFF_SIZE];
    size_t filled;
    char* ptr;
    char copy; //boolean
    size_t offset; //in pixels, where are we
} AWDecoder;

void aw_decoder_init(AWDecoder* decoder, receiver_t callback, void* cookie);
int aw_decoder_chunk(AWDecoder* decoder);
void aw_decoder_fini(AWDecoder* decoder);