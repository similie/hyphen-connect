/* HyphenRunner.h */
#ifndef HYPHEN_RUNNER_H
#define HYPHEN_RUNNER_H
#include "Ticker.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
  bool initManager();
  volatile bool connectionStarted = false;
  volatile bool runConnection = false;
  static void taskEntry(void *pv);
  void runTask();
  TaskHandle_t loopHandle = nullptr;
  HyphenConnect *hyphen = nullptr;
  void startRunnerTask();
  int allocatedStack = 8192;
  void rebuildConnection();
};

#endif
