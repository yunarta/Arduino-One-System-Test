#include <Arduino.h>
#include "StepFunction.h"
#include <IdentityShadowThing.h>
#include <LittleFS.h>
#include <QualityOfLife.h>

IdentityShadowThing *shadowThing;


class IdentityShadowThingTask {
    static void taskEntryPoint(void *p) {
        auto *task = static_cast<IdentityShadowThingTask *>(p);
        task->task();
    }

    void task() {
        shadowThing->setSignalCallback([this] {
            xTaskNotifyGive(thingLifecycleHandle);
        });
        shadowThing->begin();
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            shadowThing->loop();
            switch (shadowThing->getConnectionState()) {
                case CONNECTED:
                    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                    break;
                case CONNECTING:
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    break;
                case TIMEOUT:
                    esp_deep_sleep_start();
            }
        }
    }

public:
    IdentityShadowThing *shadowThing;

    TaskHandle_t thingLifecycleHandle = nullptr;

    IdentityShadowThingTask(IdentityShadowThing *shadowThing) {
        this->shadowThing = shadowThing;
    }

    void begin() {
        xTaskCreate(taskEntryPoint,
                    "thingLifecycle", 8192, this, 1,
                    &thingLifecycleHandle);
    }
};

IdentityShadowThingTask *thingLifecycleTask;

void setup() {
    Serial.begin(115200);

    LittleFS.begin(true);
    Config.begin();

    connectToInternet();

    shadowThing = new IdentityShadowThing(Config.doc["awsIoT"]["endPoint"].as<const char *>(),
                                          Config.doc["awsIoT"]["provisioning"].as<const char *>());
    thingLifecycleTask = new IdentityShadowThingTask(shadowThing);
    thingLifecycleTask->begin();
}

void loop() {
    delay(1000);
}
