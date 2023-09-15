#pragma once

#include <cstdint>
#include <string>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_uuid.h>
#undef max
#undef min

namespace Pinetime {
  namespace Controllers {
    class NimbleController;

    class IThinClient;
    class ThinClientService {
    public:
      explicit ThinClientService(NimbleController& nimble);
      ~ThinClientService();

      void Init();

      void event(uint8_t event);

      void event(const char* data, uint16_t size);

      int OnCommand(struct ble_gatt_access_ctxt* ctxt);

      enum class States : uint8_t { Wait, ReceiveImage };

      void setClient(IThinClient* ptr);

      static constexpr uint8_t CHUNK_SIZE = 244;

      void logWrite(std::string message);

    private:
      struct ble_gatt_chr_def characteristicDefinition[3];
      struct ble_gatt_svc_def serviceDefinition[2];

      States state = States::Wait;

      IThinClient* thinClient = nullptr;

      //void SwapBytes(uint16_t* arr, uint16_t size);

      uint16_t eventHandle {};

      NimbleController& nimble;

      static constexpr uint16_t LOG_MAX_LENGTH = 244;
    };

    class IThinClient {
      public:
          virtual ~IThinClient() {}
          virtual ThinClientService::States OnData(ThinClientService::States state, uint8_t* buffer, uint8_t len) = 0;
    };
  }
}
