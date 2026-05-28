#include "BleConnectionStatus.h"
#include "BLEHostConfiguration.h"

BleConnectionStatus::BleConnectionStatus(void) : _configuration(nullptr)
{
}

void BleConnectionStatus::setConfiguration(const BLEHostConfiguration* config)
{
    _configuration = config;
}

void BleConnectionStatus::onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo)
{
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
    this->connected = false;
}

bool BleConnectionStatus::isConnected(){
    return this->connected;
}

void BleConnectionStatus::onAuthenticationComplete(NimBLEConnInfo& connInfo)
{
    this->connected = true;
}
