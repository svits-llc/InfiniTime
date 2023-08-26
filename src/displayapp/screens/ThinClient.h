#pragma once

#include <lvgl/lvgl.h>
#include <cstdint>
#include "displayapp/screens/Screen.h"
#include "systemtask/SystemTask.h"

#include "components/ble/thinClient/ThinClientService.h"
#include "AstuteDecode.h"

namespace Pinetime {
  namespace Components {
    class LittleVgl;
  }

  namespace Applications {
    namespace Screens {

      class ThinClient : public Screen, public Pinetime::Controllers::IThinClient {
      public:
        ThinClient(System::SystemTask& systemTask, Pinetime::Components::LittleVgl& lvgl, Pinetime::Controllers::ThinClientService& thinClientService);

        ~ThinClient() override;

        bool OnTouchEvent(Pinetime::Applications::TouchEvents event) override;

        bool OnTouchEvent(uint16_t x, uint16_t y) override;

        static void DecodeReceiver(void* cookie, size_t offset, size_t count, uint16_t* pix);

        Pinetime::Controllers::ThinClientService::States OnData(Pinetime::Controllers::ThinClientService::States state, uint8_t* buffer, uint8_t len) override;

        void Refresh() override;

      private:
        Pinetime::System::SystemTask& systemTask;
        Pinetime::Components::LittleVgl& lvgl;
        Pinetime::Controllers::ThinClientService& thinClientService;

        struct {
          uint32_t compressedSize;
        } Image;

        struct {
          uint32_t compressedOffset;
          AWDecoder decoder;
        } Decompress;

        static constexpr uint16_t IMAGE_BUFFER_SIZE_BYTES = 254;
        uint16_t drawPixelsOffset = 0;
        uint16_t imageBufferOffset = 0;
        lv_color_t imageBuffer[(IMAGE_BUFFER_SIZE_BYTES+AW_BUFF_SIZE+1)/sizeof(lv_color_t)];

        uint16_t imageOffset = 0;

        void DrawScreen(lv_color_t* buffer, uint16_t offset, uint16_t count);

        lv_task_t* taskRefresh;
      };
    }
  }
}
