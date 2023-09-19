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

bool ThinClient::OnTouchEvent(uint16_t x, uint16_t y) {
  Events.Touch.x = x;
  Events.Touch.y = y;
  Events.Touch.touched = true;
  return true;
}

void ThinClient::DrawScreen(lv_color_t* buffer, uint16_t offset, uint16_t count) {
  uint16_t offsetEnd = offset + count - 1;

  uint16_t startLine = offset / LV_HOR_RES_MAX;
  uint16_t endLine = offsetEnd / LV_HOR_RES_MAX;
  for (uint16_t currLine = startLine; currLine <= endLine; currLine++) {
    lv_area_t area = {
      .x1 = 0,
      .y1 =  currLine,
      .x2 = LV_HOR_RES_MAX - 1,
      .y2 = currLine
    };
    if (currLine == startLine)
      area.x1 = offset % LV_HOR_RES_MAX;
    if (currLine == endLine)
      area.x2 = offsetEnd % LV_HOR_RES_MAX;

    //thinClientService.logWrite("x1:"+std::to_string(area.x1)+",y1:"+ std::to_string(area.y1)+
          //                             ",x2:"+std::to_string(area.x2)+",y2:"+ std::to_string(area.y2));
    //LogMetric(SEND_SCREEN);
    lvgl.SetFullRefresh(Components::LittleVgl::FullRefreshDirections::None);
    lvgl.FlushDisplay(&area, buffer);
    //LogMetric(END_SCREEN);
    buffer += area.x2 - area.x1 + 1;
  }
}

void ThinClient::DecodeReceiver(void* cookie, size_t offset, size_t count, uint16_t* pix) {
  ThinClient* pSelf = reinterpret_cast<ThinClient*>(cookie);

  /*//pSelf->thinClientService.logWrite("Off:"+ std::to_string(offset)+",Cnt:"+ std::to_string(count));

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
  pSelf->imageBufferOffset += count;*/

  pSelf->DrawScreen((lv_color_t*) pix, offset, count);
}

Pinetime::Controllers::ThinClientService::States ThinClient::OnData(Pinetime::Controllers::ThinClientService::States state,
           uint8_t* buffer,
           uint8_t len) {
  using namespace Pinetime::Controllers;

  switch (state) {
    case ThinClientService::States::Wait: {
      //thinClientService.logWrite("NewImg");

      Image = {};
      Image.compressedSize = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
      Image.frameId = buffer[4];

      state = ThinClientService::States::ReceiveImage;


      Decompress.compressedOffset = 0;
      aw_decoder_init(&Decompress.decoder, DecodeReceiver, this);
      break;
    }

    case ThinClientService::States::ReceiveImage: {
      uint8_t* ptr = buffer;
      uint8_t bufLen = len;
      LogMetric(START_DECODER);
      while (bufLen) {
        size_t left = AW_BUFF_SIZE - Decompress.decoder.filled;
        size_t size = bufLen < left ? bufLen : left;

        memcpy(Decompress.decoder.buff + Decompress.decoder.filled, ptr, size);
        Decompress.decoder.filled += size;

        aw_decoder_chunk(&Decompress.decoder);
        bufLen -= size;
        ptr += size;
      }
      LogMetric(FINISH_DECODER);

      Decompress.compressedOffset += len;

      if (Decompress.compressedOffset == Image.compressedSize) {
        aw_decoder_fini(&Decompress.decoder, false);
        //DrawScreen(imageBuffer, drawPixelsOffset, imageBufferOffset);

        SendEvents(false);

        state = ThinClientService::States::Wait;
      }

      break;
    }

    default:
      break;
  }
  return state;
}

