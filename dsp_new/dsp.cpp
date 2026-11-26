// Clean-room Sunplus SPDS104A / SPDS104A-03B CELP & PCM DSP emulator.
// Implements both legacy single-mode (NC1020) and dual-mode S480 (NC2000/NC2600)
// based on public SPDS102A/SPDS104A bitstream specifications and standard CELP synthesis.

#include "dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kNoneMode = 0;
constexpr int kCelpMode = 1;
constexpr int kWordMode = 2;
constexpr int kDiphoneMode = 3;
constexpr int kPcmMode = 4;

constexpr double kPi = 3.14159265358979323846;

bool g_log_info = true;
bool g_log_verbose = false;
bool g_log_verbose2 = false;

// Bit allocation per LSP band (3,4,4,4,4,3,3,3,3,3 -> 112 levels total)
constexpr int kLspBandOffset[Dsp::kOrder] = {0, 8, 24, 40, 56, 72, 80, 88, 96, 104};

// Sunplus SPDS104A / S480 scalar LSP quantization grid in Hz (8 kHz sample rate, Nyquist = 4000 Hz).
// Required for bitstream interoperability with existing ROM voice data.
constexpr float kLspFreqHz[112] = {
    // Band 0 (3-bit)
    100.0f, 170.0f, 225.0f, 250.0f, 280.0f, 340.0f, 420.0f, 500.0f,
    // Band 1 (4-bit)
    210.0f, 235.0f, 265.0f, 295.0f, 325.0f, 360.0f, 400.0f, 440.0f,
    480.0f, 520.0f, 560.0f, 610.0f, 670.0f, 740.0f, 810.0f, 880.0f,
    // Band 2 (4-bit)
    420.0f, 460.0f, 500.0f, 540.0f, 585.0f, 640.0f, 705.0f, 775.0f,
    850.0f, 950.0f, 1050.0f, 1150.0f, 1250.0f, 1350.0f, 1450.0f, 1550.0f,
    // Band 3 (4-bit)
    620.0f, 660.0f, 720.0f, 795.0f, 880.0f, 970.0f, 1080.0f, 1170.0f,
    1270.0f, 1370.0f, 1470.0f, 1570.0f, 1670.0f, 1770.0f, 1870.0f, 1970.0f,
    // Band 4 (4-bit)
    1000.0f, 1050.0f, 1130.0f, 1210.0f, 1285.0f, 1350.0f, 1430.0f, 1510.0f,
    1590.0f, 1670.0f, 1750.0f, 1850.0f, 1950.0f, 2050.0f, 2150.0f, 2250.0f,
    // Band 5 (3-bit)
    1470.0f, 1570.0f, 1690.0f, 1830.0f, 2000.0f, 2200.0f, 2400.0f, 2600.0f,
    // Band 6 (3-bit)
    1800.0f, 1880.0f, 1960.0f, 2100.0f, 2300.0f, 2480.0f, 2700.0f, 2900.0f,
    // Band 7 (3-bit)
    2225.0f, 2400.0f, 2525.0f, 2650.0f, 2800.0f, 2950.0f, 3150.0f, 3350.0f,
    // Band 8 (3-bit)
    2760.0f, 2880.0f, 3000.0f, 3100.0f, 3200.0f, 3310.0f, 3430.0f, 3550.0f,
    // Band 9 (3-bit)
    3190.0f, 3270.0f, 3350.0f, 3420.0f, 3490.0f, 3590.0f, 3710.0f, 3830.0f,
};

// Legacy SPDS104A (NC1020) 4-bit innovation gain table (interoperability parameter).
constexpr double kLegacyCGain[16] = {
    0.0, 1.0, 3.0, 13.0, 35.0, 64.0, 98.0, 136.0,
    178.0, 224.0, 278.0, 340.0, 418.0, 520.0, 660.0, 870.0,
};

// Generated tables (computed once from pure math formulas, zero copied lookup data)
struct GeneratedTables {
    double dual_cgain[16];
    double frac_sinc[2][8];        // 8-tap Hamming-windowed sinc for 1/3 and 2/3 sample delay
    int8_t stochastic_cb[1084];    // Ternary sparse {-1, 0, +1} codebook (~75% zeros)

