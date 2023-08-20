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

void ThinClient::DrawScreen(lv_color_t* buffer) {
  lv_area_t area = {
    .x1 = 0,
    .y1 = currentLine,
    .x2 = LV_HOR_RES_MAX - 1,
    .y2 = currentLine + 3
  };

  lvgl.SetFullRefresh(Components::LittleVgl::FullRefreshDirections::None);
  lvgl.FlushDisplay(&area, buffer);

  currentLine += 4;
  if (currentLine >= LV_VER_RES_MAX)
    currentLine = 0;
}

Pinetime::Controllers::ThinClientService::States ThinClient::OnData(Pinetime::Controllers::ThinClientService::States state,
           uint8_t* buffer,
           uint8_t len) {
  using namespace Pinetime::Controllers;
  switch (state) {
    case ThinClientService::States::Wait: {
      Image.compressedSize = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
      Image.format = (ImageCompressFormats) buffer[4];

      state = ThinClientService::States::ReceiveImage;
      imageBufferOffset = 0;
      Decompress.compressedOffset = 0;
      /*LZ4Decompress.compressedOffset = 0;
      LZ4Decompress.lz4StreamDecode = LZ4_createStreamDecode();*/
      break;
    }

    case ThinClientService::States::ReceiveImage: {
      /*int chunk_size = LZ4_decompress_safe_continue(LZ4Decompress.lz4StreamDecode,
      (const char*) buffer,
      (char*) imageBuffer + imageBufferOffset,
      ThinClientService::CHUNK_SIZE,
      ThinClientService::CHUNK_SIZE);*/

      /*if (chunk_size <= 0) { // decompress error
        //LZ4_freeStreamDecode(LZ4Decompress.lz4StreamDecode);
        state = ThinClientService::States::Wait;
        break;
      }*/
      memcpy(((uint8_t*) imageBuffer) + imageBufferOffset, buffer, len);

      Decompress.compressedOffset += len;
      //imageBufferOffset += chunk_size;
      imageBufferOffset += len;

      if (imageBufferOffset >= IMAGE_BUFFER_SIZE*sizeof(lv_color_t)) {
        DrawScreen(imageBuffer);
        memcpy(imageBuffer, ((uint8_t*) imageBuffer) + IMAGE_BUFFER_SIZE*sizeof(lv_color_t), imageBufferOffset - IMAGE_BUFFER_SIZE*sizeof(lv_color_t));
        imageBufferOffset = imageBufferOffset - IMAGE_BUFFER_SIZE*sizeof(lv_color_t);
      }

      if (Decompress.compressedOffset == Image.compressedSize) {
        //LZ4_freeStreamDecode(LZ4Decompress.lz4StreamDecode);
        state = ThinClientService::States::Wait;
        break;
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