// SPDS104A DSP hardware peripheral emulation:
//   - 6502 host bus registers ($20..$23 / $30..$33)
//   - Unified 16-bit command & streaming protocol state machine
//   - WORD_MODE / DIPHONE syllable buffer & trimming (0xC1/C2/C3/C4/C8)
//   - Internal 8 kHz PCM output FIFO, emulated DAC clock consumption, and busy backpressure

#include "dsp_cmd.h"
#include "dsp_core.h"

#include "comm.h"
#include "sound.h"
#include "state.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <vector>

extern nc2k_states_t nc2k_states;

namespace {

enum class DspMode : uint8_t {
    None = 0,
    Celp = 1,
    Word = 2,
    Diphone = 3,
    Pcm = 4,
};

constexpr uint8_t kStatSleep = 0x80;
constexpr uint8_t kStatRxReady = 0x40;
constexpr uint8_t kStatTxBusy = 0x30;

constexpr uint8_t kCtrlWakeup = 0x80;
constexpr uint8_t kCtrlReset = 0x40;

// Internal 8 kHz PCM output FIFO watermarks (unit: samples @ 8 kHz).
// Busy threshold controls 6502 backpressure ($30 bits 5:4); drop limit guards
// against software that ignores busy polling.
constexpr size_t kPcmFifoBusyThreshold = 5000;  // 625 ms
constexpr size_t kPcmFifoDropLimit = 10000;     // 1250 ms

class Spds104a {
public:
    Spds104a()
        : mode_(DspMode::Celp),
          cut_head_(0),
          cut_tail_(0),
          cut_frames_(0),
          word_data_cnt_(0),
          log_level_(1) {}

    void set_log_level(int level) {
        log_level_ = level;
        core_.set_log_level(level);
    }

    bool is_busy() const {
        return pcm_fifo_.size() > kPcmFifoBusyThreshold;
    }

    uint8_t read_status() {
        uint8_t stat = 0;
        if (nc2k_states.dspSleep) {
            stat |= kStatSleep;
        }

        if (!nc2k_states.dspSleep && is_busy()) {
            if (nc2k_states.dsp_data_feeded_but_hasnt_fetched) {
                stat |= kStatTxBusy;
            }
        } else {
            nc2k_states.dsp_data_feeded_but_hasnt_fetched = false;
        }

        if (!rx_fifo_.empty() || nc2k_states.dspRetData != -1) {
            stat |= kStatRxReady;
        }

        if (debug_level >= 3) {
            std::printf("[DSP] read_status -> %02x\n", stat);
        }
        return stat;
    }

    uint8_t read_data() {
        if (!rx_fifo_.empty()) {
            uint8_t ret = rx_fifo_.front();
            rx_fifo_.pop_front();
            return ret;
        }
        if (nc2k_states.dspRetData != -1) {
            uint8_t ret = static_cast<uint8_t>(nc2k_states.dspRetData);
            nc2k_states.dspRetData = -1;
            return ret;
        }
        return 0xFF;
    }

    void write_ctrl(uint8_t value) {
        core_.set_dual_mode(nc2000mode);

        if (value & kCtrlReset) {
            reset();
        } else if (value & kCtrlWakeup) {
            nc2k_states.dspSleep = false;
            // Hardware wakeup preserves volume and mode; only clears partial packet state.
            core_.clear_packet();
        }
    }

    void write_data_low(uint8_t value) {
        nc2k_states.dsp_data_low = value;
    }

    void write_data_high(uint8_t high) {
        const uint8_t low = nc2k_states.dsp_data_low;
        const uint16_t cmd = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | low);
        core_.set_dual_mode(nc2000mode);

        if (log_level_ >= 3) {
            std::printf("[DSP] got word %04x (mode=%u)\n", cmd, static_cast<unsigned>(mode_));
        }

        // 1. Loopback / trans test mode (0x7004): only 0xFFFF exits; all other words echo low byte
        if (nc2k_states.dspTrans) {
            if (cmd == 0xFFFF) {
                if (debug_level >= 1) std::printf("[DSP] exit loopback on 0xFFFF\n");
                nc2k_states.dspTrans = false;
            } else {
                rx_fifo_.push_back(low);
            }
            return;
        }

        // 2. Raw PCM streaming mode (0xA004): 0xFFxx exits back to CELP mode
        if (mode_ == DspMode::Pcm) {
            if (high == 0xFF) {
                if (log_level_ >= 1) std::printf("[DSP] mode PCM -> CELP\n");
                mode_ = DspMode::Celp;
            } else {
                latch_busy_if_needed();
                int16_t sample = core_.decode_pcm_word(cmd);
                push_pcm(&sample, 1);
            }
            return;
        }

        // 3. CELP frame data packets (Package 0..5 -> high < 0x60)
        if (high < 0x60) {
            handle_celp_word(cmd);
            return;
        }