    GeneratedTables() {
        // Dual-mode (NC2000/NC2600) logarithmic innovation gain:
        // +4 dB/step (2^(2/3)) for indices 1..12, +2 dB/step (2^(1/3)) for indices 13..15
        dual_cgain[0] = 0.0;
        for (int i = 1; i <= 12; ++i) {
            dual_cgain[i] = std::floor(std::pow(2.0, (2 * i + 8) / 3.0));
        }
        for (int i = 13; i <= 15; ++i) {
            dual_cgain[i] = std::floor(std::pow(2.0, (i + 20) / 3.0));
        }

        // 8-tap Hamming-windowed sinc fractional-delay filters for F=1 (1/3) and F=2 (2/3)
        for (int f = 0; f < 2; ++f) {
            double delay = (f + 1) / 3.0;
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                double t = (k - 3) - delay;
                double s = (std::abs(t) < 1e-9) ? 1.0 : std::sin(kPi * t) / (kPi * t);
                double w = 0.54 + 0.46 * std::cos(kPi * t / 4.0);
                frac_sinc[f][k] = s * w;
                sum += frac_sinc[f][k];
            }
            for (int k = 0; k < 8; ++k) {
                frac_sinc[f][k] /= sum;
            }
        }

        // Deterministic sparse ternary stochastic excitation sequence (~75% 0, ~12.5% +1, ~12.5% -1)
        uint32_t rng = 0x5A17C3E9u;
        for (int i = 0; i < 1084; ++i) {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            uint32_t r = rng & 7u;
            if (r == 0u) {
                stochastic_cb[i] = 1;
            } else if (r == 1u) {
                stochastic_cb[i] = -1;
            } else {
                stochastic_cb[i] = 0;
            }
        }
    }
};

const GeneratedTables& tables() {
    static const GeneratedTables kTables;
    return kTables;
}

inline double pitchGain(int gidx) {
    return (gidx & 0x0F) * 0.075;
}

// Decode C(30, 2) = 435 lexicographical pulse pair index (0 <= p0 < p1 < 30)
inline void decodePulsePair(int index, int& p0, int& p1) {
    if (index < 0) index = 0;
    if (index >= 435) index = 434;
    p0 = 0;
    int rem = index;
    while (p0 < 28 && rem >= (29 - p0)) {
        rem -= (29 - p0);
        ++p0;
    }
    p1 = p0 + 1 + rem;
}

class BitReader {
public:
    explicit BitReader(const uint8_t* data) : data_(data), bit_pos_(0) {}

    int read(int nbits) {
        int val = 0;
        for (int i = 0; i < nbits; ++i) {
            int byte_idx = bit_pos_ >> 3;
            int bit_idx = bit_pos_ & 7;
            val |= ((data_[byte_idx] >> bit_idx) & 1) << i;
            ++bit_pos_;
        }
        return val;
    }

private:
    const uint8_t* data_;
    int bit_pos_;
};

}  // namespace

void set_dsp_log_level(int level) {
    g_log_info = level >= 1;
    g_log_verbose = level >= 2;
    g_log_verbose2 = level >= 3;
}

Dsp::Dsp()
    : dspMode(kCelpMode),
      callback(nullptr),
      packet_off_(0),
      dual_mode_(false),
      volume_q8_(0x3FF),
      cut_head_(0),
      cut_tail_(0),
      cut_frames_(0),
      data_cnt_(0),
      first_lsp_frame_(true),
      ltp_write_pos_(kLtpHistorySize - 1),
      ltp_read_base_(0),
      prev_pitch_idx_(0),
      pitch_period_(13),
      pitch_frac_(0),
      subframe_size_(60),
      pulse_decay_(0.5) {
    std::memset(packets_, 0, sizeof(packets_));
    resetDecoderState();
}

void Dsp::set_dual_mode(bool enable) {
    dual_mode_ = enable;
}

