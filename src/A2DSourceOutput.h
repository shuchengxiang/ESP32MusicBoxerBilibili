#include "BluetoothA2DPSource.h"
#include "AudioOutput.h"

class A2DSourceOutput : public AudioOutput
{
  enum{FRAME_SZ = 1536};
  public:
    A2DSourceOutput(){}
    virtual bool begin();
    virtual void GetGainAndPause(uint16_t *volume, bool *pause);
    virtual bool ConsumeSample(int16_t sample[2]);
    virtual int monitor_message(char *message, int message_len);
    virtual int32_t PopFrames(Frame *frame, int32_t frame_sz);
    virtual bool stop();
    bool SetPause(bool enable){ _paused = enable; return _paused; }
    bool SetLsbJustified(bool lsb){ return true; }
  protected:
    Frame _frames[FRAME_SZ];
    int32_t _pushed, _poped;
    uint64_t _ts_begin, _ts_poped, _ts_wait_full;
    bool _paused, _begined;
};