/* HyphenRunner.h */
#ifndef HYPHEN_RUNNER_H
#define HYPHEN_RUNNER_H

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
  bool isAlive() const;
  // bool isConnected() const;
  void stop();
  bool initRequested(); // you can set a flag to request init()
  void restart(HyphenConnect *hc);

private:
  HyphenRunner();
  static void taskEntry(void *pv);
  static void maintenanceCallback(void *pv);
  void runTask();
  TaskHandle_t loopHandle = nullptr;
  EventGroupHandle_t eg = nullptr;
  HyphenConnect *hyphen = nullptr;
  void startRunnerTask();
  void startMaintenanceTask();
  int allocatedStack = 8192;
  bool wantInit = false;
};

#endif
