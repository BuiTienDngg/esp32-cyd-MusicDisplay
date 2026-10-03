#include <Arduino.h>
#include <esp_sleep.h>
#include <atomic>
#include <freertos/semphr.h>
#include "bluetooth.h"
#include "apple_media_service.h"
#include "display.h"
#include "buttons.h"
#include "leds.h"
#include "logger.h"
#include "cyd_board.h"

constexpr uint32_t sleepTimeout = 3 * 60 * 1000;
constexpr uint32_t displayTimeout = 30 * 1000;
ButtonManager buttonManager;
DisplayManager displayManager;
LEDManager ledManager;
std::atomic<uint32_t> startTime{0};
std::atomic<bool> disableDisplay{false}, isConnected{false}, sleepRequested{false};
SemaphoreHandle_t mediaMutex = nullptr;
AppleMediaService::MediaInformation media_info{};

AppleMediaService::MediaInformation mediaSnapshot() {
    xSemaphoreTake(mediaMutex, portMAX_DELAY);
    auto snapshot = media_info;
    xSemaphoreGive(mediaMutex);
    return snapshot;
}

void onDataUpdateCallback() {
    xSemaphoreTake(mediaMutex, portMAX_DELAY);
    media_info = AppleMediaService::GetMediaInformation();
    xSemaphoreGive(mediaMutex);
}

void handleLedsButtons(void*) {
    for (;;) {
        if (sleepRequested.load()) {
            ledManager.disable();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        buttonManager.handle();
        if (!isConnected.load()) buttonManager.clearCommands();
        if (buttonManager.isAnyClicked()) {
            startTime.store(millis());
            if (disableDisplay.exchange(false)) buttonManager.clearCommands();
        }
        if (disableDisplay.load()) ledManager.disable();
        else {
            xSemaphoreTake(mediaMutex, portMAX_DELAY);
            const auto playback = media_info.mPlaybackState;
            xSemaphoreGive(mediaMutex);
            ledManager.updateLEDs(playback, isConnected.load());
        }
        // Yield so the input/LED loop does not starve BLE or the watchdog.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void handleBLEDisplay(void*) {
    bool screenOff = false;
    for (;;) {
        Bluetooth::Service();
        const bool connected = Bluetooth::IsConnected();
        isConnected.store(connected);
        const auto media = mediaSnapshot();
        const uint32_t idle = millis() - startTime.load();
        if (idle >= displayTimeout) disableDisplay.store(true);

        // Snapshot feedback before a BLE command can block waiting for its ACK.
        const int highlighted = buttonManager.highlightedControl();
        uint8_t command;
        while (buttonManager.nextCommand(command)) {
            if (connected && !disableDisplay.load()) AppleMediaService::setRemoteCommandValue(command);
        }
        if (disableDisplay.load()) {
            if (!screenOff) { displayManager.disable(); screenOff = true; }
        } else {
            displayManager.updateDisplay(media, connected,
                highlighted >= 0 ? highlighted : buttonManager.highlightedControl());
            screenOff = false;
        }

        if (idle >= sleepTimeout && (!connected || media.mPlaybackState != AppleMediaService::MediaInformation::PlaybackState::Playing)) {
            sleepRequested.store(true);
            // Allow the input task to stop updating RGB before turning it off.
            vTaskDelay(pdMS_TO_TICKS(30));
            ledManager.disable();
            if (!screenOff) displayManager.disable();
            LOG_TRACE("CYD deep sleep; press BOOT to wake");
            Serial.flush();
            btStop();
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
            // GPIO35 is an unconnected input on CYD, not a second wake button.
            esp_deep_sleep_start();
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void setup() {
    Serial.begin(115200);
    LOG_INFO("CTRL 01: ESP32-CYD ILI9341 320x240");
    mediaMutex = xSemaphoreCreateMutex();
    configASSERT(mediaMutex);
    ledManager.begin();
    buttonManager.begin();
    displayManager.begin();
    displayManager.updateDisplay(media_info, false);
    AppleMediaService::RegisterForNotifications(onDataUpdateCallback, AppleMediaService::NotificationLevel::All);
    Bluetooth::Begin("CTRL 01");
    startTime.store(millis());
    const BaseType_t inputTask = xTaskCreatePinnedToCore(handleLedsButtons, "CYD input", 4096, nullptr, 1, nullptr, 0);
    const BaseType_t bleTask = xTaskCreatePinnedToCore(handleBLEDisplay, "BLE display", 8192, nullptr, 1, nullptr, 1);
    configASSERT(inputTask == pdPASS && bleTask == pdPASS);
}

void loop() {
    delay(1000);
}