void Dsp::resetDecoderState() {
    first_lsp_frame_ = true;
    std::memset(prev_lsp_hz_, 0, sizeof(prev_lsp_hz_));
    std::memset(curr_lsp_hz_, 0, sizeof(curr_lsp_hz_));
    std::memset(lpc_state_, 0, sizeof(lpc_state_));
    std::memset(ltp_hist_, 0, sizeof(ltp_hist_));
    std::memset(pitch_buf_, 0, sizeof(pitch_buf_));
    ltp_write_pos_ = kLtpHistorySize - 1;
    ltp_read_base_ = 0;
    prev_pitch_idx_ = 0;
    pitch_period_ = 13;
    pitch_frac_ = 0;
    pulse_decay_ = 0.5;
}

void Dsp::resetSyllableState() {
    resetDecoderState();
    data_cnt_ = 0;
    packet_off_ = 0;
    word_buf_.clear();
}

void Dsp::reset() {
    if (g_log_info) {
        std::printf("[DSP] reset\n");
        if (dspMode != kCelpMode) {
            std::printf("[DSP] dsp mode change from %02x to %02x by reset\n", dspMode, kCelpMode);
        }
    }
    dspMode = kCelpMode;
    volume_q8_ = 0x3FF;
    cut_head_ = 0;
    cut_tail_ = 0;
    cut_frames_ = 0;
    resetSyllableState();
}

void Dsp::wakeup() {
    // Hardware WakeUp preserves configured volume and mode; only clear partial packet state.
    packet_off_ = 0;
}

void Dsp::flush() {
    // 0xE000 (CMD_DELAY / flush): clear any pending partial packet or syllable buffer.
    packet_off_ = 0;
    data_cnt_ = 0;
    word_buf_.clear();
}

