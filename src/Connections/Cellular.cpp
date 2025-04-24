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
    pinMode(CELLULAR_POWER_PIN, OUTPUT);
    pinMode(CELLULAR_POWER_PIN_AUX, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN_AUX, HIGH);
}

TinyGsm &Cellular::getModem()
{
    return modem;
}

Client *Cellular::getClient()
{
    return new TinyGsmClient(modem, 0);
}

// Active NTP sync via AT+CNTP
bool Cellular::syncTimeViaCNTP(float tz)
{
    // Enable network time update if supported
    modem.sendAT("+CLTS=1");
    modem.waitResponse();
    // Configure the NTP server and timezone offset (using pool.ntp.org and UTC offset 0)
    char cNTPCommand[64];
    snprintf(cNTPCommand, sizeof(cNTPCommand), "+CNTP=\"pool.ntp.org\",%d", (int)tz * 4);
    modem.sendAT(cNTPCommand);

    if (modem.waitResponse() != 1)
    {
        Log.warningln("Failed to set NTP server via CNTP.");
        return false;
    }

    // Trigger NTP synchronization
    modem.sendAT("+CNTP");
    if (modem.waitResponse(10000L, "+CNTP:") != 1)
    {
        Log.warningln("NTP synchronization via CNTP failed.");
        return false;
    }

    // Give the modem time to update its internal clock
    delay(3000);

    // Optionally, read the updated time for debugging
    String updatedTime = modem.getGSMDateTime(DATE_TIME);
    if (updatedTime.length() == 0)
    {
        Log.warningln("Failed to retrieve updated time via AT+CCLK.");
        return false;
    }
    Log.notice(F("Updated time from modem: %s" CR), updatedTime.c_str());
    return true;
}

