#include "connections/Cellular.h"

Cellular::Cellular()
#ifdef DUMP_AT_COMMANDS
    : debugger(SerialAT, SerialMon), modem(debugger), gsmClient(modem, 0)
#else
    : modem(SerialAT), gsmClient(modem, 0)
#endif
{
    activeSim = SimType::SIM7600;
    connected = false;
}

TinyGsm &Cellular::getModem()
{
    return modem;
}

Client *Cellular::getClient()
{
    return new TinyGsmClient(modem, 0);
}

bool Cellular::getTime(struct tm &timeinfo, float &timezone)
{
    if (!isConnected())
    {
        return false;
    }

    int year, month, day, hour, minute, second;

    if (!modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &timezone))
    {
        String networkTime = modem.getGSMDateTime(DATE_TIME);
        if (networkTime == nullptr || networkTime.length() == 0)
        {
            Log.warningln("Failed to get network time");
            return false;
        }
        sscanf(networkTime.c_str(), "%d/%d/%d,%d:%d:%d%*c%d", &year, &month, &day, &hour, &minute, &second, &timezone);
    }
    year += (year < 100) ? ((year < 70) ? 2000 : 1900) : 0;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;
    return true;
}
/**
 * @brief Get GPS data from modem
 * @todo This function is not yet implemented as my test unit does not seem to have a
 * working GPS module.
 * @return GPSData
 */
GPSData Cellular::getGPSData()
{
    enableGPS();
    delay(300);
    GPSData data = {0, 0, 0, 0};

    unsigned long startTime = millis();
    const unsigned long timeout = 10000;

    while (millis() - startTime < timeout)
    {

        if (modem.getGPS(&data.lat, &data.lon, &data.speed, &data.alt))
        {
            // Check if we received a non-zero position
            if (data.lat != 0.0 && data.lon != 0.0)
            {
                break;
            }
        }

        delay(1000); // Wait a second before retrying
    }

    Log.notice(F("RAW GPS %s" CR), modem.getGPSraw().c_str());

    modem.sendAT("+CGNSINF"); // Query GPS information

    if (modem.waitResponse(10000L))
    {
        String gpsData = modem.stream.readStringUntil('\n');
        Log.error(F("GPS data not available %s" CR), gpsData.c_str());
    }
    // tick.detach();
    digitalWrite(LED_PIN, LOW);
    disableGPS();
    return data;
}

// Power setup for the modem
void Cellular::setupPower()
{

    delay(10);
    pinMode(CELLULAR_POWER_PIN, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN, HIGH);
    // this is the action for the SIM7600, other sims may have different power requirements
    pinMode(CELLULAR_POWER_PIN_AUX, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN_AUX, HIGH);
    delay(500);
    digitalWrite(CELLULAR_POWER_PIN_AUX, LOW);
    /**
     * @brief You can assign an interrupt
     * on this pin indicate when the cellular module
     * I ready to communicate
     */
    pinMode(CELLULAR_IND_PIN, INPUT);
}

bool Cellular::init()
{
    if (!on())
    {
        Log.errorln("Failed network connection.");
        return false;
    }

    if (connect())
    {
        Log.noticeln("Connected to network.");
        return true; // keepAlive(30);
    }
    Log.errorln("Failed to connect.");

    return false;
}

// FreeRTOS task function for maintain. Not used in this implementation
void Cellular::maintainTask(void *param)
{
    Cellular *cellular = static_cast<Cellular *>(param);
    while (true)
    {
        cellular->maintain();                                          // Call maintain function to check and reconnect
        vTaskDelay(cellular->maintainIntervalMs / portTICK_PERIOD_MS); // Delay between checks
    }
}

// Turn on the modem
bool Cellular::on()
{
    setupPower();
    delay(10000);
    SerialAT.begin(UART_BAUD, SERIAL_8N1, CELLULAR_PIN_RX, CELLULAR_PIN_TX);
    initModem();
    return connected;
}

void Cellular::terminateThreads()
{
    if (keepAliveHandle != NULL)
    {
        Log.noticeln("Deleting keepAlive task...");
        vTaskDelete(keepAliveHandle);
        keepAliveHandle = NULL;
    }

    if (maintainHandle != NULL)
    {
        Log.noticeln("Deleting maintain task...");
        vTaskDelete(maintainHandle);
        maintainHandle = NULL;
    }

    delay(100); // Give time for task cleanup
}

