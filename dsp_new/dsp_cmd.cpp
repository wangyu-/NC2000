// SPDS104A DSP hardware peripheral emulation:
//   - 6502 host bus registers ($20..$23 / $30..$33) per SPDS104A Programming Guide
//   - Unified 16-bit command & streaming protocol state machine
//   - WORD_MODE / DIPHONE streaming syllable trimming (0xC1/C2/C3/C4/C8)
//   - Internal 8 kHz PCM output FIFO, emulated DAC clock consumption, and busy backpressure

#include "dsp_cmd.h"
#include "dsp_core.h"

#include "comm.h"
#include "sound.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <vector>

namespace {

enum class DspMode : uint8_t {
    None = 0,
    Celp = 1,
    Word = 2,
    Diphone = 3,
    Pcm = 4,
};

// 0xD001 timing/speed calibration sub-protocol (verified in nc1020.rom 0x806d47 & nc2000.nand 0x19d1d81):
//   Host: 0xD001 -> count word (e.g. 0x00C8)
//   DSP:  0x5A (bit 6=1) -> host reads $31 (bit 6=0) -> countdown -> 0xFF (bit 6=1) -> host reads $31
//   Host: speed parameter word (e.g. 0x00FE, 0x0033..0x004C)
enum class D001State : uint8_t {
    Idle = 0,
    WaitCountParam = 1,
    WaitStartRead = 2,
    Counting = 3,
    WaitSpeedParam = 4,
};

// $30 / $20 status bits (SPDS104A Programming Guide p.11)
constexpr uint8_t kStatSleep = 0x80;    // B7: sleeping
constexpr uint8_t kStatRxReady = 0x40;  // B6: return data ready in $31 / $21
constexpr uint8_t kStatTxBusy = 0x30;   // B5:B4: forward data ack (unfetched by DSP)

// $30 / $20 control bits (SPDS104A Programming Guide p.11)
constexpr uint8_t kCtrlWakeup = 0x80;   // B7: wake up
constexpr uint8_t kCtrlReset = 0x40;    // B6: software reset

// Internal 8 kHz PCM output FIFO watermarks (unit: samples @ 8 kHz).
// Kept at 250 ms so SLEEP (0x8000) draining pcm_fifo_ always completes well under
// the 6502 BIOS 2-NMI (0.5-1.0 s) watchdog while keeping >10x margin over SDL callbacks.
constexpr size_t kPcmFifoBusyThreshold = 2000;  // 250 ms
constexpr size_t kPcmFifoDropLimit = 10000;     // 1250 ms

// Host SDL audio staging batch (80 samples @ 8 kHz = 10 ms, flushed at most at 100 Hz).
constexpr int kHostAudioBatchSamples = 80;
constexpr uint64_t kHostAudioFlushHz = 100;

// Exact floor(c * freq / CYCLES_SECOND) without 64-bit mul overflow at large cycle counts.
inline uint64_t ticks_at_freq(uint64_t c, uint64_t freq) {
    return (c / CYCLES_SECOND) * freq + (c % CYCLES_SECOND) * freq / CYCLES_SECOND;
}

class Spds104a {
public:
    Spds104a()
        : sleeping_(false),
          sleep_pending_(false),
          loopback_(false),
          tx_busy_(false),
          rx_ready_(false),
          rx_data_(0),
          data_low_(0),
          mode_(DspMode::Celp),
          head_skip_(0),
          cut_tail_(0),
          cut_frames_(0),
          syllable_frame_(0),
          d001_state_(D001State::Idle),
          d001_delay_cycles_(0),
          speed_param_(0x0040),
          host_out_count_(0),
          log_level_(1) {}

    void set_log_level(int level) {
        log_level_ = level;
        core_.set_log_level(level);
    }

    // Power-on / hardware reset: clears everything including forward-data latch (manual p.15).
    void hw_reset() {
        sw_reset();
        data_low_ = 0;
    }

    uint8_t read_status() const {
        uint8_t stat = 0;
        if (sleeping_) {
            stat |= kStatSleep;
        }
        if (rx_ready_) {
            stat |= kStatRxReady;
        }
        if (tx_busy_) {
            stat |= kStatTxBusy;
        }
        if (debug_level >= 3) {
            std::printf("[DSP] read_status -> %02x\n", stat);
        }
        return stat;
    }

    uint8_t read_data() {
        const uint8_t ret = rx_data_;
        const bool was_ready = rx_ready_;
        rx_ready_ = false;
        if (was_ready && d001_state_ == D001State::WaitStartRead && ret == 0x5A) {
            d001_state_ = D001State::Counting;
        }
        return ret;
    }

