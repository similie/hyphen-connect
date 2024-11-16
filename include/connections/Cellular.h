
#include <Ticker.h>
#include "connections/Connection.h"
#define SerialMon Serial
#define SerialAT Serial1
#include <TinyGsmClient.h>
// #define DUMP_AT_COMMANDS 0
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif

#ifndef cellular_h
#define cellular_h
enum class SimType
{
    SIM7070,
    SIM7600
};

typedef struct
{
    float lat;
    float lon;
    float speed;
    float alt;
} GPSData;

class Cellular : public Connection
{
public:
    Cellular();
    bool connect();
    void disconnect();
    bool clearCredentials();
    bool isConnected();
    SimType getActiveSim();
    void setActiveSim(SimType simType);
    bool on();
    bool off();
    bool keepAlive(uint8_t seconds);
    int getSignalQuality();
    bool init();
    void maintain();
    TinyGsm &getModem();
    Client *getClient();
    void enableGPS();
    void disableGPS();
    GPSData getGPSData();

private:
#ifdef DUMP_AT_COMMANDS
    StreamDebugger debugger;
#endif
    TinyGsmClient gsmClient;
    TinyGsm modem;
    Ticker tick;
    SimType activeSim;
    bool connected;
    const char *apn = CELLULAR_APN;
    const char *gprsUser = "";
    const char *gprsPass = "";
    uint8_t keepAliveInterval;              // Store the delay interval in seconds
    static void keepAliveTask(void *param); // FreeRTOS task for keeping alive
    TaskHandle_t keepAliveHandle = NULL;
    TaskHandle_t maintainHandle = NULL;    // Task handle for maintain task
    static void maintainTask(void *param); // Task function for maintain
    uint32_t maintainIntervalMs = 5000;    // Interval for maintain checks (5 seconds, adjustable)
    void initModem();
    void setupPower();
    bool setupNetwork();
    void terminateThreads();
};
#endif