void Dsp::write(int high, int low) {
    high &= 0xFF;
    low &= 0xFF;
    if (g_log_verbose2) {
        std::printf("[DSP] got data %02x %02x\n", high, low);
    }

    // Raw PCM streaming mode: 0xFFxx exits back to CELP mode
    if (dspMode == kPcmMode) {
        if (high == 0xFF) {
            if (g_log_info) {
                std::printf("[DSP] dsp mode back to 0x01 celp from 0x04 pcm\n");
            }
            dspMode = kCelpMode;
        } else {
            int raw = ((high << 8) | low) - 0x8000;
            int scaled = (raw * volume_q8_) >> 10;
            scaled = std::max(-32768, std::min(32767, scaled));
            emitSample8k(static_cast<int16_t>(scaled));
        }
        return;
    }

    // CELP frame data packets (Package 0..5 -> high < 0x60)
    if (high < 0x60) {
        if (dspMode != kWordMode && dspMode != kCelpMode && dspMode != kDiphoneMode) {
            if (g_log_verbose) {
                std::printf("[DSP] got celp data %02x %02x in unexpected %02x mode\n",
                            high, low, dspMode);
            }
            return;
        }
        ++data_cnt_;
        int pkg_id = high >> 4;
        if (packet_off_ >= 15) {
            if (g_log_info) std::printf("[DSP] oops packet_off_ out of bound\n");
            packet_off_ = 0;
        }
        if (pkg_id < 3) {
            packet_off_ = pkg_id;
            packets_[packet_off_++] = static_cast<uint16_t>((high << 8) | low);
        } else if (pkg_id < 6 && ((packet_off_ % 3) + 3) == pkg_id) {
            packets_[packet_off_++] = static_cast<uint16_t>((high << 8) | low);
            if (packet_off_ == 15) {
                packet_off_ = 0;
                unpackAndDecodeFrame();
            }
        }
        return;
    }

    // Volume control: 0x9000..0x93FF (Q8 gain 0.0x .. ~4.0x)
    if (high >= 0x90 && high <= 0x93) {
        volume_q8_ = ((high & 0x03) << 8) | low;
        if (g_log_verbose) {
            std::printf("[DSP] volume set to 0x%03x (%.2fx)\n", volume_q8_, volume_q8_ / 256.0);
        }
        return;
    }

    // Mode switch: 0xA0xx
    if (high == 0xA0) {
        if (g_log_verbose) {
            std::printf("[DSP] got dsp mode change cmd %02x %02x\n", high, low);
        }
        if (low != kCelpMode && low != kWordMode && low != kDiphoneMode &&
            low != kPcmMode && low != kNoneMode) {
            if (g_log_info) std::printf("[DSP] oops invalid dspMode %02x\n", low);
            reset();
            return;
        }
        if (g_log_info && dspMode != low) {
            std::printf("[DSP] dsp mode changed from %02x to %02x\n", dspMode, low);
        }
        dspMode = low;
        data_cnt_ = 0;
        packet_off_ = 0;
        return;
    }

    // TTS WORD_MODE syllable parameters: 0xC1 / 0xC2 / 0xC3 / 0xC4 / 0xC8
    if ((high & 0xF0) == 0xC0) {
        if (g_log_verbose) {
            std::printf("[DSP] got DSP_CMD %02x %02x\n", high, low);
        }
        if (dspMode != kWordMode && dspMode != kDiphoneMode) return;

        if (high == 0xC2) {
            cut_head_ = low;
            data_cnt_ = 0;
        } else if (high == 0xC4) {
            cut_tail_ = low;
            data_cnt_ = 0;
        } else if (high == 0xC8) {
            cut_frames_ = low;
            data_cnt_ = 0;
        } else if (high == 0xC1 || high == 0xC3) {
            if (data_cnt_ % 15 != 0) {
                if (g_log_info) {
                    std::printf("[DSP] oops, data_cnt=%d while got %02x, mod 15 != 0, reset\n",
                                data_cnt_, high);
                }
                resetSyllableState();
                return;
            }
            int num_frames = data_cnt_ / 15;
            int end = (cut_tail_ == 0) ? (240 * num_frames) : (cut_tail_ + 240 * (num_frames - 1));
            int buf_sz = static_cast<int>(word_buf_.size());
            if (end > buf_sz) {
                if (g_log_info) {
                    std::printf("[DSP] oops, end=%d buf_size=%d, clamping\n", end, buf_sz);
                }
                end = buf_sz;
            }
            int start = std::max(0, std::min(cut_head_, end));

            if (callback) {
                for (int i = start; i < end; ++i) {
                    int16_t s = word_buf_[i];
                    callback(reinterpret_cast<unsigned char*>(&s), sizeof(s));
                }
            }
            // Full per-syllable reset (LTP + synthesis IIR state + LSP history + pulse decay)
            resetSyllableState();
        } else {
            if (g_log_info) {
                std::printf("[DSP] oops, unhandled tts param cmd, high=%02x low=%02x\n", high, low);
            }
        }
        return;
    }

    // 0xE000: CMD_DELAY / flush
    if (high == 0xE0) {
        flush();
        return;
    }

    // 0xF000 / 0xFFFF: SPEECH_END / stream end
    if (high == 0xF0 || (high == 0xFF && low == 0xFF)) {
        resetSyllableState();
        return;
    }
}

