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

GPSData Cellular::getGPSData()
{
    enableGPS();
    delay(300);
    GPSData data = {0, 0, 0, 0};

    unsigned long startTime = millis();
    const unsigned long timeout = 10000;
    // tick.attach_ms(200, []()
    //                { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); });

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

// FreeRTOS task function for maintain
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

    if (modem.getSimStatus() != 3 && GSM_SIM_PIN)
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
    Log.noticeln("Maintaining connection ");

    if (connection && connected)
    {
        return modem.maintain();
    }
    connected = false;
    Log.noticeln("Reconnecting...");
    modem.init();
    connect();
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
    Log.noticeln("Network connected");
    connected = modem.gprsConnect(apn, gprsUser, gprsPass);
    if (connected)
    {
        Log.noticeln("GPRS connected");
        // modem.sendAT("+CDNSCFG=\"8.8.8.8\"");
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

int Cellular::getSignalQuality()
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