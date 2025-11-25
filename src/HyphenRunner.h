/* HyphenRunner.h */
#ifndef HYPHEN_RUNNER_H
#define HYPHEN_RUNNER_H
#include "Ticker.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

class HyphenConnect;

class HyphenRunner
{
public:
  static HyphenRunner &get();
  void begin(HyphenConnect *hc);
  void stop();
  void restart(HyphenConnect *hc);
  void loop();
  bool ready();
  bool sendRegistration = true;
  void noRegistration() { sendRegistration = false; };
  void withRegistration() { sendRegistration = true; };
  void breakConnector() { runConnection = false; };

private:
  HyphenRunner();
  void begin();
  void processorLoops();
  TaskHandle_t manangerInitHandle = nullptr;
  void startManagerInitTask();
  static void managerInitTask(void *pv);
  bool initManager();
  volatile bool connectionStarted = false;
  volatile bool runConnection = false;
  unsigned long threadCheck = 0;
  static void taskEntry(void *pv);
  static void maintenanceCallback(void *pv);
  void runTask();
  TaskHandle_t loopHandle = nullptr;
  EventGroupHandle_t eg = nullptr;
  HyphenConnect *hyphen = nullptr;
  void startRunnerTask();
  void startMaintenanceTask();
  int allocatedStack = 8192;
  const uint32_t LAST_ALIVE_THRESHOLD = 1000 * 60 * 5; // 10 minutes
  volatile bool initialized = false;
  void rebuildConnection();
};

#endif