void Dsp::unpackAndDecodeFrame() {
    uint8_t frame[18];
    const uint16_t* p = packets_;

    bool short_frame = !dual_mode_ && ((p[0] & 0x0800) != 0);
    uint8_t mode_bits = dual_mode_ ? static_cast<uint8_t>((p[3] >> 8) & 0x0F) : 0x0F;

    frame[0] = static_cast<uint8_t>((mode_bits << 4) | (p[0] & 0x0F));
    frame[1] = static_cast<uint8_t>(((p[0] >> 4) & 0x0F) | (((p[0] >> 8) & 0x07) << 4) | ((p[1] & 0x01) << 7));
    frame[2] = static_cast<uint8_t>(((p[1] >> 1) & 0x7F) | (((p[1] >> 8) & 0x01) << 7));
    frame[3] = static_cast<uint8_t>(((p[1] >> 9) & 0x03) | ((p[2] & 0x3F) << 2));
    frame[4] = static_cast<uint8_t>(((p[2] >> 6) & 0x03) | (((p[2] >> 8) & 0x0F) << 2) | ((p[3] & 0x03) << 6));
    frame[5] = static_cast<uint8_t>(((p[3] >> 2) & 0x3F) | ((p[4] & 0x03) << 6));
    frame[6] = static_cast<uint8_t>(((p[4] >> 2) & 0x3F) | (((p[4] >> 8) & 0x03) << 6));
    frame[7] = static_cast<uint8_t>(p[5] & 0xFF);
    frame[8] = static_cast<uint8_t>(((p[5] >> 8) & 0x03) | ((p[6] & 0x1F) << 2) | ((p[7] & 0x01) << 7));
    frame[9] = static_cast<uint8_t>(((p[7] >> 1) & 0x7F) | ((p[7] >> 1) & 0x80));
    frame[10] = static_cast<uint8_t>(((p[7] >> 9) & 0x01) | ((p[8] & 0x7F) << 1));
    frame[11] = static_cast<uint8_t>(((p[8] >> 7) & 0x01) | (((p[8] >> 8) & 0x03) << 1) | ((p[9] & 0x1F) << 3));
    frame[12] = static_cast<uint8_t>(((p[9] >> 5) & 0x07) | ((p[10] & 0x1F) << 3));
    frame[13] = static_cast<uint8_t>(((p[10] >> 5) & 0x07) | (((p[10] >> 8) & 0x03) << 3) | ((p[11] & 0x07) << 5));
    frame[14] = static_cast<uint8_t>(((p[11] >> 3) & 0x1F) | (((p[11] >> 8) & 0x03) << 5) | ((p[12] & 0x01) << 7));
    frame[15] = static_cast<uint8_t>(((p[12] >> 1) & 0x0F) | ((p[13] & 0x0F) << 4));
    frame[16] = static_cast<uint8_t>(((p[13] >> 4) & 0x0F) | (((p[13] >> 8) & 0x03) << 4) | ((p[14] & 0x03) << 6));
    frame[17] = static_cast<uint8_t>(((p[14] >> 2) & 0x3F) | (((p[14] >> 8) & 0x03) << 6));

    decodeCelpFrame(frame, mode_bits, short_frame);
}

void Dsp::decodeCelpFrame(const uint8_t frame[18], uint8_t mode_bits, bool short_frame) {
    subframe_size_ = short_frame ? 40 : 60;

    BitReader br(frame);

    // Unpack 10 scalar-quantized LSP indices (bits 4..7 of byte 0 sit between LSP1 bit 0 and LSP1 bits 1..3)
    int lsp_idx[kOrder];
    lsp_idx[0] = br.read(3);
    int lsp1_lo = br.read(1);
    (void)br.read(4);  // 4-bit header nibble (mode_bits already extracted)
    lsp_idx[1] = lsp1_lo | (br.read(3) << 1);
    lsp_idx[2] = br.read(4);
    lsp_idx[3] = br.read(4);
    lsp_idx[4] = br.read(4);
    for (int i = 5; i < kOrder; ++i) {
        lsp_idx[i] = br.read(3);
    }

    for (int i = 0; i < kOrder; ++i) {
        curr_lsp_hz_[i] = kLspFreqHz[kLspBandOffset[i] + lsp_idx[i]];
    }
    if (first_lsp_frame_) {
        for (int i = 0; i < kOrder; ++i) {
            prev_lsp_hz_[i] = curr_lsp_hz_[i];
        }
        first_lsp_frame_ = false;
    }

    // Decode 4 subframes
    for (int sf = 0; sf < 4; ++sf) {
        bool two_pulse = !dual_mode_ || (((mode_bits >> (3 - sf)) & 1) != 0);
        int pitch_idx = br.read((sf & 1) == 0 ? 8 : 5);
        int fixpos = br.read(9);

        if (two_pulse) {
            int grid = br.read(1);
            int pg_idx = br.read(4);
            int cg_idx = br.read(4);
            int sign1 = br.read(1);
            int sign0 = br.read(1);
            synthesizeSubframe(sf, pitch_idx, fixpos, grid, pg_idx, cg_idx, sign0, sign1, true);
        } else {
            (void)br.read(1);        // unused in stochastic mode
            int pg_idx = br.read(4);
            int cg_signed = br.read(5);
            (void)br.read(1);        // unused in stochastic mode
            int cg_idx = cg_signed & 0x0F;
            int cg_sign = (cg_signed >> 4) & 1;
            synthesizeSubframe(sf, pitch_idx, fixpos, 0, pg_idx, cg_idx, cg_sign, cg_sign, false);
        }
    }

    for (int i = 0; i < kOrder; ++i) {
        prev_lsp_hz_[i] = curr_lsp_hz_[i];
    }
}

