#pragma once

#include <cstdint>
#include <vector>

// Pure S480 / CELP & PCM audio synthesis core (zero emulator/IO dependencies).
// Supports both legacy single-mode 2-pulse CELP (PC1000/NC1020) and
// dual-mode 2-pulse + sparse ternary stochastic codebook CELP (NC2000/NC2600).
class DspCore {
public:
    static constexpr int kOrder = 10;
    static constexpr int kMaxSubframeSize = 60;
    static constexpr int kMaxFrameSamples = 240;
    static constexpr int kLtpHistorySize = 148;
    static constexpr int kWordsPerFrame = 15;

    DspCore();

    void set_dual_mode(bool enable);
    void set_volume_q8(int volume_q8);
    void set_log_level(int level);

    // Full chip reset (restores default volume and resets decoder state).
    void reset();

    // Per-syllable / per-utterance reset (preserves volume and dual_mode).
    void reset_decoder();

    // Discard any partially assembled 15-word CELP frame.
    void clear_packet();

    // Feed one 16-bit CELP packet word (high byte < 0x60).
    // When a complete 15-word frame has been assembled, decodes 160 or 240
    // 8 kHz signed 16-bit PCM samples into `out` and returns true.
    bool feed_celp_word(uint16_t word, std::vector<int16_t>& out);

    // Decode one raw unsigned 16-bit PCM word (0xA004 streaming mode).
    int16_t decode_pcm_word(uint16_t word) const;

private:
    void unpack_and_decode_frame(std::vector<int16_t>& out);
    void decode_celp_frame(const uint8_t frame[18], uint8_t mode_bits,
                           bool short_frame, std::vector<int16_t>& out);
    void synthesize_subframe(int sf, int pitch_idx, int fixpos, int grid,
                             int pitch_gidx, int inno_gidx, int sign0, int sign1,
                             bool two_pulse_mode, std::vector<int16_t>& out);
    void decode_pitch(int pitch_idx, int sf);
    double sample_pitch(int pos, int frac, int s, int pcount);
    void interpolate_lsp(int sf, double out_cos[kOrder]) const;
    static void lsp_to_lpc(const double lsp_cos[kOrder], double lpc[kOrder + 1]);

    // Packet reassembly
    uint16_t packets_[kWordsPerFrame];
    int packet_off_;

    // Configuration
    bool dual_mode_;
    uint16_t& volume_q8_;  // Q8 linear gain from 0x9000..0x93FF (bound to nc2k_states.dsp_state.volume_q8)
    int log_level_;

    // CELP synthesis filter & LTP history state
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
