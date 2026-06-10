#include "BleConnectionStatus.h"
#include "BLEHostConfiguration.h"
#include "esp_log.h"

static const char* BLE_CONN_TAG = "BleConn";

BleConnectionStatus::BleConnectionStatus(void) : _configuration(nullptr)
{
}

void BleConnectionStatus::setConfiguration(const BLEHostConfiguration* config)
{
    _configuration = config;
}

void BleConnectionStatus::onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo)
{
    ESP_LOGI(BLE_CONN_TAG, "PS5 connected — addr=%s connHandle=%d",
             connInfo.getAddress().toString().c_str(), connInfo.getConnHandle());
    uint16_t minInterval = 6;
    uint16_t maxInterval = 7;
    uint16_t latency = 0;
    uint16_t timeout = 600;
    
    if (_configuration) {
        minInterval = _configuration->getMinConnectionInterval();
        maxInterval = _configuration->getMaxConnectionInterval();
        latency = _configuration->getSlaveLatency();
        timeout = _configuration->getSupervisionTimeout();
    }

    pServer->updateConnParams(connInfo.getConnHandle(), minInterval, maxInterval, latency, timeout);
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
    ESP_LOGI(BLE_CONN_TAG, "PS5 disconnected — reason=0x%02X connHandle=%d", reason, connInfo.getConnHandle());
    this->connected = false;
}

bool BleConnectionStatus::isConnected(){
    return this->connected;
}

void BleConnectionStatus::onAuthenticationComplete(NimBLEConnInfo& connInfo)
{
    ESP_LOGI(BLE_CONN_TAG, "Auth complete — bonded=%d encrypted=%d connHandle=%d",
             connInfo.isBonded(), connInfo.isEncrypted(), connInfo.getConnHandle());
    this->connected = true;
}
