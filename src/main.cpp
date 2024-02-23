#include <Arduino.h>

#if defined(ESP32)
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorAAC.h"
#include "AudioFileSourceHTTPStreamV2.h"
// #include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBufferEx.h"
#include "A2DSourceOutput.h"
#include "UDPMessageController.h"
#include "esp_heap_caps.h"

// To run, set your ESP8266 build to 160MHz, update the SSID info, and upload.

// Enter your WiFi setup here:
#ifndef STASSID
#define STASSID "scx"
#define STAPSK  "12345678"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

typedef A2DSourceOutput  MyAudioOutput;
typedef AudioFileSourceBufferEx       MyAudioFileSourceBuffer;
typedef AudioFileSourceHTTPStreamV2   MyHttpFileSource;
// typedef AudioFileSourceBuffer       MyAudioFileSourceBuffer;
// typedef AudioFileSourceHTTPStream   MyHttpFileSource;

BluetoothA2DPSource a2dp_source;
MyAudioOutput output;
static char *audio_url = (char*)"";
static char audio_from_server[512] = {0};
// static char audio_from_bakup[512] = {0};
// static uint64_t audio_url_expired = 0, _last_udp_msg_timestamp = 0;
// static uint32_t digital_token_sz = 0;
// static char digital_token[32] = {0};
static AudioGenerator *audio;
static MyHttpFileSource *file;
// AudioFileSourceHTTPStream *file;
// static AudioFileSource *file;
static AudioFileSource *buff;
typedef AudioGeneratorAAC MyAudioGenerator;



int32_t a2dp_fill_samples_cb(Frame *frames, int32_t frame_sz){
  output.PopFrames(frames, frame_sz);
  return frame_sz;
}

#define DEF_RRSI  (-999)
typedef struct a2dp_match_ctx_t{
  int32_t _max_db;
  uint32_t _timeout;
  esp_bd_addr_t _max_db_device;
}a2dp_match_ctx_t;

#define ESP32_SOURCE_NAME   ("ESP32_A2DP_SRC")
static a2dp_match_ctx_t _a2dp_match;
// static int auto_refresh_digital_token = 0;

static void a2dp_match_best_reset(){
  memset(&_a2dp_match, 0, sizeof(a2dp_match_ctx_t));
  _a2dp_match._max_db = DEF_RRSI;
}

/**
 * @brief 
 * 寻找指定时间内（3000毫秒）的信号最好的蓝牙音箱
 * @param name 设备名称
 * @param address 设备地址
 * @param rrsi 信号强度
 * @return true 
 * @return false 
 */
static bool a2dp_match_best_signal_device_cb(const char *name, esp_bd_addr_t address, int rrsi){
  bool found = false;
  a2dp_match_ctx_t *ctx = &_a2dp_match;
  uint64_t ts_now = millis();
  
  if(ctx->_max_db < rrsi && strncmp(ESP32_SOURCE_NAME, name, strlen(ESP32_SOURCE_NAME)) != 0){
    if(ctx->_timeout == 0){
      ctx->_timeout = ts_now + 3000;
    }
    ctx->_max_db = rrsi;
    memcpy(ctx->_max_db_device, address, sizeof(esp_bd_addr_t));
  }
  if(ctx->_max_db != DEF_RRSI && ctx->_timeout < ts_now){
    if(memcmp(address, ctx->_max_db_device, sizeof(esp_bd_addr_t)) == 0){
      found = true;
    }
  }  
  Serial.printf("a2dp_match_best_signal_device_cb: %s, db: %d, found: %d\n", name, rrsi, found);
  return found;
}

void stop_current_audio() {
  if (audio) {
    if (audio->isRunning()) {
      audio->stop();
    }
    
    file->close();
    buff->close();
    output.stop();
    
    delete audio;
    delete file;
    delete buff;
    
    audio = NULL;
    file = NULL;
    buff = NULL;
  }
}

static int main_xnet_message_handler(const xnet_message_t *msg){
  switch(msg->_opcode){
    case XNET_ACK_MUSIC_NEXT:{
      memset(audio_from_server, 0, sizeof(audio_from_server));
      strncpy(audio_from_server, (const char*)msg->_payload, msg->_payload_sz);
      // Serial.printf("recv url: %s\n", audio_url);
      audio_url = audio_from_server;
      // audio_url_expired = millis() + 3000;
      if(audio) {
        stop_current_audio();
      }
    }
    break;
    case XNET_ACK_PLAYER_STATE:{
      float gain = msg->_uparam / 100.0f;
      output.SetGain(gain);
      output.SetPause(msg->_wparam ? true : false);
      // Serial.printf("set gain %.1f, pause: %d by xnet\n", gain, msg->_wparam);
    }
    break;
  }
  return 0;
}


// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  if (code != 257) {
    const char *ptr = reinterpret_cast<const char *>(cbData);
    // Note that the string may be in PROGMEM, so copy it to RAM for printf
    char s1[64];
    strncpy_P(s1, string, sizeof(s1));
    s1[sizeof(s1)-1]=0;
    Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
    Serial.flush();
  }
}

void setup()
{
  Serial.begin(115200);
  uint64_t ts_expired = 0;
  delay(1000);
  Serial.println("Connecting to WiFi");

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(ssid, password);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");

  XNetController::setup(main_xnet_message_handler);
  

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  // 连接蓝牙设备
  a2dp_source.set_discoverability(ESP_BT_NON_DISCOVERABLE);
  a2dp_source.set_auto_reconnect(true);
  a2dp_source.set_ssid_callback(a2dp_match_best_signal_device_cb);
  a2dp_source.start("tom and jerry", a2dp_fill_samples_cb);
  a2dp_source.set_volume(30);

  a2dp_match_best_reset();
  ts_expired = millis() + 20000;
  while(!a2dp_source.is_connected() && ts_expired > millis()){
    Serial.println("... connecting bluetooth speaker ...");
    delay(1000);
  }
  audioLogger = &Serial;
}


void loop()
{
  XNetController::loop();
  static int lastms = 0;
  if (audio) {
    if (audio->isRunning()) {
      if (millis()-lastms > 1000) {
        lastms = millis();
        // Serial.printf("Running for %d ms...\n", lastms);
        // Serial.flush();
      }
      if (!audio->loop()) {
        audio->stop();
        delete audio;
        audio = NULL;
      }
    } else {
      delete audio;
      audio = NULL;
    }
  } else {
    if (strlen(audio_url) != 0) {
      Serial.printf("play music: %s\n", audio_url);
      file = new MyHttpFileSource(audio_url);
      file->RegisterMetadataCB(MDCallback, (void*)"ICY");
      buff = new MyAudioFileSourceBuffer(file, 1024*8);
      buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
      audio = new MyAudioGenerator();
      // output1 = new AudioOutputI2SNoDAC();
      audio->RegisterStatusCB(StatusCallback, (void*)"audio");
      audio->begin(buff, &output);
    }
    Serial.printf("audio done\n");
    Serial.printf("SDK version:%s\n", esp_get_idf_version());
    //获取芯片可用内存
    Serial.printf("heap_caps_get_free_size : %d  \n", heap_caps_get_free_size(MALLOC_CAP_EXEC));
    //获取从未使用过的最小内存
    Serial.printf("heap_caps_get_minimum_free_size : %d  \n", heap_caps_get_minimum_free_size(MALLOC_CAP_EXEC));
    Serial.printf("heap_caps_get_largest_free_block : %d  \n", heap_caps_get_largest_free_block(MALLOC_CAP_EXEC));
    
    delay(1000);
    audio_url = (char*)"";
    XNetController::req_new_music(false);
  }
}
