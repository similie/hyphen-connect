#include "cellular.h"

Cellular::Cellular()
#ifdef DUMP_AT_COMMANDS
    : debugger(SerialAT, SerialMon), modem(debugger), gsmClient(modem, 0), sslClient(&gsmClient)
#else
    : modem(SerialAT), gsmClient(modem, 0), sslClient(&gsmClient)
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
    tick.attach_ms(200, []()
                   { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); });

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

    Serial.print("Raw GPS data: ");
    Serial.printf("RAW GPS %s\n", modem.getGPSraw().c_str());

    modem.sendAT("+CGNSINF"); // Query GPS information

    if (modem.waitResponse(10000L))
    {
        String gpsData = modem.stream.readStringUntil('\n');
        Serial.printf("GPS data not available %s\n", gpsData.c_str());
    }
    tick.detach();
    digitalWrite(LED_PIN, LOW);
    disableGPS();
    return data;
}

// Power setup for the modem
void Cellular::setupPower()
{

    delay(10);
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);

    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(500);
    digitalWrite(PWR_PIN, LOW);

    pinMode(IND_PIN, INPUT);
    // pinMode(LED_PIN, OUTPUT);
    // digitalWrite(LED_PIN, LOW);
}

bool Cellular::init()
{
    if (!on())
    {
        SerialMon.println("Failed network connection.");
        return false;
    }

    if (connect())
    {
        SerialMon.println("Connected to network.");
        return keepAlive(30);
    }
    SerialMon.println("Failed to connect.");
    return false;
}

// void Cellular::maintain()
// {
//     Serial.println("i MA MAINTAINING");
//     modem.maintain();
// }
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
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    initModem();
    return connected;
}

void Cellular::terminateThreads()
{
    if (keepAliveHandle != NULL)
    {
        Serial.println("Deleting keepAlive task...");
        vTaskDelete(keepAliveHandle);
        keepAliveHandle = NULL;
    }

    if (maintainHandle != NULL)
    {
        Serial.println("Deleting maintain task...");
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
        SerialMon.println("Failed to initialize modem.");
        return;
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
    Serial.print("Maintain ");
    Serial.println(connected);
    if (connection && connected)
    {
        return modem.maintain();
    }
    connected = false;
    Serial.println("Reconnecting...");
    modem.restart();
    connect();
}

// Connect to the network
bool Cellular::connect()
{
    terminateThreads();
    if (!modem.waitForNetwork())
    {
        SerialMon.println("Network connection failed.");
        return false;
    }
    SerialMon.println("Network connected");
    connected = modem.gprsConnect(apn, gprsUser, gprsPass);
    if (connected)
    {
        SerialMon.println("GPRS connected");
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
        SerialMon.println("Failed to enable GNSS power");
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
            SerialMon.println("Keep-alive task: Connection maintained.");
        }
        vTaskDelay(delayTicks); // Delay according to the specified interval
    }
}