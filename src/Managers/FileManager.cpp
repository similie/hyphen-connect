#include "managers/FileManager.h"

FileManager::FileManager()
{
}

String FileManager::loadFileContents(const char *name)
{
    File file = SPIFFS.open(name, "r");
    if (!file)
    {
        Log.errorln("Failed to open file for reading");
        return "";
    }

    String contents = file.readString();
    file.close();
    return contents;
}

String FileManager::fileContentsOpen(const char *name)
{
    start();
    String contents = loadFileContents(name);
    end();
    return contents;
}

bool FileManager::running()
{
    return isRunning;
}

bool FileManager::start()
{
    if (isRunning)
    {
        return true;
    }

    if (!SPIFFS.begin(true))
    {
        Log.errorln("Failed to initialize SPIFFS");
        return false;
    }
    isRunning = true;
    return true;
}

void FileManager::end()
{
    if (!isRunning)
    {
        return;
    }
    isRunning = false;
    SPIFFS.end();
}