        // 4. Control & TTS commands (high >= 0x60)
        switch (high >> 4) {
            case 0x7:
                if (cmd == 0x7001 || cmd == 0x7002) {
                    rx_fifo_.push_back(0x06);
                } else if (cmd == 0x7004) {
                    if (debug_level >= 1) std::printf("[DSP] enter loopback (0x7004)\n");
                    nc2k_states.dspTrans = true;
                }
                break;

            case 0x8:
                // SLEEP (0x80xx): hardware decodes high byte 0x80 regardless of low byte
                // (nc2600 factory test at 0x1f68d writes $33=#$80 before $32=#$00, sending 0x80FF)
                if (high == 0x80) {
                    if (debug_level >= 2) std::printf("[DSP] sleep (%04x)\n", cmd);
                    nc2k_states.dspSleep = true;
                }
                break;

            case 0x9:
                // Volume control: 0x9000..0x93FF (Q8 gain 0.0x .. ~4.0x)
                if (high <= 0x93) {
                    int vol = ((high & 0x03) << 8) | low;
                    core_.set_volume_q8(vol);
                    if (log_level_ >= 2) {
                        std::printf("[DSP] volume=0x%03x (%.2fx)\n", vol, vol / 256.0);
                    }
                }
                break;

            case 0xA:
                if (high == 0xA0) {
                    handle_mode_switch(low);
                }
                break;

            case 0xC:
                // TTS WORD_MODE / DIPHONE syllable parameters (0xC1/C2/C3/C4/C8)
                handle_word_cmd(high, low);
                break;

            case 0xD:
                // 0xD000 / 0xD001 sync/status query: returns 0x5A followed by 0xFF
                if (high == 0xD0) {
                    if (debug_level >= 1) std::printf("[DSP] sync query %04x\n", cmd);
                    rx_fifo_.push_back(0x5A);
                    rx_fifo_.push_back(0xFF);
                }
                break;

            case 0xE:
                // 0xE000: CMD_DELAY / flush pending packet & syllable buffer
                if (high == 0xE0) {
                    if (debug_level >= 1) std::printf("[DSP] flush (%04x)\n", cmd);
                    core_.clear_packet();
                    word_data_cnt_ = 0;
                    word_buf_.clear();
                }
                break;

            case 0xF:
                // 0xF000 (SPEECH_END) / 0xFFFF
                if (high == 0xF0 || cmd == 0xFFFF) {
                    reset_syllable();
                }
                break;

            default:
                if (debug_level >= 1) std::printf("[DSP] unhandled cmd %04x\n", cmd);
                break;
        }
    }

    // Drain up to `samples` at emulated 8 kHz DAC clock and forward to host SDL audio sink.
    void tick(int samples) {
        if (samples <= 0) return;

        int16_t batch[256];
        int remaining = samples;
        while (remaining > 0 && !pcm_fifo_.empty()) {
            int n = std::min(remaining, static_cast<int>(sizeof(batch) / sizeof(batch[0])));
            int actual = 0;
            while (actual < n && !pcm_fifo_.empty()) {
                batch[actual++] = pcm_fifo_.front();
                pcm_fifo_.pop_front();
            }
            if (actual > 0) {
                sound_push_dsp_samples(batch, actual);
                remaining -= actual;
            }
        }

        if (!is_busy()) {
            nc2k_states.dsp_data_feeded_but_hasnt_fetched = false;
        }
    }

private:
    void reset() {
        if (log_level_ >= 1) {
            std::printf("[DSP] reset\n");
            if (mode_ != DspMode::Celp) {
                std::printf("[DSP] mode %u -> CELP by reset\n", static_cast<unsigned>(mode_));
            }
        }
        nc2k_states.dspSleep = false;
        nc2k_states.dspTrans = false;
        nc2k_states.dspRetData = -1;
        nc2k_states.dsp_0x7001_0x7002 = 0;
        nc2k_states.dsp_0xd0 = false;
        nc2k_states.dsp_data_feeded_but_hasnt_fetched = false;
        rx_fifo_.clear();

        mode_ = DspMode::Celp;
        cut_head_ = 0;
        cut_tail_ = 0;
        cut_frames_ = 0;
        word_data_cnt_ = 0;
        word_buf_.clear();
        core_.reset();
    }

    void reset_syllable() {
        core_.reset_decoder();
        word_data_cnt_ = 0;
        word_buf_.clear();
    }

    void latch_busy_if_needed() {
        if (!nc2k_states.dspSleep && is_busy()) {
            nc2k_states.dsp_data_feeded_but_hasnt_fetched = true;
        }
    }

    void push_pcm(const int16_t* samples, size_t count) {
        if (enable_debug_dsp) {
            static int log_cnt = 0;
            if (++log_cnt % 1000 == 0) {
                std::printf("[DSP] pcm_fifo=%zu\n", pcm_fifo_.size());
            }
        }
        for (size_t i = 0; i < count; ++i) {
            if (pcm_fifo_.size() >= kPcmFifoDropLimit) {
                if (enable_debug_dsp) {
                    std::printf("[DSP] pcm_fifo full, dropping (len=%zu)\n", pcm_fifo_.size());
                }
                break;
            }
            pcm_fifo_.push_back(samples[i]);
        }
    }