void Dsp::decodePitch(int pitch_idx, int sf) {
    int p = pitch_idx;
    if ((sf & 1) == 0) {
        prev_pitch_idx_ = p;
    } else {
        p += prev_pitch_idx_ - 16;
    }
    if (p < 0) p = 0;
    if (p <= 191) {
        pitch_period_ = p / 3 + 13;
        pitch_frac_ = p % 3;
    } else {
        pitch_period_ = p - 115;
        pitch_frac_ = 0;
    }
    if (pitch_period_ < 13) pitch_period_ = 13;
    if (pitch_period_ > kLtpHistorySize - 1) pitch_period_ = kLtpHistorySize - 1;
}

double Dsp::samplePitch(int pos, int frac, int s, int pcount) {
    if (frac == 0) {
        if (pcount < 0) {
            pitch_buf_[s] = pitch_buf_[-pcount - 1];
        } else {
            pitch_buf_[s] = ltp_hist_[pos];
        }
    } else {
        const double* filt = tables().frac_sinc[frac - 1];
        int fpos = pos + 4;
        if (fpos >= kLtpHistorySize) fpos -= kLtpHistorySize;
        int fcount = pcount + 5;
        double acc = 0.0;
        for (int i = 0; i < 8; ++i) {
            --fcount;
            if (fcount < 0) {
                acc += pitch_buf_[-fcount - 1] * filt[i];
            } else {
                acc += ltp_hist_[fpos] * filt[i];
                fpos = (fpos > 0) ? (fpos - 1) : (kLtpHistorySize - 1);
            }
        }
        pitch_buf_[s] = acc;
    }
    return pitch_buf_[s];
}

void Dsp::interpolateLsp(int sf, double out_cos[kOrder]) const {
    double w_new = (sf + 1) * 0.25;
    double w_old = 1.0 - w_new;
    for (int i = 0; i < kOrder; ++i) {
        double freq_hz = w_old * prev_lsp_hz_[i] + w_new * curr_lsp_hz_[i];
        double omega = kPi * freq_hz / 4000.0;
        out_cos[i] = std::cos(omega);
    }
}

void Dsp::lspToLpc(const double lsp_cos[kOrder], double lpc[kOrder + 1]) {
    // Expand P(z) = (1 + z^-1) * prod_{k=0..4}(1 - 2*c[2k]*z^-1 + z^-2)
    // and    Q(z) = (1 - z^-1) * prod_{k=0..4}(1 - 2*c[2k+1]*z^-1 + z^-2)
    double p_poly[kOrder + 2] = {};
    double q_poly[kOrder + 2] = {};
    p_poly[0] = 1.0;
    q_poly[0] = 1.0;
    int deg = 0;

    for (int k = 0; k < 5; ++k) {
        double cp = -2.0 * lsp_cos[2 * k];
        double cq = -2.0 * lsp_cos[2 * k + 1];
        for (int j = deg + 2; j >= 2; --j) {
            p_poly[j] += cp * p_poly[j - 1] + p_poly[j - 2];
            q_poly[j] += cq * q_poly[j - 1] + q_poly[j - 2];
        }
        p_poly[1] += cp * p_poly[0];
        q_poly[1] += cq * q_poly[0];
        deg += 2;
    }

    // Multiply P(z) by (1 + z^-1) and Q(z) by (1 - z^-1), then average A(z) = 0.5 * (P + Q)
    lpc[0] = 1.0;
    for (int i = 1; i <= kOrder; ++i) {
        double pi = p_poly[i] + p_poly[i - 1];
        double qi = q_poly[i] - q_poly[i - 1];
        lpc[i] = 0.5 * (pi + qi);
    }
}

