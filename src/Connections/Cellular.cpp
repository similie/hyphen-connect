#include "connections/Cellular.h"

Cellular::Cellular()
#ifdef DUMP_AT_COMMANDS
    : debugger(SerialAT, SerialMon), modem(debugger), gsmClient(modem, 0), secondaryGsmClient(modem, 1)
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

Client &Cellular::getClient()
{
    return gsmClient;
}
SecureClient &Cellular::secureClient()
{
    sslClient.setClient(&getClient());
    return sslClient;
}

Client &Cellular::getNewClient()
{
    return secondaryGsmClient;
}

SecureClient &Cellular::getNewSecureClient()
{
    secondarySslClient.setClient(&getNewClient());
    return secondarySslClient;
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
    coreDelay(3000);

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
    if (!isConnected())
    {
        Log.errorln("Not connected to cellular network.");
        return {0, 0, 0, 0};
    }

    if (!enableGPS())
    {
        Log.errorln("Failed to enable GPS.");
        return {0, 0, 0, 0};
    }
    coreDelay(300);
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

        coreDelay(300); // Wait a second before retrying
    }

    Log.notice(F("RAW GPS %s" CR), modem.getGPSraw().c_str());
    disableGPS();
    return data;
}

// Power setup for the modem
void Cellular::setupPower()
{

    coreDelay(10);
    // pinMode(CELLULAR_POWER_PIN, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN, HIGH);
    // this is the action for the SIM7600, other sims may have different power requirements
    // pinMode(CELLULAR_POWER_PIN_AUX, OUTPUT);
    digitalWrite(CELLULAR_POWER_PIN_AUX, HIGH);
    coreDelay(500);
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
        cellular->maintainLocal();                                     // Call maintain function to check and reconnect
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
    SerialAT.begin(UART_BAUD, SERIAL_8N1, CELLULAR_PIN_RX, CELLULAR_PIN_TX);
    powerOn = true;
    return initModem();
}
// Turn off the modem
bool Cellular::off()
{
    modem.poweroff();
    connected = false;
    digitalWrite(CELLULAR_POWER_PIN, LOW);
    powerOn = false;
    return true;
}

bool Cellular::reload()
{
    disconnect();
    off();
    coreDelay(1000);
    return init();
}

// Call this after your modem object is initialized but before you try to use it
/**
 * @brief Factory reset the modem to default settings
 * @note this function is not having an impact on stuck sims - it has so far been somewhat inert
 */
bool Cellular::factoryReset()
{ // just get out of it until we can test it better
    if (true)
    {
        return true;
    }
    modem.sendAT("+COPS=2"); // deregister from network
    modem.waitResponse(10000L);
    // 3. Reset network selection and mode to auto
    modem.sendAT("+COPS=0"); // auto network selection
    modem.waitResponse();
    // modem.sendAT("+CNMP=2"); // auto mode (2G/3G/LTE)
    // modem.waitResponse();
    // (AT+CNBP to reset band mask is usually not needed if using AT&F)
    // 4. Load factory defaults and save to NVRAM
    modem.sendAT("&F"); // factory default profile [oai_citation_attribution:14‡manualslib.com](https://www.manualslib.com/manual/1889278/Simcom-Sim7500-Series.html#:~:text=,value)
    modem.waitResponse();
    modem.sendAT("&W"); // save profile to NVRAM [oai_citation_attribution:15‡manualslib.com](https://www.manualslib.com/manual/1889278/Simcom-Sim7500-Series.html#:~:text=%2A%20,The%20User%20Setting%20To%20Me)
    modem.waitResponse();

    // 5. Reboot the modem to apply changes
    modem.sendAT("+CFUN=6");         // reboot module [oai_citation_attribution:16‡docs.monogoto.io](https://docs.monogoto.io/getting-started/general-device-configurations/iot-devices/simcom-sim7600g-h#:~:text=Reset%20the%20modem%20to%20its,default%20configuration)
    modem.waitResponse(5000L, "OK"); // (No waitResponse here because modem will reset immediately)
    modem.sendAT("+CFUN=1,1");
    modem.waitResponse(5000L, "OK");

    coreDelay(2000);

    return true;
}

void Cellular::setSimRegistration()
{
    modem.sendAT("+CREG=2"); // get unsolicited network-registration events
    modem.waitResponse();
    modem.sendAT("+CGREG=2"); // same for GPRS registration
    modem.waitResponse();
    // now explicitly ask the SIM to PS-attach
    modem.sendAT("+CGATT=1"); // Packet-domain attach
    modem.waitResponse(5000); // give it up to 5 s
}

