#include "ble_link.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include <atomic>
#include <cstring>
#include "log.h"
#include "prelude/frame_assembler.h"

#ifndef PRELUDE_CONTROLLER_WHITELIST
#define PRELUDE_CONTROLLER_WHITELIST 1
#endif

namespace ble_link {
namespace {

constexpr char kServiceUuid[] = "7e1d0000-6b6f-4a6b-9f1a-5072656c7564";
constexpr char kInfoUuid[]    = "7e1d0001-6b6f-4a6b-9f1a-5072656c7564";
constexpr char kCommandUuid[] = "7e1d0002-6b6f-4a6b-9f1a-5072656c7564";
constexpr char kFrameUuid[]   = "7e1d0003-6b6f-4a6b-9f1a-5072656c7564";
constexpr char kEventUuid[]   = "7e1d0004-6b6f-4a6b-9f1a-5072656c7564";
constexpr char kSensorsUuid[] = "7e1d0005-6b6f-4a6b-9f1a-5072656c7564";

Callbacks g_cb{};
NimBLEServer* g_server = nullptr;
NimBLECharacteristic* g_info = nullptr;
NimBLECharacteristic* g_command = nullptr;
NimBLECharacteristic* g_frame = nullptr;
NimBLECharacteristic* g_event = nullptr;
NimBLECharacteristic* g_sensors = nullptr;
NimBLECharacteristic* g_battery = nullptr;
prelude::FrameAssembler* g_assembler = nullptr;

std::atomic<bool> g_renderBusy{false};
std::atomic<bool> g_subscribed{false};
char g_name[16] = "Prelude-????";
char g_peerStr[18] = "";
bool g_bonded = false;
NimBLEAddress g_bondedAddr;
// Peer the device is locked to, captured when advertising starts (before any
// connection) so the check does not depend on callback ordering.
bool g_locked = false;
NimBLEAddress g_lockedPeer;
uint32_t g_droppedChunks = 0;
ConnParams g_connParams{12, 24, 0, 400};  // Performance defaults until power::begin runs

void applyConnParams(uint16_t connHandle) {
  g_server->updateConnParams(connHandle, g_connParams.minInterval, g_connParams.maxInterval,
                             g_connParams.latency, g_connParams.timeout);
}

void refreshBondState() {
  g_bonded = NimBLEDevice::getNumBonds() > 0;
  if (g_bonded) {
    g_bondedAddr = NimBLEDevice::getBondedAddress(0);
    strncpy(g_peerStr, g_bondedAddr.toString().c_str(), sizeof g_peerStr - 1);
    g_peerStr[sizeof g_peerStr - 1] = '\0';
  } else {
    g_peerStr[0] = '\0';
  }
}

void startAdvertising() {
  refreshBondState();
  g_locked = g_bonded;
  g_lockedPeer = g_bondedAddr;
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
#if PRELUDE_CONTROLLER_WHITELIST
  if (g_bonded) {
    NimBLEDevice::whiteListAdd(g_bondedAddr);
    adv->setScanFilter(false, true);
  } else {
    adv->setScanFilter(false, false);
  }
#endif
  adv->start();
  LOG("ble: advertising as %s (%s)", g_name, g_bonded ? "whitelisted" : "open pairing");
}

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    LOG("ble: connected %s", info.getAddress().toString().c_str());
    // A bonded host usually encrypts the link before this runs; only ask for
    // security when it has not, so an unpaired host is made to pair now.
    if (!info.isEncrypted()) NimBLEDevice::startSecurity(info.getConnHandle());
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    LOG("ble: disconnected (reason %d)", reason);
    g_subscribed = false;
    g_assembler->abort();
    if (g_cb.onDisconnected) g_cb.onDisconnected();
    startAdvertising();
  }

  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (!info.isEncrypted()) {
      LOG("ble: encryption failed, disconnecting");
      g_server->disconnect(info);
      return;
    }
    NimBLEAddress peer = info.getIdAddress();
    if (g_locked && !(peer == g_lockedPeer)) {
      LOG("ble: rejecting unknown peer %s (device is paired to %s)", peer.toString().c_str(), g_peerStr);
      NimBLEDevice::deleteBond(peer);
      g_server->disconnect(info);
      return;
    }
    refreshBondState();
    LOG("ble: authenticated %s bonded=%d mtu=%u", g_peerStr, info.isBonded(), info.getMTU());
    applyConnParams(info.getConnHandle());
    if (g_cb.onConnected) g_cb.onConnected(g_peerStr);
  }

  void onConnParamsUpdate(NimBLEConnInfo& info) override {
    LOG("ble: conn interval %.2f ms latency %u timeout %u ms", info.getConnInterval() * 1.25f,
        (unsigned)info.getConnLatency(), (unsigned)info.getConnTimeout() * 10u);
  }

  void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override { LOG("ble: mtu %u", mtu); }
};

class CommandCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    prelude::ParsedCommand cmd = prelude::parseCommand(v.data(), v.size());
    if (!cmd.valid) {
      LOG("ble: bad command opcode=0x%02X len=%u", cmd.opcode, (unsigned)v.size());
      sendAck(cmd.opcode, prelude::AckStatus::BadArg);
      return;
    }
    switch (static_cast<prelude::Opcode>(cmd.opcode)) {
      case prelude::Opcode::FrameBegin: {
        if (g_renderBusy.load()) {
          sendAck(cmd.opcode, prelude::AckStatus::Busy);
          return;
        }
        if (g_assembler->expireIfStale(millis())) LOG("ble: discarded stale frame");
        prelude::AckStatus st = g_assembler->begin(cmd.frameLength, cmd.frameCrc, millis());
        g_droppedChunks = 0;
        sendAck(cmd.opcode, st);
        return;
      }
      case prelude::Opcode::FrameEnd: {
        prelude::AckStatus st = g_assembler->end();
        if (g_droppedChunks) LOG("ble: %lu chunks dropped", (unsigned long)g_droppedChunks);
        if (st != prelude::AckStatus::Ok) {
          LOG("ble: frame rejected status=%u received=%lu", (unsigned)st,
              (unsigned long)g_assembler->received());
          sendAck(cmd.opcode, st);
          return;
        }
        g_renderBusy = true;
        if (!g_cb.onFrameReady || !g_cb.onFrameReady()) {
          g_renderBusy = false;
          sendAck(cmd.opcode, prelude::AckStatus::Busy);
        }
        return;
      }
      default:
        if (!g_cb.onCommand || !g_cb.onCommand(cmd)) sendAck(cmd.opcode, prelude::AckStatus::Busy);
        return;
    }
  }
};

class FrameCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    prelude::FrameChunk ch = prelude::parseFrameChunk(v.data(), v.size());
    if (!ch.valid || !g_assembler->chunk(ch.offset, ch.data, ch.len)) ++g_droppedChunks;
  }
};

class EventCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
    g_subscribed = (subValue & 0x0001) != 0;
    LOG("ble: events %s", g_subscribed ? "subscribed" : "unsubscribed");
  }
};

}  // namespace

void begin(const Callbacks& cb, uint8_t* frameBuffer, bool clearBonds) {
  g_cb = cb;
  static prelude::FrameAssembler assembler(frameBuffer, prelude::kFrameBytes);
  g_assembler = &assembler;

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  snprintf(g_name, sizeof g_name, "Prelude-%02X%02X", mac[4], mac[5]);

  NimBLEDevice::init(g_name);
  if (clearBonds) {
    NimBLEDevice::deleteAllBonds();
    LOG("ble: all bonds cleared");
  }
  NimBLEDevice::setMTU(517);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(new ServerCb());
  g_server->advertiseOnDisconnect(false);

  NimBLEService* svc = g_server->createService(kServiceUuid);
  g_info = svc->createCharacteristic(kInfoUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC);
  g_command = svc->createCharacteristic(kCommandUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC, 512);
  g_frame = svc->createCharacteristic(kFrameUuid, NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC, 517);
  g_event = svc->createCharacteristic(kEventUuid, NIMBLE_PROPERTY::NOTIFY);
  g_sensors = svc->createCharacteristic(kSensorsUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC);
  g_command->setCallbacks(new CommandCb());
  g_frame->setCallbacks(new FrameCb());
  g_event->setCallbacks(new EventCb());

  NimBLEService* bat = g_server->createService("180F");
  g_battery = bat->createCharacteristic("2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  // Services are registered when advertising starts the GATT server.
  // Enable the scan response first so setName() places the name there and the
  // 128-bit service UUID fits in the 31-byte advertising packet.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->enableScanResponse(true);
  adv->setName(g_name);
  adv->addServiceUUID(kServiceUuid);
  startAdvertising();
}

const char* deviceName() { return g_name; }
bool isBonded() { return g_bonded; }
const char* bondedPeer() { return g_peerStr; }

void notifyEvent(const uint8_t* data, size_t len) {
  if (g_event && g_subscribed.load()) g_event->notify(data, len);
}

void sendAck(uint8_t opcode, prelude::AckStatus status) {
  uint8_t buf[3];
  prelude::encodeAck(opcode, status, buf);
  notifyEvent(buf, sizeof buf);
}

void setInfo(const prelude::DeviceInfo& info) {
  uint8_t buf[prelude::kInfoSize];
  prelude::encodeInfo(info, buf);
  if (g_info) g_info->setValue(buf, sizeof buf);
}

void setSensors(const uint8_t* tlv, size_t len) {
  if (g_sensors) g_sensors->setValue(tlv, len);
}

void setBatteryLevel(uint8_t percent) {
  if (!g_battery) return;
  g_battery->setValue(&percent, 1);
  if (g_server && g_server->getConnectedCount() > 0) g_battery->notify();
}

void setRenderBusy(bool busy) { g_renderBusy = busy; }

void setConnParams(const ConnParams& p) {
  g_connParams = p;
  if (!g_server) return;
  const uint8_t n = g_server->getConnectedCount();
  for (uint8_t i = 0; i < n; ++i) applyConnParams(g_server->getPeerInfo(i).getConnHandle());
}
const uint8_t* frameData() { return g_assembler ? g_assembler->data() : nullptr; }

}  // namespace ble_link