void Dsp::synthesizeSubframe(int sf, int pitch_idx, int fixpos, int grid,
                             int pitch_gidx, int inno_gidx, int sign0, int sign1,
                             bool two_pulse_mode) {
    decodePitch(pitch_idx, sf);

    double lsp_cos[kOrder];
    double lpc[kOrder + 1];
    interpolateLsp(sf, lsp_cos);
    lspToLpc(lsp_cos, lpc);

    double pg = pitchGain(pitch_gidx);
    double cg = dual_mode_ ? tables().dual_cgain[inno_gidx & 0x0F]
                           : (kLegacyCGain[inno_gidx & 0x0F] * 4.0);

    // Prepare innovation excitation for the subframe
    double exc_inno[kMaxSubframeSize] = {};
    if (two_pulse_mode) {
        if (fixpos < 0 || fixpos >= 435) {
            if (g_log_info) std::printf("[DSP] oops fixpos %d out of bound\n", fixpos);
        }
        int p0 = 0, p1 = 1;
        decodePulsePair(fixpos, p0, p1);
        int pos0 = p0 * 2 + (grid & 1);
        int pos1 = p1 * 2 + (grid & 1);

        double amp0 = sign0 ? -cg : cg;
        double amp1 = sign1 ? -cg : cg;
        double decay = dual_mode_ ? pulse_decay_ : 1.0;

        for (int idx = pos0; idx < subframe_size_; idx += pitch_period_) {
            exc_inno[idx] += amp0;
            amp0 *= decay;
        }
        for (int idx = pos1; idx < subframe_size_; idx += pitch_period_) {
            exc_inno[idx] += amp1;
            amp1 *= decay;
        }
    } else {
        // Dual-mode stochastic sparse ternary codebook excitation (9-bit index 0..511)
        const int8_t* cb = tables().stochastic_cb;
        int cb_base = (fixpos & 0x1FF) * 2;
        double amp = sign0 ? -cg : cg;
        for (int i = 0; i < subframe_size_; ++i) {
            exc_inno[i] = cb[cb_base + i] * amp;
        }
        pulse_decay_ = pg;
    }

    int pit_pos = ltp_read_base_ + pitch_period_ - 1;
    if (pit_pos >= kLtpHistorySize) pit_pos -= kLtpHistorySize;
    int ex_pos = ltp_write_pos_;
    int pcount = pitch_period_ - 1;

    // Q8 volume multiplier (0x3FF -> ~4.0x matching wqxsim.exe << 2; 0x100 -> 1.0x)
    double vol_scale = volume_q8_ / 256.0;

    for (int i = 0; i < subframe_size_; ++i, --pcount) {
        double pitch_sample = samplePitch(pit_pos, pitch_frac_, i, pcount);
        double exc = pg * pitch_sample + exc_inno[i];
        ltp_hist_[ex_pos] = exc;

        // 10th-order all-pole synthesis filter: y[n] = exc[n] - sum_{k=1..10} a[k] * y[n-k]
        double sum = 0.0;
        for (int k = kOrder; k > 0; --k) {
            sum += lpc_state_[k] * lpc[k];
            lpc_state_[k] = lpc_state_[k - 1];
        }
        double y = exc - sum;
        lpc_state_[1] = y;

        int out = static_cast<int>(std::lround(y * vol_scale));
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        emitSample8k(static_cast<int16_t>(out));

        ex_pos = (ex_pos > 0) ? (ex_pos - 1) : (kLtpHistorySize - 1);
        pit_pos = (pit_pos > 0) ? (pit_pos - 1) : (kLtpHistorySize - 1);
    }

    // Update LTP ring buffer pointers
    ltp_write_pos_ -= subframe_size_;
    if (ltp_write_pos_ < 0) ltp_write_pos_ += kLtpHistorySize;
    ltp_read_base_ = ltp_write_pos_ + 1;
    if (ltp_read_base_ >= kLtpHistorySize) ltp_read_base_ -= kLtpHistorySize;
}

void Dsp::emitSample8k(int16_t sample) {
    if (dspMode != kWordMode && dspMode != kDiphoneMode) {
        if (callback) {
            callback(reinterpret_cast<unsigned char*>(&sample), sizeof(sample));
        }
    } else {
        word_buf_.push_back(sample);
    }
}
