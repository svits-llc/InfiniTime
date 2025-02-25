#pragma once

#include <lvgl/lvgl.h>
#include <cstdint>
#include "displayapp/screens/Screen.h"
#include "systemtask/SystemTask.h"

#include "components/ble/thinClient/ThinClientService.h"
#include "astute-compression/AstuteDecode.h"
#include "astute-compression/aw_profile.h"

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
          uint32_t compressedSize = 0;
          uint8_t frameId = 0;
        } Image;
        bool qrDrawn = false;

        struct {
          uint32_t compressedOffset = 0;
          AWDecoder decoder = {};
        } Decompress;

        struct {
            struct {
                uint8_t x = 0;
                uint8_t y = 0;
                bool touched = false;
            } Touch;
        } Events;
        uint32_t eventsSendTimestamp = 0;

        void DrawScreen(lv_color_t* buffer, uint16_t offset, uint16_t count);

        static constexpr const char* touchEventFmt = R"("x":%d,"y":%d)";
        static constexpr const char* frameEventFmt = R"("frame":%d)";
        static constexpr const char* nextEventDelimiter = ",";
        static constexpr const char* startEvents = "{";
        static constexpr const char* endEvents = "}";
        static constexpr uint16_t SEND_EVENTS_PERIOD = 500;
        void SendEvents(bool idle);
        lv_task_t* taskRefresh;

        enum MetricType {
            START_DECODER,
            FINISH_DECODER,
            START_RECV_PACKET,
            END_RECV_PACKET,
            START_RECV_FRAME,
            END_RECV_FRAME,
            START_SEND_METRICS,
            END_SEND_METRICS
        };
        AWProfiler Profiler = {};
        void LogMetric(MetricType type);
        void SendMetrics();
      };
    }
  }
}