// Turn off the modem
bool Cellular::off()
{
    modem.poweroff();
    connected = false;
    terminateThreads();
    return true;
}

// Initialize the modem
void Cellular::initModem()
{
    if (!modem.init())
    {
        Log.errorln("Failed to initialize modem.");
        return;
    }
    if (modem.getSimStatus() != SimStatus::SIM_READY && GSM_SIM_PIN)
    {
        modem.simUnlock(GSM_SIM_PIN);
    }

    connected = setupNetwork();
}

void Cellular::maintain()
{
    if (!connected)
    {
        return;
    }
    bool connection = isConnected();
    if (connection && connected)
    {
        return;
    }
    connected = false;
}

// Connect to the network
bool Cellular::connect()
{
    terminateThreads();
    if (!modem.waitForNetwork())
    {
        Log.errorln("Network connection failed.");
        return false;
    }

    connected = modem.gprsConnect(apn, gprsUser, gprsPass);
    return connected;
}

// Disconnect from the network
void Cellular::disconnect()
{
    modem.gprsDisconnect();
    connected = false;
}

// Clear GPRS credentials
bool Cellular::clearCredentials()
{
    modem.gprsDisconnect();
    return !modem.isGprsConnected();
}

// Check network connection status
bool Cellular::isConnected()
{
    return connected && modem.isNetworkConnected();
}

// Get the active SIM type
SimType Cellular::getActiveSim()
{
    return activeSim;
}

// Set the active SIM type
void Cellular::setActiveSim(SimType simType)
{
    activeSim = simType;
}

// Keep the cellular connection alive using FreeRTOS
bool Cellular::keepAlive(uint8_t seconds)
{
    // Create a FreeRTOS task for keep-alive functionality
    keepAliveInterval = seconds; // Set the interval delay
    if (keepAliveHandle == NULL)
    {
        xTaskCreatePinnedToCore(keepAliveTask, "KeepAliveTask", 4096, this, 1, &keepAliveHandle, 1);
    }
    return true;
}

// Enable GPS functionality
void Cellular::enableGPS()
{

    modem.sendAT("+CGNSPWR=1");
    if (modem.waitResponse(20000L) != 1)
    {
        Log.errorln("Failed to enable GNSS power");
    }
    modem.enableGPS();
}

// Disable GPS functionality
void Cellular::disableGPS()
{
    modem.sendAT("+CGNSPWR=0");
    modem.disableGPS();
}

// Set up the network mode for LTE/GSM
bool Cellular::setupNetwork()
{
    return modem.setNetworkMode(2); // Automatic mode (GSM and LTE)
}

inline String Cellular::getModemInfo()
{
    return modem.getModemInfo();
}

inline String Cellular::getIMSI()
{
    return modem.getIMSI();
}

inline String Cellular::getLocalIP()
{
    return modem.getLocalIP();
}

inline String Cellular::getIMEI()
{
    return modem.getIMEI();
}

inline String Cellular::getOperator()
{
    return modem.getOperator();
}

inline int16_t Cellular::getNetworkMode()
{
    return modem.getNetworkMode();
}

inline String Cellular::getSimCCID()
{
    return modem.getSimCCID();
}

inline String Cellular::getProvider()
{
    return modem.getProvider();
}

float Cellular::getTemperature()
{
    return modem.getTemperature();
}

inline int16_t Cellular::getSignalQuality()
{
    return modem.getSignalQuality();
}

// Static FreeRTOS task function for keep-alive
void Cellular::keepAliveTask(void *param)
{
    Cellular *cellular = static_cast<Cellular *>(param);
    uint32_t delayTicks = cellular->keepAliveInterval * 1000 / portTICK_PERIOD_MS;

    while (true)
    {
        if (cellular->isConnected())
        {
            cellular->modem.maintain();
            Log.noticeln("Keep-alive task: Connection maintained.");
        }
        vTaskDelay(delayTicks); // Delay according to the specified interval
    }
}