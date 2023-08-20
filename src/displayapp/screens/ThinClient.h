#pragma once

#include <lvgl/lvgl.h>
#include <cstdint>
#include "displayapp/screens/Screen.h"
#include "systemtask/SystemTask.h"

#include "components/ble/thinClient/ThinClientService.h"

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

        Pinetime::Controllers::ThinClientService::States OnData(Pinetime::Controllers::ThinClientService::States state, uint8_t* buffer, uint8_t len) override;

        void Refresh() override;

      private:
        Pinetime::System::SystemTask& systemTask;
        Pinetime::Components::LittleVgl& lvgl;
        Pinetime::Controllers::ThinClientService& thinClientService;

        uint8_t currentLine = 0;

        enum class ImageCompressFormats : uint8_t {
          LZ4 = 0x00
        };

        struct {
          uint32_t compressedSize;
          ImageCompressFormats format;
        } Image;

        struct {
          uint32_t compressedOffset;
        } Decompress;

        /*struct {
          uint32_t compressedOffset;
          LZ4_streamDecode_t* lz4StreamDecode;
        } LZ4Decompress;*/

        static constexpr uint16_t IMAGE_BUFFER_SIZE = LV_HOR_RES_MAX*4;
        uint16_t imageBufferOffset = 0;
        lv_color_t imageBuffer[IMAGE_BUFFER_SIZE + Pinetime::Controllers::ThinClientService::CHUNK_SIZE/2 + 1];

        void DrawScreen(lv_color_t* buffer);

        lv_task_t* taskRefresh;
      };
    }
  }
}
