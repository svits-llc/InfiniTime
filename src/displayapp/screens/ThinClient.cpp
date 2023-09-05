#include "displayapp/screens/ThinClient.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/LittleVgl.h"

using namespace Pinetime::Applications::Screens;

ThinClient::ThinClient(System::SystemTask& systemTask, Pinetime::Components::LittleVgl& lvgl, Pinetime::Controllers::ThinClientService& thinClientService)
  : systemTask {systemTask}, lvgl {lvgl}, thinClientService {thinClientService} {
  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  thinClientService.setClient(this);
  systemTask.PushMessage(Pinetime::System::Messages::DisableSleeping);
}

ThinClient::~ThinClient() {
  thinClientService.setClient(nullptr);
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
  systemTask.PushMessage(Pinetime::System::Messages::EnableSleeping);
}

bool ThinClient::OnTouchEvent(Pinetime::Applications::TouchEvents /*event*/) {
  return true;
}

bool ThinClient::OnTouchEvent(uint16_t /*x*/, uint16_t /*y*/) {
  return true;
}

void ThinClient::DrawScreen(lv_color_t* buffer, uint16_t offset, uint16_t count) {
  uint16_t offsetEnd = offset + count - 1;

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

    //thinClientService.logWrite("x1:"+std::to_string(area.x1)+",y1:"+ std::to_string(area.y1)+
          //                             ",x2:"+std::to_string(area.x2)+",y2:"+ std::to_string(area.y2));

    lvgl.SetFullRefresh(Components::LittleVgl::FullRefreshDirections::None);
    lvgl.FlushDisplay(&area, buffer);
    buffer += area.x2 - area.x1 + 1;
  }
}

void ThinClient::DecodeReceiver(void* cookie, size_t offset, size_t count, uint16_t* pix) {
  ThinClient* pSelf = reinterpret_cast<ThinClient*>(cookie);

  //pSelf->thinClientService.logWrite("Off:"+ std::to_string(offset)+",Cnt:"+ std::to_string(count));

  if (pSelf->Decompress.callbackFirstCall) {
      pSelf->drawPixelsOffset = offset;
      pSelf->Decompress.callbackFirstCall = false;
  }
  if (offset != pSelf->drawPixelsOffset + pSelf->imageBufferOffset
  || pSelf->imageBufferOffset >= IMAGE_BUFFER_SIZE_BYTES/sizeof(lv_color_t)) {
   pSelf->DrawScreen(pSelf->imageBuffer, pSelf->drawPixelsOffset, pSelf->imageBufferOffset);
   pSelf->drawPixelsOffset = offset;
   pSelf->imageBufferOffset = 0;
  }
  memcpy(pSelf->imageBuffer + pSelf->imageBufferOffset, pix, count*sizeof(uint16_t));
  pSelf->imageBufferOffset += count;
}

Pinetime::Controllers::ThinClientService::States ThinClient::OnData(Pinetime::Controllers::ThinClientService::States state,
           uint8_t* buffer,
           uint8_t len) {
  using namespace Pinetime::Controllers;

  switch (state) {
    case ThinClientService::States::Wait: {
      //thinClientService.logWrite("NewImg");

      Image.compressedSize = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
      Image.id = buffer[4];

      state = ThinClientService::States::ReceiveImage;

      drawPixelsOffset = 0;
      imageBufferOffset = 0;
      Decompress.compressedOffset = 0;
      Decompress.decoder = {};
      Decompress.callbackFirstCall = true;
      aw_decoder_init(&Decompress.decoder, DecodeReceiver, this);
      break;
    }

    case ThinClientService::States::ReceiveImage: {
      uint8_t crc = 0;
      for (uint16_t i = 0; i < len; i++)
          crc += buffer[i];

      uint8_t* ptr = buffer;
      uint8_t bufLen = len;
      while (bufLen) {
        size_t left = AW_BUFF_SIZE - Decompress.decoder.filled;
        size_t size = bufLen < left ? bufLen : left;

        memcpy(Decompress.decoder.buff + Decompress.decoder.filled, ptr, size);
        Decompress.decoder.filled += size;

        aw_decoder_chunk(&Decompress.decoder);
        bufLen -= size;
        ptr += size;
      }

      Decompress.compressedOffset += len;

      if (Decompress.compressedOffset == Image.compressedSize) {
        aw_decoder_fini(&Decompress.decoder);
        DrawScreen(imageBuffer, drawPixelsOffset, imageBufferOffset);
        thinClientService.frameAck(Image.id);
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