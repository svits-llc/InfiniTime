#include "displayapp/screens/ThinClient.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/LittleVgl.h"

using namespace Pinetime::Applications::Screens;

ThinClient::ThinClient(System::SystemTask& systemTask, Pinetime::Components::LittleVgl& lvgl, Pinetime::Controllers::ThinClientService& thinClientService)
  : systemTask {systemTask}, lvgl {lvgl}, thinClientService {thinClientService} {
  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  thinClientService.setClient(this);
}

ThinClient::~ThinClient() {
  thinClientService.setClient(nullptr);
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

bool ThinClient::OnTouchEvent(Pinetime::Applications::TouchEvents /*event*/) {
  return true;
}

bool ThinClient::OnTouchEvent(uint16_t /*x*/, uint16_t /*y*/) {
  return true;
}

void ThinClient::DrawScreen(lv_color_t* buffer, uint16_t offset, uint16_t count) {
  uint16_t offsetEnd = offset + count;

  uint16_t startLine = offset / LV_HOR_RES_MAX;
  uint16_t endLine = offsetEnd / LV_HOR_RES_MAX;
  for (uint16_t currLine = startLine; currLine <= endLine; currLine++) {
    lv_area_t area = {
      .x1 = 0,
      .y1 = currLine,
      .x2 = LV_HOR_RES_MAX - 1,
      .y2 = currLine
    };
    if (currLine == startLine)
      area.x1 = offset % LV_HOR_RES_MAX;
    if (currLine == endLine)
      area.x2 = offsetEnd % LV_HOR_RES_MAX;
    lvgl.SetFullRefresh(Components::LittleVgl::FullRefreshDirections::None);
    lvgl.FlushDisplay(&area, buffer);
    buffer += area.x2 - area.x1;
  }
}

void ThinClient::DecodeReceiver(void* cookie, size_t offset, size_t count, uint16_t* pix) {
  ThinClient* pSelf = reinterpret_cast<ThinClient*>(cookie);
  // TODO: diff mode
  if (offset != pSelf->drawPixelsOffset + pSelf->imageBufferOffset) {
   pSelf->DrawScreen(pSelf->imageBuffer, pSelf->drawPixelsOffset, pSelf->imageBufferOffset);
   pSelf->drawPixelsOffset = offset;
   pSelf->imageBufferOffset = 0;
  }
  memcpy(pSelf->imageBuffer + pSelf->imageBufferOffset, pix, count*sizeof(uint16_t));
  pSelf->imageBufferOffset += count;
  if (pSelf->imageBufferOffset >= IMAGE_BUFFER_SIZE_BYTES/sizeof(lv_color_t)) {
    pSelf->DrawScreen(pSelf->imageBuffer, pSelf->drawPixelsOffset, pSelf->imageBufferOffset);
    pSelf->drawPixelsOffset = offset;
    memcpy(pSelf->imageBuffer, ((uint8_t*) pSelf->imageBuffer) + pSelf->IMAGE_BUFFER_SIZE_BYTES, pSelf->imageBufferOffset*sizeof(lv_color_t) - pSelf->IMAGE_BUFFER_SIZE_BYTES);
    pSelf->imageBufferOffset = pSelf->imageBufferOffset*sizeof(lv_color_t) - pSelf->IMAGE_BUFFER_SIZE_BYTES;
  }
}

Pinetime::Controllers::ThinClientService::States ThinClient::OnData(Pinetime::Controllers::ThinClientService::States state,
           uint8_t* buffer,
           uint8_t len) {
  using namespace Pinetime::Controllers;
  switch (state) {
    case ThinClientService::States::Wait: {
      Image.compressedSize = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];

      state = ThinClientService::States::ReceiveImage;
      drawPixelsOffset = 0;
      imageBufferOffset = 0;
      imageOffset = 0;
      Decompress.compressedOffset = 0;
      Decompress.decoder = {};
      //aw_decoder_init(&Decompress.decoder, DecodeReceiver, this);
      break;
    }

    case ThinClientService::States::ReceiveImage: {

      uint8_t* ptr = buffer;
      /*while (len) {
        size_t left = AW_BUFF_SIZE - Decompress.decoder.filled;
        size_t size = len < left ? len : left;

        memcpy(Decompress.decoder.buff + Decompress.decoder.filled, ptr, size);
        Decompress.decoder.filled += size;

        aw_decoder_chunk(&Decompress.decoder);
        len -= size;
        ptr += size;
      }*/

      DecodeReceiver(this, imageOffset, len/sizeof(uint16_t), reinterpret_cast<uint16_t *>(buffer));

      imageOffset += len/sizeof(uint16_t);
      Decompress.compressedOffset += len;

      if (Decompress.compressedOffset == Image.compressedSize) {
        //aw_decoder_fini(&Decompress.decoder);
        DrawScreen(imageBuffer, drawPixelsOffset, imageBufferOffset);
        state = ThinClientService::States::Wait;
      }

      break;
    }

    default:
      break;
  }
  return state;
}

void ThinClient::Refresh() {
  /*if (thinClientService.getFrame(currentFrame)) {
    lvgl.SetFullRefresh(Components::LittleVgl::FullRefreshDirections::None);
    lvgl.FlushDisplay(&currentFrame.area, currentFrame.data);
  }*/
}