    void write_ctrl(uint8_t value) {
        core_.set_dual_mode(nc2000mode);

        if (value & kCtrlReset) {
            sw_reset();
        } else if (value & kCtrlWakeup) {
            sleeping_ = false;
            sleep_pending_ = false;
            tx_busy_ = false;
            // Hardware wakeup preserves volume and mode; only clears partial packet state.
            core_.clear_packet();
        }
    }

    void write_data_low(uint8_t value) {
        data_low_ = value;
    }

    void write_data_high(uint8_t high) {
        const uint8_t low = data_low_;
        const uint16_t cmd = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | low);
        core_.set_dual_mode(nc2000mode);

        if (log_level_ >= 3) {
            std::printf("[DSP] got word %04x (mode=%u)\n", cmd, static_cast<unsigned>(mode_));
        }

        // 1. Loopback test mode (0x7004): only 0xFFFF exits; all other words echo low byte
        if (loopback_) {
            if (cmd == 0xFFFF) {
                if (debug_level >= 1) std::printf("[DSP] exit loopback on 0xFFFF\n");
                loopback_ = false;
            } else {
                set_rx_data(low);
            }
            return;
        }

        // 2. Raw PCM streaming mode (0xA004): 0xFFxx exits back to CELP mode
        if (mode_ == DspMode::Pcm) {
            if (high == 0xFF) {
                if (log_level_ >= 1) std::printf("[DSP] mode PCM -> CELP\n");
                mode_ = DspMode::Celp;
            } else {
                int16_t sample = core_.decode_pcm_word(cmd);
                push_pcm(&sample, 1);
                latch_busy_if_needed();
            }
            return;
        }

        // 3. 0xD001 multi-word parameter state machine (swallows count & speed words before CELP dispatch)
        if (d001_state_ != D001State::Idle) {
            if (high >= 0x60) {
                // Host sent a new control command; abort incomplete 0xD001 sequence cleanly.
                d001_state_ = D001State::Idle;
                d001_delay_cycles_ = 0;
            } else if (d001_state_ == D001State::WaitCountParam) {
                const uint64_t ticks = std::max<uint64_t>(1, std::min<uint64_t>(cmd, 2000));
                d001_delay_cycles_ = (static_cast<uint64_t>(CYCLES_SECOND) * ticks) / DSP_AUDIO_HZ;
                if (debug_level >= 1) {
                    std::printf("[DSP] 0xD001 count=%u (delay=%llu cycles)\n",
                                static_cast<unsigned>(cmd),
                                static_cast<unsigned long long>(d001_delay_cycles_));
                }
                set_rx_data(0x5A);
                d001_state_ = D001State::WaitStartRead;
                return;
            } else if (d001_state_ == D001State::WaitSpeedParam) {
                speed_param_ = cmd;
                if (debug_level >= 1) {
                    std::printf("[DSP] 0xD001 speed_param=0x%04x\n",
                                static_cast<unsigned>(speed_param_));
                }
                d001_state_ = D001State::Idle;
                return;
            } else {
                d001_state_ = D001State::Idle;
                d001_delay_cycles_ = 0;
            }
        }

        // 4. CELP frame data packets (Package 0..5 -> high < 0x60)
        if (high < 0x60) {
            handle_celp_word(cmd);
            return;
        }