    void handle_celp_word(uint16_t word) {
        if (mode_ != DspMode::Celp && mode_ != DspMode::Word && mode_ != DspMode::Diphone) {
            if (log_level_ >= 2) {
                std::printf("[DSP] celp word %04x ignored in mode %u\n",
                            word, static_cast<unsigned>(mode_));
            }
            return;
        }

        latch_busy_if_needed();
        ++word_data_cnt_;

        if (mode_ == DspMode::Word || mode_ == DspMode::Diphone) {
            core_.feed_celp_word(word, word_buf_);
        } else {
            frame_scratch_.clear();
            if (core_.feed_celp_word(word, frame_scratch_)) {
                push_pcm(frame_scratch_.data(), frame_scratch_.size());
            }
        }
    }

    void handle_mode_switch(uint8_t low) {
        if (log_level_ >= 2) {
            std::printf("[DSP] mode switch cmd 0xA0%02x\n", low);
        }
        if (low > static_cast<uint8_t>(DspMode::Pcm)) {
            if (log_level_ >= 1) std::printf("[DSP] invalid mode %02x, resetting\n", low);
            reset();
            return;
        }
        DspMode next = static_cast<DspMode>(low);
        if (log_level_ >= 1 && mode_ != next) {
            std::printf("[DSP] mode %u -> %u\n",
                        static_cast<unsigned>(mode_), static_cast<unsigned>(next));
        }
        mode_ = next;
        word_data_cnt_ = 0;
        core_.clear_packet();
    }

    void handle_word_cmd(uint8_t high, uint8_t low) {
        if (log_level_ >= 2) {
            std::printf("[DSP] word cmd %02x %02x\n", high, low);
        }
        if (mode_ != DspMode::Word && mode_ != DspMode::Diphone) return;

        switch (high) {
            case 0xC2:
                cut_head_ = low;
                word_data_cnt_ = 0;
                break;
            case 0xC4:
                cut_tail_ = low;
                word_data_cnt_ = 0;
                break;
            case 0xC8:
                cut_frames_ = low;
                word_data_cnt_ = 0;
                break;
            case 0xC1:
            case 0xC3: {
                if (word_data_cnt_ % DspCore::kWordsPerFrame != 0) {
                    if (log_level_ >= 1) {
                        std::printf("[DSP] word_data_cnt=%d not multiple of 15 on %02x, reset syllable\n",
                                    word_data_cnt_, high);
                    }
                    reset_syllable();
                    return;
                }
                int num_frames = word_data_cnt_ / DspCore::kWordsPerFrame;
                int end = (cut_tail_ == 0)
                    ? (DspCore::kMaxFrameSamples * num_frames)
                    : (cut_tail_ + DspCore::kMaxFrameSamples * (num_frames - 1));
                int buf_sz = static_cast<int>(word_buf_.size());
                if (end > buf_sz) {
                    if (log_level_ >= 1) {
                        std::printf("[DSP] syllable end=%d > buf_size=%d, clamping\n", end, buf_sz);
                    }
                    end = buf_sz;
                }
                int start = std::max(0, std::min(cut_head_, end));
                if (end > start) {
                    push_pcm(word_buf_.data() + start, static_cast<size_t>(end - start));
                }
                reset_syllable();
                break;
            }
            default:
                if (log_level_ >= 1) {
                    std::printf("[DSP] unhandled word cmd %02x %02x\n", high, low);
                }
                break;
        }
    }

    DspCore core_;
    DspMode mode_;

    // WORD_MODE / DIPHONE syllable assembly & trimming
    std::vector<int16_t> word_buf_;
    std::vector<int16_t> frame_scratch_;
    int cut_head_;
    int cut_tail_;
    int cut_frames_;
    int word_data_cnt_;

    // Host bus RX FIFO (DSP -> 6502) and internal 8 kHz PCM output FIFO
    std::deque<uint8_t> rx_fifo_;
    std::deque<int16_t> pcm_fifo_;

    int log_level_;
};

Spds104a g_dsp;

}  // namespace

uint8_t dsp_read_status() { return g_dsp.read_status(); }
uint8_t dsp_read_data() { return g_dsp.read_data(); }
void dsp_write_ctrl(uint8_t value) { g_dsp.write_ctrl(value); }
void dsp_write_data_low(uint8_t value) { g_dsp.write_data_low(value); }
void dsp_write_data_high(uint8_t value) { g_dsp.write_data_high(value); }
void dsp_tick(int samples) { g_dsp.tick(samples); }
bool dsp_is_busy() { return g_dsp.is_busy(); }
void dsp_set_log_level(int level) { g_dsp.set_log_level(level); }