// Combined function to sync time and then retrieve it
bool Cellular::getTime(struct tm &timeinfo, float &timezone)
{
    if (!isConnected())
    {
        Log.warningln("Not connected to cellular network.");
        return false;
    }

    // First, attempt to actively sync time via AT+CNTP.
    // (Even if this fails, we try to get time from the modem's built-in methods.)
    bool syncOk = syncTimeViaCNTP(timezone);
    if (!syncOk)
    {
        Log.warningln("Active CNTP sync failed; falling back to modem network time.");
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

    // Try using the modem's getNetworkTime() first.
    if (!modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &timezone))
    {
        Log.notice(F("getNetworkTime() failed. Attempting fallback using GSM DateTime." CR));
        // Fallback: get the GSM date/time string (expected format: "yy/mm/dd,hh:mm:ss+tz")
        String networkTime = modem.getGSMDateTime(DATE_TIME);
        if (networkTime.length() == 0)
        {
            Log.warningln("Failed to get network time: GSM DateTime string is empty.");
            return false;
        }
        // Parse the string; note that %f captures a fractional timezone (after skipping the sign with %*c)
        int ret = sscanf(networkTime.c_str(), "%d/%d/%d,%d:%d:%d%*c%f",
                         &year, &month, &day, &hour, &minute, &second, &timezone);
        if (ret < 7)
        {
            Log.warning("Failed to parse GSM date/time string: ");
            Log.warningln(F("%s"), networkTime);
            return false;
        }
    }

    // Convert a two-digit year to a full year if needed
    if (year < 100)
    {
        year += (year < 70) ? 2000 : 1900;
    }

    // Dynamically adjust the year if it is far ahead relative to our build year.
    int buildYear = getBuildYear();
    // If the reported year exceeds the build year by more than 2 years, assume it needs correction.
    if (year > buildYear + 2)
    {
        int offset = year - buildYear;
        Log.warning("Reported year (");
        Log.warning(F("%s"), String(year));
        Log.warning(") is far ahead of build year (");
        Log.warning(F("%s"), String(buildYear));
        Log.warningln("), applying correction.");
        year -= offset;
    }

    // Validate the parsed date/time components
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
    {
        Log.warningln("Invalid date/time received.");
        return false;
    }

    // Populate the tm structure
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;

    Log.notice(F("Time updated: %04d-%02d-%02d %02d:%02d:%02d, timezone: %.2f" CR),
               year, month, day, hour, minute, second, timezone);
    return true;
}
// Helper to extract the build year from __DATE__ (format "Mmm dd yyyy")
int Cellular::getBuildYear()
{
    char yearStr[5] = {0};
    // __DATE__ format: "Mmm dd yyyy" e.g., "Mar 27 2025"
    strncpy(yearStr, __DATE__ + 7, 4);
    return atoi(yearStr);
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
    // pinMode(CELLULAR_POWER_PIN, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN, HIGH);
    // this is the action for the SIM7600, other sims may have different power requirements
    // pinMode(CELLULAR_POWER_PIN_AUX, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN_AUX, HIGH);
    delay(500);
    digitalWrite(CELLULAR_POWER_PIN_AUX, LOW);
    /**
     * @brief You can assign an interrupt
     * on this pin indicate when the cellular module
     * I ready to communicate
     */
    // pinMode(CELLULAR_IND_PIN, INPUT);
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

bool Cellular::powerSave(bool on)
{
    return setFunctionality((int)on);
}

bool Cellular::setFunctionality(int func)
{
    String cmd = String("+CFUN=") + String(func);
    Log.noticeln("Configuring Functionality context: %s", cmd.c_str());
    modem.sendAT(cmd.c_str());
    if (modem.waitResponse(5000L, "OK") != 1)
    {
        Log.errorln("Failed to function mode");
        return false;
    }
    return true;
}

// Turn on the modem
bool Cellular::on()
{
    setupPower();
    // delay(10000);
    SerialAT.begin(UART_BAUD, SERIAL_8N1, CELLULAR_PIN_RX, CELLULAR_PIN_TX);
    initModem();
    delay(1000L);
    return connected;
}

void Cellular::terminateThreads()
{
#ifndef HYPHEN_THREADED
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
#endif
}

// Turn off the modem
bool Cellular::off()
{
    modem.poweroff();
    connected = false;
    terminateThreads();
    return true;
}

bool Cellular::reload()
{
    off();
    delay(1000);
    return on();
}

// Initialize the modem
void Cellular::initModem()
{
    unsigned long startTime = millis();
    bool modemReady = false;
    while (millis() - startTime < 60000)
    { // Wait up to 60 seconds
        if (modem.testAT())
        { // Check if modem responds
            modemReady = true;
            break;
        }
        delay(1000);
    }

    if (!modemReady)
    {
        return;
    }

    if (!modem.init())
    {
        Log.errorln("Failed to initialize modem.");
        return;
    }
    if (modem.getSimStatus() != SimStatus::SIM_READY && simPin)
    {
        modem.simUnlock(simPin.c_str());
    }
    // setFunctionality(4);
    connected = setupNetwork();
}

bool Cellular::setSimPin(const char *sPin)
{
    simPin = String(sPin);
    return reload();
}
bool Cellular::setApn(const char *setApn)
{
    apn = String(setApn);
    return reload();
}

void Cellular::maintain()
{
    if (!connected)
    {
        return;
    }
    connected = isConnected();
}

// Connect to the network
bool Cellular::connect()
{
    terminateThreads();
    if (!modem.waitForNetwork(20000L))
    {
        Log.errorln("Network connection failed.");
        return false;
    }

    connected = modem.gprsConnect(apn.c_str(), gprsUser, gprsPass);
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
    return modem.isNetworkConnected();
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
#ifndef HYPHEN_THREADED
    // Create a FreeRTOS task for keep-alive functionality
    keepAliveInterval = seconds; // Set the interval delay
    if (keepAliveHandle == NULL)
    {
        xTaskCreatePinnedToCore(keepAliveTask, "KeepAliveTask", 4096, this, 1, &keepAliveHandle, 1);
    }
#endif
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
    /**
 *
 * 2 – Automatic
13 – GSM Only
14 – WCDMA Only
38 – LTE Only
59 – TDS-CDMA Only
9 – CDMA Only
10 – EVDO Only
19 – GSM+WCDMA Only
22 – CDMA+EVDO Only
48 – Any but LTE
60 – GSM+TDSCDMA Only
63 – GSM+WCDMA+TDSCDMA Only
67 – CDMA+EVDO+GSM+WCDMA+TDSCDMA Only
39 – GSM+WCDMA+LTE Only
51 – GSM+LTE Only
54 – WCDMA+LTE Only
 *
 */
    return modem.setNetworkMode(NETWORK_MODE); // Automatic mode (GSM and LTE)
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