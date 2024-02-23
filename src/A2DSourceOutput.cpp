#include "A2DSourceOutput.h"

bool A2DSourceOutput::begin(){
  _pushed = 0;
  _poped  = 0;
  _ts_poped = 0;
  _ts_begin = millis();
  _ts_wait_full = _ts_begin + 1000;
  _paused = false;
  _begined = true;
  return true;
}
void A2DSourceOutput::GetGainAndPause(uint16_t *volume, bool *pause){
  uint16_t v = (gainF2P6 >> 6) * 100 + (100.0f/64.0f) * (gainF2P6 & 0b00111111);
  if (v > 100) v = 100;
  *volume = v;
  *pause = _paused;
}
bool A2DSourceOutput::ConsumeSample(int16_t sample[2]){
  if(_pushed < _poped + FRAME_SZ){
    int idx = _pushed % FRAME_SZ;
    _frames[idx].channel1 = Amplify(sample[0]);
    _frames[idx].channel2 = Amplify(sample[1]);
    _pushed++;
    return true;
  }
  return false;
}
int A2DSourceOutput::monitor_message(char *message, int message_len){
  uint32_t ts_timeout = millis() - _ts_begin;
  if(ts_timeout > 60000){
    if(_paused == false && _poped - _ts_poped == 0 && _begined == true){
      Serial.println("one minitue, no samples consume, reboot ...");
      ESP.restart();
    }
    ts_timeout = 1;
    _ts_poped = _poped;
    _ts_begin = millis();
  }
  float audio_freq = (_poped-_ts_poped) / (ts_timeout/1000.0f);
  if(audio_freq < 32000 && _pushed - _poped < 512 && _begined == true){
    _ts_wait_full = millis() + 1000;
    Serial.printf(" _ts_wait_fill_samples 1second ... ");
  }
  return snprintf(message, message_len, ", audio: (%5.0fhz, %4d)", audio_freq, _pushed - _poped);
}
int32_t A2DSourceOutput::PopFrames(Frame *frame, int32_t frame_sz){
  int32_t can_poped_sz = _pushed - _poped;
  if(can_poped_sz >= frame_sz && _ts_wait_full < millis() && _paused == false){
    int32_t idx = _poped % FRAME_SZ;
    if(idx + frame_sz < FRAME_SZ){
      memcpy(&frame[0], &_frames[idx], sizeof(Frame) * frame_sz);
    }else{
      int32_t frame2end_sz = FRAME_SZ - idx;
      memcpy(&frame[0], &_frames[idx], sizeof(Frame) * frame2end_sz);
      memcpy(&frame[frame2end_sz], &_frames[0], sizeof(Frame) * (frame_sz - frame2end_sz));
    }
    _poped += frame_sz;
  }
  return frame_sz;
}
bool A2DSourceOutput::stop(){
  _poped = 0;
  _pushed = 0;
  _begined = false;
  return true;
}