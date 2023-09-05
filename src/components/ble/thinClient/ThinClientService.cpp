/*  Copyright (C) 2020-2021 JF, Adam Pigg, Avamander

    This file is part of InfiniTime.

    InfiniTime is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    InfiniTime is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "ThinClientService.h"
#include "systemtask/SystemTask.h"

namespace {
  // 0006yyxx-78fc-48fe-8e23-433b3a1942d0
  constexpr ble_uuid128_t CharUuid(uint8_t x, uint8_t y) {
    return ble_uuid128_t {.u = {.type = BLE_UUID_TYPE_128},
                          .value = {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, x, y, 0x06, 0x00}};
  }

  // 00060000-78fc-48fe-8e23-433b3a1942d0
  constexpr ble_uuid128_t BaseUuid() {
    return CharUuid(0x00, 0x00);
  }

  constexpr ble_uuid128_t msUuid {BaseUuid()};

  constexpr ble_uuid128_t msWriteCharUuid {CharUuid(0x01, 0x00)};
  constexpr ble_uuid128_t msReadCharUuid {CharUuid(0x02, 0x00)};

  int ThinClientCallback(uint16_t /*conn_handle*/, uint16_t /*attr_handle*/, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    return static_cast<Pinetime::Controllers::ThinClientService*>(arg)->OnCommand(ctxt);
  }
}

Pinetime::Controllers::ThinClientService::ThinClientService(Pinetime::Controllers::NimbleController& nimble) : nimble(nimble) {
  characteristicDefinition[0] = {.uuid = &msWriteCharUuid.u,
                                 .access_cb = ThinClientCallback,
                                 .arg = this,
                                 .flags = BLE_GATT_CHR_F_WRITE_NO_RSP};
  characteristicDefinition[1] = {.uuid = &msReadCharUuid.u,
                                 .access_cb = ThinClientCallback,
                                 .arg = this,
                                 .flags = BLE_GATT_CHR_F_NOTIFY,
                                 .val_handle = &eventHandle};
  characteristicDefinition[2] = {0};

  serviceDefinition[0] = {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &msUuid.u, .characteristics = characteristicDefinition};
  serviceDefinition[1] = {0};
}

Pinetime::Controllers::ThinClientService::~ThinClientService() {
}

void Pinetime::Controllers::ThinClientService::Init() {
  uint8_t res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

/*void Pinetime::Controllers::ThinClientService::SwapBytes(uint16_t* arr, uint16_t size) {
  for (uint16_t i = 0; i < size; i++)
    arr[i] = (arr[i]>>8) | (arr[i]<<8);
}*/

int Pinetime::Controllers::ThinClientService::OnCommand(struct ble_gatt_access_ctxt* ctxt) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    return 0;
  if (ble_uuid_cmp(ctxt->chr->uuid, &msWriteCharUuid.u) != 0)
    return 0;

  if (thinClient == nullptr)
    return 0;

  os_mbuf* om = ctxt->om;
  state = thinClient->OnData(state, om->om_data, om->om_len);

  return 0;
}

void Pinetime::Controllers::ThinClientService::setClient(IThinClient* ptr) {
    thinClient = ptr;
}

void Pinetime::Controllers::ThinClientService::frameAck(uint8_t id) {
    event(id);
}

void Pinetime::Controllers::ThinClientService::logWrite(std::string message) {
    if (message.length() > LOG_MAX_LENGTH) {
        return;
    }
    event(message.c_str(), message.length());
}

void Pinetime::Controllers::ThinClientService::event(uint8_t event) {
    auto* om = ble_hs_mbuf_from_flat(&event, 1);

    uint16_t connectionHandle = nimble.connHandle();

    if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
      return;
    }

    ble_gattc_notify_custom(connectionHandle, eventHandle, om);
}

void Pinetime::Controllers::ThinClientService::event(const char* event, uint16_t size) {
    auto* om = ble_hs_mbuf_from_flat(event, size);

    uint16_t connectionHandle = nimble.connHandle();

    if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
      return;
    }

    ble_gattc_notify_custom(connectionHandle, eventHandle, om);
}
