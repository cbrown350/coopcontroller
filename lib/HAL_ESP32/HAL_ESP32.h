#ifndef __HAL_ESP32_H__
#define __HAL_ESP32_H__

#include "IHAL.h"
class HAL_ESP32 : public IHAL
{
public:
    void SerialPrintf(const char* format, ...) override;
    void SerialPrintln(const char* message) override;
    void SerialPrint(const char* message) override;
    bool initFilesystem() override;
    unsigned long getTime() override;
    uint32_t getFreeHeap() override;
    bool WiFiIsConnected() override;
};

#endif // __HAL_ESP32_H__