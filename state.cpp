#include "state.h"

SystemState      gState;
SemaphoreHandle_t gStateMutex = nullptr;

void stateInit() {
  gStateMutex = xSemaphoreCreateMutex();
  configASSERT(gStateMutex);
}