        // 5. Control & TTS commands (high >= 0x60)
        switch (high >> 4) {
            case 0x7:
                if (cmd == 0x7001 || cmd == 0x7002) {
                    set_rx_data(0x06);
                } else if (cmd == 0x7004) {
                    if (debug_level >= 1) std::printf("[DSP] enter loopback (0x7004)\n");
                    loopback_ = true;
                }
                break;

            case 0x8:
                // SLEEP (0x80xx): hardware decodes high byte 0x80 regardless of low byte
                // (nc2600 factory test at 0x1f68d writes $33=#$80 before $32=#$00, sending 0x80FF).
                // Per SPDS104A manual p.23/26, normal SLEEP waits until queued audio finishes
                // playing before asserting $30 bit 7 (sleeping).
                if (high == 0x80) {
                    if (debug_level >= 2) {
                        std::printf("[DSP] sleep (%04x, pending_pcm=%zu)\n", cmd, pcm_fifo_.size());
                    }
                    tx_busy_ = false;
                    if (pcm_fifo_.empty()) {
                        finish_sleep();
                    } else {
                        sleep_pending_ = true;
                    }
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
                if (cmd == 0xD001) {
                    if (debug_level >= 1) std::printf("[DSP] 0xD001 calibration start\n");
                    d001_state_ = D001State::WaitCountParam;
                    d001_delay_cycles_ = 0;
                } else if (high == 0xD0) {
                    if (debug_level >= 2) std::printf("[DSP] cmd %04x\n", cmd);
                }
                break;

            case 0xE:
                // 0xE000: CMD_DELAY (Not Defined / NOP in SPDS104A; used as a handshake delay word before 0xFFFF / 0x8000)
                if (high == 0xE0) {
                    if (debug_level >= 1) std::printf("[DSP] nop/delay (%04x)\n", cmd);
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

    // Advance DSP cycle-level timers and 8 kHz DAC clock across (last_cycles, cycles].
    void step(uint64_t last_cycles, uint64_t cycles) {
        if (cycles <= last_cycles || CYCLES_SECOND == 0) return;

        const uint64_t delta_cycles = cycles - last_cycles;
        step_timers(delta_cycles);

        const uint64_t dac_samples =
            ticks_at_freq(cycles, DSP_AUDIO_HZ) - ticks_at_freq(last_cycles, DSP_AUDIO_HZ);
        if (dac_samples > 0) {
            step_dac(dac_samples);
        }

        if (sleep_pending_ && pcm_fifo_.empty()) {
            finish_sleep();
        }

        if (host_out_count_ > 0 &&
            ticks_at_freq(cycles, kHostAudioFlushHz) != ticks_at_freq(last_cycles, kHostAudioFlushHz)) {
            flush_host_audio();
        }
    }

private:
    bool is_busy() const {
        return pcm_fifo_.size() > kPcmFifoBusyThreshold;
    }

    void set_rx_data(uint8_t value) {
        rx_data_ = value;
        rx_ready_ = true;
    }

    // Cycle-resolution hardware timing hook (0xD001 countdown; extensible for other timers).
    void step_timers(uint64_t delta_cycles) {
        if (d001_state_ == D001State::Counting) {
            if (d001_delay_cycles_ > delta_cycles) {
                d001_delay_cycles_ -= delta_cycles;
            } else {
                d001_delay_cycles_ = 0;
                set_rx_data(0xFF);
                d001_state_ = D001State::WaitSpeedParam;
            }
        }
    }

    // Drain `samples` at the emulated 8 kHz DAC clock and stage into host SDL audio batches.
    void step_dac(uint64_t samples) {
        while (samples > 0 && !pcm_fifo_.empty()) {
            host_out_buf_[host_out_count_++] = pcm_fifo_.front();
            pcm_fifo_.pop_front();
            --samples;
            if (host_out_count_ == kHostAudioBatchSamples) {
                flush_host_audio();
            }
        }

        if (!is_busy()) {
            tx_busy_ = false;
        }

        if (sleep_pending_ && pcm_fifo_.empty()) {
            finish_sleep();
        }
    }

    void finish_sleep() {
        flush_host_audio();
        sleeping_ = true;
        sleep_pending_ = false;
        tx_busy_ = false;
    }

    void flush_host_audio() {
        if (host_out_count_ > 0) {
            sound_push_dsp_samples(host_out_buf_, host_out_count_);
            host_out_count_ = 0;
        }
    }

    // Software reset ($30 bit 6): clears return-data latch, playback FIFO, and decoder state,
    // while preserving the forward-data latch `data_low_` (SPDS104A manual p.15).
    void sw_reset() {
        if (log_level_ >= 1) {
            std::printf("[DSP] reset\n");
            if (mode_ != DspMode::Celp) {
                std::printf("[DSP] mode %u -> CELP by reset\n", static_cast<unsigned>(mode_));
            }
        }
        sleeping_ = false;
        sleep_pending_ = false;
        loopback_ = false;
        tx_busy_ = false;
        rx_ready_ = false;
        rx_data_ = 0;

        mode_ = DspMode::Celp;
        head_skip_ = 0;
        cut_tail_ = 0;
        cut_frames_ = 0;
        syllable_frame_ = 0;
        d001_state_ = D001State::Idle;
        d001_delay_cycles_ = 0;
        pcm_fifo_.clear();
        host_out_count_ = 0;
        core_.reset();
    }

    void reset_syllable() {
        core_.reset_decoder();
        head_skip_ = 0;
        cut_tail_ = 0;
        cut_frames_ = 0;
        syllable_frame_ = 0;
    }

    void latch_busy_if_needed() {
        if (!sleeping_ && !sleep_pending_ && is_busy()) {
            tx_busy_ = true;
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
        frame_scratch_.clear();
        if (!core_.feed_celp_word(word, frame_scratch_)) {
            return;
        }

        if (mode_ == DspMode::Word || mode_ == DspMode::Diphone) {
            // Stream-trim each decoded frame on the fly using 0xC2/0xC4/0xC8 parameters
            // (real SPDS104A streams frame-by-frame rather than buffering an entire syllable).
            const int n = static_cast<int>(frame_scratch_.size());
            int end = n;
            if (cut_tail_ != 0) {
                if (syllable_frame_ == cut_frames_) {
                    end = std::min(cut_tail_, n);
                } else if (syllable_frame_ > cut_frames_) {
                    end = 0;
                }
            }
            const int start = std::min(head_skip_, end);
            head_skip_ -= std::min(head_skip_, n);
            if (end > start) {
                push_pcm(frame_scratch_.data() + start, static_cast<size_t>(end - start));
            }
            ++syllable_frame_;
        } else {
            push_pcm(frame_scratch_.data(), frame_scratch_.size());
        }
        latch_busy_if_needed();
    }

    void handle_mode_switch(uint8_t low) {
        if (log_level_ >= 2) {
            std::printf("[DSP] mode switch cmd 0xA0%02x\n", low);
        }
        if (low > static_cast<uint8_t>(DspMode::Pcm)) {
            if (log_level_ >= 1) std::printf("[DSP] ignored unknown mode 0xA0%02x\n", low);
            return;
        }
        DspMode next = static_cast<DspMode>(low);
        if (log_level_ >= 1 && mode_ != next) {
            std::printf("[DSP] mode %u -> %u\n",
                        static_cast<unsigned>(mode_), static_cast<unsigned>(next));
        }
        mode_ = next;
        head_skip_ = 0;
        cut_tail_ = 0;
        cut_frames_ = 0;
        syllable_frame_ = 0;
        core_.clear_packet();
    }

    void handle_word_cmd(uint8_t high, uint8_t low) {
        if (log_level_ >= 2) {
            std::printf("[DSP] word cmd %02x %02x\n", high, low);
        }
        if (mode_ != DspMode::Word && mode_ != DspMode::Diphone) return;

        switch (high) {
            case 0xC2:
                head_skip_ = low;
                syllable_frame_ = 0;
                break;
            case 0xC4:
                cut_tail_ = low;
                syllable_frame_ = 0;
                break;
            case 0xC8:
                cut_frames_ = low;
                syllable_frame_ = 0;
                break;
            case 0xC1:
            case 0xC3:
                reset_syllable();
                break;
            default:
                if (log_level_ >= 1) {
                    std::printf("[DSP] unhandled word cmd %02x %02x\n", high, low);
                }
                break;
        }
    }

    // Hardware interface registers (SPDS104A Programming Guide p.11-12)
    bool sleeping_;
    bool sleep_pending_;
    bool loopback_;
    bool tx_busy_;
    bool rx_ready_;
    uint8_t rx_data_;
    uint8_t data_low_;

    DspCore core_;
    DspMode mode_;

    // WORD_MODE / DIPHONE streaming syllable trimming (0xC2/0xC4/0xC8)
    std::vector<int16_t> frame_scratch_;
    int head_skip_;
    int cut_tail_;
    int cut_frames_;
    int syllable_frame_;

    // 0xD001 two-stage calibration handshake & speed parameter state
    D001State d001_state_;
    uint64_t d001_delay_cycles_;
    uint16_t speed_param_;

    // Internal 8 kHz PCM output FIFO
    std::deque<int16_t> pcm_fifo_;

    // Staging buffer to batch per-sample DAC pops before locking host SDL audio
    int16_t host_out_buf_[kHostAudioBatchSamples];
    int host_out_count_;

    int log_level_;
};

Spds104a g_dsp;

}  // namespace

uint8_t dsp_read_status() { return g_dsp.read_status(); }
uint8_t dsp_read_data() { return g_dsp.read_data(); }
void dsp_write_ctrl(uint8_t value) { g_dsp.write_ctrl(value); }
void dsp_write_data_low(uint8_t value) { g_dsp.write_data_low(value); }
void dsp_write_data_high(uint8_t value) { g_dsp.write_data_high(value); }
void dsp_hw_reset() { g_dsp.hw_reset(); }
void dsp_step(uint64_t last_cycles, uint64_t cycles) { g_dsp.step(last_cycles, cycles); }
void dsp_set_log_level(int level) { g_dsp.set_log_level(level); }
