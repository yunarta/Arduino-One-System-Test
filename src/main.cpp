#include <Arduino.h>
#include <ESP32QoL.h>
#include <IdentityShadowThing.h>
#include <LittleFS.h>
#include <QualityOfLife.h>
#include <Update.h>

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
            // vTaskDelay(pdMS_TO_TICKS(1000));
            shadowThing->loop();
            switch (shadowThing->getConnectionState()) {
                case CONNECTED:
                    // ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;
                case CONNECTING:
                    vTaskDelay(pdMS_TO_TICKS(10000));
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
                    "thingLifecycle", 10 * 1024, this, 1,
                    &thingLifecycleHandle);
    }
};

IdentityShadowThingTask *thingLifecycleTask;
Preferences otaPreferences;

bool eventCallback(const String &event) {
    if (event.equals(IDENTITY_THING_EVENT_JOBS)) {
        JsonDocument jobs = shadowThing->getPendingJobs();
        if (jobs.size() > 0) {
            Serial.println("Pending jobs found.");
            for (const JsonObject &job: jobs["queuedJobs"].as<JsonArray>()) {
                String jobId = job["jobId"];
                Serial.printf("Requesting job detail for jobId: %s\n", jobId.c_str());
                shadowThing->requestJobDetail(jobId);
            }
            return true;
        }
    }

    return false;
}

/**
 * {
 *     "version": "1.0",
 *     "command": {
 *         "name": "updateThreshold",
 *         "params": {
 *             "temperature": 28,
 *             "humidity": 70
 *         }
 *     },
 *     "expect": {
 *         "status": { "s": "OK" },
 *         "statusCode": { "s": "200" }
 *     }
 * }
 */
bool commandCallback(const String &executionId, const JsonDocument &payload) {
    auto name = payload["command"]["name"].as<String>();

    CommandReply reply{
        .status = "SUCCEEDED",
        .statusCode = "200",
        .statusReason = "OK",
        .result = payload["expect"]
    };
    shadowThing->commandReply(executionId, reply);
    return true;
}

bool jobCallback(const String &jobId, JsonDocument &payload) {
    Serial.println("Job processing started.");
    Serial.printf("Processing job ID: %s\n", jobId.c_str());
    //
    // auto execution = payload["execution"].as<JsonObject>();
    // auto document = execution["jobDocument"].as<JsonObject>();
    //
    // String jobType = document["type"];
    //
    // Preferences otaPreferences;
    //
    // if (jobType.equals("UpdateFirmware") && otaPreferences.begin("OTAUpdate", false)) {
    //     String firmwareUrl = document["job"]["params"]["url"];
    //     String firmwareVersion = document["job"]["params"]["version"];
    //
    //     String installedVersion = otaPreferences.getString("appVersion", "");
    //     if (installedVersion.equals(firmwareVersion)) {
    //         long expectedVersion = otaPreferences.getLong("expectedVersion", -1);
    //         if (expectedVersion != -1) {
    //             String expect = otaPreferences.getString("expect", "{}");
    //
    //             JsonDocument expectJsonDoc;
    //             deserializeJson(expectJsonDoc, expect);
    //
    //             JobReply reply{
    //                 .status = "SUCCEEDED",
    //                 .expectedVersion = expectedVersion,
    //                 .statusDetails = expectJsonDoc,
    //             };
    //
    //             Serial.printf("Replying to job ID: %s with status: %s\n", jobId.c_str(), reply.status.c_str());
    //             shadowThing->jobReply(jobId, reply);
    //
    //             otaPreferences.remove("expect");
    //         }
    //     } else {
    //         JsonObject expect = document["expect"];
    //
    //         String expectJson;
    //         serializeJson(expect, expectJson);
    //
    //         otaPreferences.putString("executionId", jobId);
    //         otaPreferences.putLong("expectedVersion", execution["versionNumber"].as<long>());
    //         otaPreferences.putString("expect", expectJson);
    //         otaPreferences.end();
    //
    //         OTAUpdate.begin(firmwareVersion, firmwareUrl);
    //     }
    //
    //     otaPreferences.end();
    //     // Serial.println("Job processing completed successfully.");
    //     return true;
    // }

    Serial.println("Job type not recognized. Skipping.");
    return false;
}


void setup() {
    Serial.begin(115200);

    LittleFS.begin(true);
    Config.begin();


    if (!connectToInternet()) {
        OTAUpdate.markAsInvalid();
        ESP.restart();
    }

    // esp_ota_mark_app_valid_cancel_rollback();

    shadowThing = new IdentityShadowThing(Config.doc["awsIoT"]["endPoint"].as<const char *>(),
                                          Config.doc["awsIoT"]["provisioning"].as<const char *>());
    shadowThing->setEventCallback(eventCallback);
    shadowThing->setCommandCallback(commandCallback);
    shadowThing->setJobCallback(jobCallback);

    thingLifecycleTask = new IdentityShadowThingTask(shadowThing);
    thingLifecycleTask->begin();

    OTAUpdate.markAsValid();
}

bool loopStarted = false;

void loop() {
    if (!loopStarted) {
        loopStarted = true;
        Serial.println("[INFO] Loop started.");
    }
    delay(1000);
}
