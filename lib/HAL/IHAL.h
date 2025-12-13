#ifndef __IHAL_H__
#define __IHAL_H__

#include <stdint.h>

class IHAL 
{
public:
    virtual ~IHAL() = default;

    virtual void SerialPrintf(const char* format, ...) = 0;
    virtual void SerialPrintln(const char* message) = 0;
    virtual void SerialPrint(const char* message) = 0;
    virtual bool initFilesystem() = 0;
    virtual unsigned long getTime() = 0;
    virtual uint32_t getFreeHeap() = 0;
    virtual bool WiFiIsConnected() = 0;
};

#endif // __IHAL_H__