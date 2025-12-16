#ifndef __HAL_ESP32_H__
#define __HAL_ESP32_H__

#include "IHAL.h"
#include <stdint.h>
class HAL_ESP32 : public IHAL
{
public:
    void SerialPrintf(const char* format, ...) override;
    void SerialPrintln(const char* message) override;
    void SerialPrint(const char* message) override;
    bool begin() override;
    unsigned long getTime() override;
    uint32_t getFreeHeap() override;
    bool WiFiIsConnected() override;
    
    bool initFilesystem();
};

#endif // __HAL_ESP32_H__