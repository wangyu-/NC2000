#pragma once

#include <cstdint>
#include <vector>

class Dsp {
public:
    static constexpr int kOrder = 10;
    static constexpr int kMaxSubframeSize = 60;
    static constexpr int kMaxFrameSamples = 240;
    static constexpr int kLtpHistorySize = 148;

    int dspMode;
    void (*callback)(unsigned char*, int);

    Dsp();

    void reset();
    void wakeup();
    void flush();
    void set_dual_mode(bool enable);
    void write(int high, int low);

private:
    void resetDecoderState();
    void resetSyllableState();
    void unpackAndDecodeFrame();
    void decodeCelpFrame(const uint8_t frame[18], uint8_t mode_bits, bool short_frame);
    void synthesizeSubframe(int sf, int pitch_idx, int fixpos, int grid,
                            int pitch_gidx, int inno_gidx, int sign0, int sign1,
                            bool two_pulse_mode);
    void decodePitch(int pitch_idx, int sf);
    double samplePitch(int pos, int frac, int s, int pcount);
    void interpolateLsp(int sf, double out_cos[kOrder]) const;
    static void lspToLpc(const double lsp_cos[kOrder], double lpc[kOrder + 1]);
    void emitSample8k(int16_t sample);

    // Packet assembly & mode state
    uint16_t packets_[15];
    int packet_off_;
    bool dual_mode_;        // false = NC1020 SPDS104A (2-pulse), true = NC2000/NC2600 SPDS104A-03B (dual-mode)
    int volume_q8_;         // Q8 linear gain from 0x9000..0x93FF (0x100 = 1.0x, 0x3FF = ~4.0x)

    // WORD_MODE (TTS syllable concatenation) state
    std::vector<int16_t> word_buf_;
    int cut_head_;          // 0xC2xx
    int cut_tail_;          // 0xC4xx
    int cut_frames_;        // 0xC8xx
    int data_cnt_;

    // CELP decoder state
    bool first_lsp_frame_;
    double prev_lsp_hz_[kOrder];
    double curr_lsp_hz_[kOrder];
    double lpc_state_[kOrder + 1];
    double ltp_hist_[kLtpHistorySize];
    double pitch_buf_[kMaxSubframeSize];
    int ltp_write_pos_;
    int ltp_read_base_;
    int prev_pitch_idx_;
    int pitch_period_;
    int pitch_frac_;
    int subframe_size_;
    double pulse_decay_;
};

void set_dsp_log_level(int level);