// Initialize the modem
bool Cellular::initModem()
{
    connectionAttempts++;
    Log.noticeln("Connection attempt: %d", connectionAttempts);
    unsigned long startTime = millis();
    bool modemReady = false;
    while (millis() - startTime < 60000)
    { // Wait up to 60 seconds
        if (!powerOn)
        {
            break;
        }
        Log.noticeln("Waiting for modem to be ready...");
        if (modem.testAT(1000U))
        { // Check if modem responds
            Log.noticeln("Modem is ready.");
            modemReady = true;
            break;
        }
        coreDelay(1000);
    }

    if (!modemReady)
    {

        return false;
    }

    if (modemReady && connectionAttempts >= maxConnectionAttempts && factoryReset())
    {
        connectionAttempts = 0;
        Log.noticeln("RESTORING MODEM FACTORY DEFAULT");
    }
    coreDelay(500);
    if (!modem.init())
    {
        Log.errorln("Failed to initialize modem.");
        return false;
    }

    if (modem.getSimStatus() != SimStatus::SIM_READY && simPin)
    {
        modem.simUnlock(simPin.c_str());
    }

    return setupNetwork();
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

bool Cellular::maintainLocal()
{
    if (!modem.testAT())
    {
        return false;
    }

    if (!setupNetwork())
    {
        return false;
    }
    return connect();
}

bool Cellular::internetPathTest()
{
    // const char *testHost = "tls-v1-2.badssl.com"; // reliable public endpoint
    const char *testHost = CELLULAR_TEST_URL;
    // const uint16_t testPort = 1012;               // TLS port for this host
    const uint16_t testPort = CELLULAR_TEST_PORT; // TLS port for this host
    const unsigned long timeoutMs = 10000UL;      // 10 second timeout

    // Use a “new” client to avoid interfering with main client
    Client &rawClient = getNewClient();
    // SecureClient &sslClient = getNewSecureClient();
    // sslClient.setClient(&rawClient);

    rawClient.setTimeout(timeoutMs / 1000);

    Log.noticeln("Internet path test: connecting to %s:%u", testHost, testPort);
    if (!rawClient.connect(testHost, testPort))
    {
        Log.errorln("Internet path test: connect to %s:%u failed", testHost, testPort);
        return false;
    }

    // Optionally send a minimal HTTP HEAD request — this ensures data path is working
    // Minimal HTTP HEAD request
    String req = String("HEAD / HTTP/1.1\r\nHost: ") + testHost +
                 "\r\nConnection: close\r\n\r\n";
    rawClient.print(req);

    unsigned long startMs = millis();
    while (millis() - startMs < timeoutMs)
    {
        if (rawClient.available())
        {
            String line = rawClient.readStringUntil('\n');
            Log.noticeln("Internet path test: got response: %s", line.c_str());
            rawClient.stop();
            return true;
        }
        delay(10);
    }

    Log.warningln("Internet path test: no response within %lu ms", timeoutMs);
    rawClient.stop();
    return false;
}

bool Cellular::maintain()
{
#ifdef NO_CELLULAR_TEST_INTERVAL_MAINTAIN
    return isConnected();
#else

    Log.noticeln("Maintaining cellular connection...");
    unsigned long now = millis();
    if (lastTestMs != 0 && now - lastTestMs < CELLULAR_TEST_INTERVAL_MS)
    {
        return isConnected(); // Skip test if interval not reached
    }

    lastTestMs = now;

    if (!internetPathTest())
    {
        return false;
    }
    return true;

#endif
}
void Cellular::setClient()
{
    gsmClient.init(&modem, 0);
    secondaryGsmClient.init(&modem, 1);
}

void Cellular::restore()
{
    reload();
}
// Connect to the network
bool Cellular::connect()
{
    if (!modem.waitForNetwork(20000L))
    {
        Log.errorln("Network connection failed.");
        return false;
    }

    if (!modem.gprsConnect(apn.c_str(), gprsUser, gprsPass))
    {
        return false;
    }

    uint8_t count = 0;
    const uint8_t maxRetries = 10;
    connected = false;
    while (!connected && count < maxRetries)
    {
        connected = modem.isNetworkConnected();
        if (count > 0 && !connected)
        {
            Log.errorln("Network connection failed. Retrying...");
        }
        coreDelay(1000);
        count++;
    }

    if (connected)
    {
        Log.noticeln("Network connected. %s", String(connected ? "true" : "false").c_str());
        setClient();
        connectionAttempts = 0;
    }

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
bool Cellular::enableGPS()
{
    return modem.enableGPS();
}

// Disable GPS functionality
bool Cellular::disableGPS()
{
    // modem.sendAT("+CGNSPWR=0");
    return modem.disableGPS();
}

// Set up the network mode for LTE/GSM
bool Cellular::setupNetwork()
{
    /**
 * SIMCOM 7600 Series Network Modes:
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

String Cellular::getModemInfo()
{
    return modem.getModemInfo();
}

String Cellular::getIMSI()
{
    return modem.getIMSI();
}

String Cellular::getLocalIP()
{
    return modem.getLocalIP();
}

String Cellular::getIMEI()
{
    return modem.getIMEI();
}

String Cellular::getOperator()
{
    return modem.getOperator();
}

int16_t Cellular::getNetworkMode()
{
    return modem.getNetworkMode();
}

String Cellular::getSimCCID()
{
    return modem.getSimCCID();
}

String Cellular::getProvider()
{
    return modem.getProvider();
}

float Cellular::getTemperature()
{
    return modem.getTemperature();
}

int16_t Cellular::getSignalQuality()
{
    return modem.getSignalQuality();
}