void ThinClient::LogMetric(MetricType type) {
    std::ignore = type;
    /*switch(type) {
        case START_DECODER:
            startDecoderTimestamps[startDecoderCnt] = lv_tick_get();
            startDecoderCnt++;
            break;
        case FINISH_DECODER:
            finishDecoderTimestamps[finishDecoderCnt] = lv_tick_get();
            finishDecoderCnt++;
            break;
        case SEND_SCREEN:
            sendScreenTimestamps[sendScreenCnt] = lv_tick_get();
            sendScreenCnt++;
            break;
        case END_SCREEN:
            endScreenTimestamps[endScreenCnt] = lv_tick_get();
            endScreenCnt++;
            break;
    }
    if (startDecoderCnt >= MAX_METRIC_CNT || finishDecoderCnt >= MAX_METRIC_CNT
    || sendScreenCnt >= MAX_METRIC_CNT || endScreenCnt >= MAX_METRIC_CNT)
        SendMetrics();*/
}

void ThinClient::SendMetrics() {
    /*
    uint8_t metricsLen = 0;

    uint32_t firstTimestamp = UINT32_MAX;
    if (startDecoderTimestamps[0] < firstTimestamp) firstTimestamp = startDecoderTimestamps[0];
    if (finishDecoderTimestamps[0] < firstTimestamp) firstTimestamp = finishDecoderTimestamps[0];
    if (sendScreenTimestamps[0] < firstTimestamp) firstTimestamp = sendScreenTimestamps[0];
    if (endScreenTimestamps[0] < firstTimestamp) firstTimestamp = endScreenTimestamps[0];

    metricsLen += sprintf(responseBuffer+metricsLen, "%s", "SD:");
    for (uint8_t i = 0; i < startDecoderCnt; i++)
        metricsLen += sprintf(responseBuffer+metricsLen, "%d,", int16_t (startDecoderTimestamps[i]-firstTimestamp));

    metricsLen += sprintf(responseBuffer+metricsLen, "%s", "\nFD:");
    for (uint8_t i = 0; i < finishDecoderCnt; i++)
        metricsLen += sprintf(responseBuffer+metricsLen, "%d,", int16_t (finishDecoderTimestamps[i]-firstTimestamp));

    metricsLen += sprintf(responseBuffer+metricsLen, "%s", "\nSS:");
    for (uint8_t i = 0; i < sendScreenCnt; i++)
        metricsLen += sprintf(responseBuffer+metricsLen, "%d,", int16_t (sendScreenTimestamps[i]-firstTimestamp));

    metricsLen += sprintf(responseBuffer+metricsLen, "%s", "\nES:");
    for (uint8_t i = 0; i < endScreenCnt; i++)
        metricsLen += sprintf(responseBuffer+metricsLen, "%d,", int16_t (endScreenTimestamps[i]-firstTimestamp));

    thinClientService.event(responseBuffer, metricsLen);

    startDecoderCnt = 0;
    finishDecoderCnt = 0;
    sendScreenCnt = 0;
    endScreenCnt = 0;*/
}

void ThinClient::SendEvents(bool idle) {
    uint32_t currTimestamp = lv_tick_get();
    if (idle && currTimestamp - eventsSendTimestamp < SEND_EVENTS_PERIOD)
        return;
    eventsSendTimestamp = currTimestamp;

    uint8_t eventLen = 0;
    strcpy(responseBuffer, startEvents);
    eventLen += strlen(startEvents);

    if (!idle)
        eventLen += sprintf(responseBuffer+eventLen, frameEventFmt, Image.frameId);
    if (Events.Touch.touched) {
        if (eventLen > 1) {
            strcpy(responseBuffer+eventLen, nextEventDelimiter);
            eventLen += strlen(nextEventDelimiter);
        }
        eventLen += sprintf(responseBuffer+eventLen, touchEventFmt, Events.Touch.x, Events.Touch.y);
    }

    strcpy(responseBuffer+eventLen, endEvents);
    eventLen += strlen(endEvents);

    thinClientService.event(responseBuffer, eventLen);
    Events = {};
}

void ThinClient::Refresh() {
    if (!qrDrawn) {
        aw_decoder_init(&Decompress.decoder, DecodeReceiver, this);
        aw_decoder_qr(&Decompress.decoder);
        aw_decoder_fini(&Decompress.decoder, false);
        //DrawScreen(imageBuffer, drawPixelsOffset, imageBufferOffset);
        qrDrawn = true;
    }
    SendEvents(true);
}