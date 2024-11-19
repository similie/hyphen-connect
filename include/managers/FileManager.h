#ifndef __FILE_MANAGER_H_
#define __FILE_MANAGER_H_
#include <Arduino.h>
#include <ArduinoLog.h>
#include "FS.h"     // File system for SPIFFS
#include "SPIFFS.h" // ESP32 SPIFFS

class FileManager
{
private:
    bool isRunning = false;

public:
    FileManager();
    String loadFileContents(const char *name);
    String fileContentsOpen(const char *name);
    bool start();
    void end();
    bool running();
};

#endif // __FILE_MANAGER_H_