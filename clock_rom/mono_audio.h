// SPDX-License-Identifier: Apache-2.0
// Register sequence reference Copyright 2015-2026 Espressif Systems (Shanghai) CO LTD.
// ES8311 playback only. Board wiring: seller schematic SHA256 840B5900...DB7A76.
// Register/clock sequence based on Espressif esp-bsp components/es8311/es8311.c
// (Espressif Systems, Apache-2.0); no microphone capture, no network audio.
#pragma once
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>

namespace monoAudio {
constexpr int sda = 40, scl = 39, mclk = 38, bclk = 16, ws = 7, dout = 4, pa = 8;
constexpr uint8_t codec = 0x18;
constexpr int sampleRate = 16000;
QueueHandle_t requests = nullptr;
volatile int state = 0; // 0=not initialized, 1=ready, -1=failed
volatile bool busy = false;
bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(codec); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}
bool initialize() {
  pinMode(pa, OUTPUT); digitalWrite(pa, LOW);
  if (!Wire.begin(sda, scl, 100000)) return false;
  Wire.setTimeOut(50);
  Wire.beginTransmission(codec); if (Wire.endTransmission() != 0) return false;
  i2s_config_t cfg{};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = sampleRate; cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1; cfg.dma_buf_count = 4; cfg.dma_buf_len = 256;
  cfg.use_apll = false; cfg.tx_desc_auto_clear = true; cfg.fixed_mclk = sampleRate * 256;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins{};
  pins.mck_io_num = mclk; pins.bck_io_num = bclk; pins.ws_io_num = ws;
  pins.data_out_num = dout; pins.data_in_num = I2S_PIN_NO_CHANGE;
  bool ok = i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
  ok = ok && writeReg(0x00, 0x1f); vTaskDelay(pdMS_TO_TICKS(20));
  // 4.096 MHz MCLK / 16 kHz: prediv=1, multiplier=1, LRCK=256.
  const uint8_t regs[][2] = {
    {0x00,0x00},{0x00,0x80},{0x01,0x3f},{0x02,0x00},{0x03,0x10},
    {0x04,0x10},{0x05,0x00},{0x06,0x03},{0x07,0x00},{0x08,0xff},
    {0x09,0x0c},{0x0a,0x0c},{0x0d,0x01},{0x0e,0x02},{0x12,0x00},
    {0x13,0x10},{0x1c,0x6a},{0x37,0x08},{0x32,0xbf},{0x31,0x60}
  };
  for (auto& r : regs) { if (!ok) break; ok = writeReg(r[0], r[1]); }
  if (!ok) { i2s_driver_uninstall(I2S_NUM_0); return false; }
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}
bool play(uint8_t volume) {
  // Cap digital peak well below full scale, even at UI 100%; ramp avoids clicks.
  if (!writeReg(0x31, 0x00)) return false;
  i2s_zero_dma_buffer(I2S_NUM_0); digitalWrite(pa, HIGH);
  vTaskDelay(pdMS_TO_TICKS(20));
  int16_t block[256 * 2]; bool ok = true;
  for (int index = 0; index < sampleRate * 3 / 2 && ok; index += 256) {
    int count = min(256, sampleRate * 3 / 2 - index);
    for (int j = 0; j < count; ++j) {
      int position = (index + j) % (sampleRate / 2);
      float envelope = position < 2880 ? min(1.0f, min(position / 240.0f, (2880-position) / 240.0f)) : 0;
      int16_t value = (int16_t)(sinf(2 * PI * 880 * (index+j) / sampleRate) * envelope * 6000 * volume / 100.0f);
      block[j*2] = block[j*2+1] = value;
    }
    size_t bytes = 0;
    ok = i2s_write(I2S_NUM_0, block, count * 4, &bytes, pdMS_TO_TICKS(100)) == ESP_OK && bytes == (size_t)count*4;
  }
  vTaskDelay(pdMS_TO_TICKS(80)); digitalWrite(pa, LOW);
  writeReg(0x31, 0x60); i2s_zero_dma_buffer(I2S_NUM_0);
  return ok;
}
void worker(void*) {
  uint8_t volume;
  for (;;) {
    if (xQueueReceive(requests, &volume, portMAX_DELAY) != pdTRUE) continue;
    if (volume && state == 0) state = initialize() ? 1 : -1;
    if (volume && state == 1 && !play(volume)) { digitalWrite(pa, LOW); state = -1; }
    busy = false;
  }
}
void begin() {
  pinMode(pa, OUTPUT); digitalWrite(pa, LOW);
  requests = xQueueCreate(1, sizeof(uint8_t));
  if (!requests || xTaskCreatePinnedToCore(worker, "hourly-audio", 4096, nullptr, 1, nullptr, 0) != pdPASS) state = -1;
}
bool request(uint8_t volume) {
  if (busy || !requests || state < 0) return false;
  busy = true;
  if (xQueueSend(requests, &volume, 0) != pdTRUE) { busy = false; return false; }
  return true;
}
}
