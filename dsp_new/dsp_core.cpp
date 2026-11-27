// Clean-room Sunplus SPDS104A S480 / CELP & PCM audio synthesis core.
// Pure signal-processing module with zero emulator or I/O dependencies.

#include "dsp_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Bit allocation per LSP band (3,4,4,4,4,3,3,3,3,3 -> 112 levels total)
constexpr int kLspBandOffset[DspCore::kOrder] = {0, 8, 24, 40, 56, 72, 80, 88, 96, 104};

// Sunplus SPDS104A / S480 scalar LSP quantization grid in Hz (8 kHz sample rate, Nyquist = 4000 Hz).
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

// Legacy SPDS104A (PC1000/NC1020) 4-bit innovation gain table.
constexpr double kLegacyCGain[16] = {
    0.0, 1.0, 3.0, 13.0, 35.0, 64.0, 98.0, 136.0,
    178.0, 224.0, 278.0, 340.0, 418.0, 520.0, 660.0, 870.0,
};

// Generated tables (computed once from math formulas).
struct GeneratedTables {
    double frac_sinc[2][8];      // 8-tap Hamming-windowed sinc for 1/3 and 2/3 sample delay
    int8_t stochastic_cb[1084];  // Ternary sparse {-1, 0, +1} codebook (~75% zeros)

    GeneratedTables() {
        // 8-tap Hamming-windowed sinc fractional-delay filters for F=1 (1/3) and F=2 (2/3).
        // sample_pitch walks backwards from pos+4, so k=4 is lag T and k=3 is lag T+1.
        for (int f = 0; f < 2; ++f) {
            double delay = (f + 1) / 3.0;
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                double t = (k - 4) + delay;
                double s = (std::abs(t) < 1e-9) ? 1.0 : std::sin(kPi * t) / (kPi * t);
                double w = 0.54 + 0.46 * std::cos(kPi * t / 4.0);
                frac_sinc[f][k] = s * w;
                sum += frac_sinc[f][k];
            }
            for (int k = 0; k < 8; ++k) {
                frac_sinc[f][k] /= sum;
            }
        }

        // Deterministic sparse ternary stochastic excitation (~75% 0, ~12.5% +1, ~12.5% -1)
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

inline double pitch_gain(int gidx) {
    return (gidx & 0x0F) * 0.075;
}

// Decode C(30, 2) = 435 lexicographical pulse pair index (0 <= p0 < p1 < 30)
inline void decode_pulse_pair(int index, int& p0, int& p1) {
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

DspCore::DspCore()
    : packet_off_(0),
      dual_mode_(false),
      volume_q8_(0x3FF),
      log_level_(1),
      first_lsp_frame_(true),
      ltp_write_pos_(kLtpHistorySize - 1),
      ltp_read_base_(0),
      prev_pitch_idx_(0),
      pitch_period_(13),
      pitch_frac_(0),
      subframe_size_(60),
      pulse_decay_(1.0) {
    std::memset(packets_, 0, sizeof(packets_));
    reset_decoder();
}

void DspCore::set_dual_mode(bool enable) {
    dual_mode_ = enable;
}

void DspCore::set_volume_q8(int volume_q8) {
    volume_q8_ = std::max(0, std::min(0x3FF, volume_q8));
}

void DspCore::set_log_level(int level) {
    log_level_ = level;
}

void DspCore::reset() {
    volume_q8_ = 0x3FF;
    reset_decoder();
}

void DspCore::reset_decoder() {
    packet_off_ = 0;
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
    subframe_size_ = 60;
    pulse_decay_ = 1.0;
}

void DspCore::clear_packet() {
    packet_off_ = 0;
}

int16_t DspCore::decode_pcm_word(uint16_t word) const {
    int raw = static_cast<int>(word) - 0x8000;
    int scaled = (raw * volume_q8_) >> 10;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    return static_cast<int16_t>(scaled);
}

bool DspCore::feed_celp_word(uint16_t word, std::vector<int16_t>& out) {
    int pkg_id = (word >> 12) & 0x0F;
    if (packet_off_ >= kWordsPerFrame) {
        if (log_level_ >= 1) std::printf("[DSP] oops packet_off_ out of bound\n");
        packet_off_ = 0;
    }
    if (pkg_id < 3) {
        packet_off_ = pkg_id;
        packets_[packet_off_++] = word;
        return false;
    }
    if (packet_off_ >= 3 && pkg_id < 6 && ((packet_off_ % 3) + 3) == pkg_id) {
        packets_[packet_off_++] = word;
        if (packet_off_ == kWordsPerFrame) {
            packet_off_ = 0;
            unpack_and_decode_frame(out);
            return true;
        }
    }
    return false;
}

void DspCore::unpack_and_decode_frame(std::vector<int16_t>& out) {
    uint8_t frame[18];
    const uint16_t* p = packets_;

    bool short_frame = !dual_mode_ && ((p[0] & 0x0800) != 0);
    uint8_t mode_bits = dual_mode_ ? static_cast<uint8_t>((p[3] >> 8) & 0x0F) : 0x0F;

    frame[0] = static_cast<uint8_t>((mode_bits << 4) | (p[0] & 0x0F));
    frame[1] = static_cast<uint8_t>(((p[0] >> 4) & 0x0F) | (((p[0] >> 8) & 0x07) << 4) | ((p[1] & 0x01) << 7));
    frame[2] = static_cast<uint8_t>((p[1] >> 1) & 0xFF);
    frame[3] = static_cast<uint8_t>(((p[1] >> 9) & 0x03) | ((p[2] & 0x3F) << 2));
    frame[4] = static_cast<uint8_t>(((p[2] >> 6) & 0x03) | (((p[2] >> 8) & 0x0F) << 2) | ((p[3] & 0x03) << 6));
    frame[5] = static_cast<uint8_t>(((p[3] >> 2) & 0x3F) | ((p[4] & 0x03) << 6));
    frame[6] = static_cast<uint8_t>(((p[4] >> 2) & 0x3F) | (((p[4] >> 8) & 0x03) << 6));
    frame[7] = static_cast<uint8_t>(p[5] & 0xFF);
    frame[8] = static_cast<uint8_t>(((p[5] >> 8) & 0x03) | ((p[6] & 0x1F) << 2) | ((p[7] & 0x01) << 7));
    frame[9] = static_cast<uint8_t>((p[7] >> 1) & 0xFF);
    frame[10] = static_cast<uint8_t>(((p[7] >> 9) & 0x01) | ((p[8] & 0x7F) << 1));
    frame[11] = static_cast<uint8_t>(((p[8] >> 7) & 0x01) | (((p[8] >> 8) & 0x03) << 1) | ((p[9] & 0x1F) << 3));
    frame[12] = static_cast<uint8_t>(((p[9] >> 5) & 0x07) | ((p[10] & 0x1F) << 3));
    frame[13] = static_cast<uint8_t>(((p[10] >> 5) & 0x07) | (((p[10] >> 8) & 0x03) << 3) | ((p[11] & 0x07) << 5));
    frame[14] = static_cast<uint8_t>(((p[11] >> 3) & 0x1F) | (((p[11] >> 8) & 0x03) << 5) | ((p[12] & 0x01) << 7));
    frame[15] = static_cast<uint8_t>(((p[12] >> 1) & 0x0F) | ((p[13] & 0x0F) << 4));
    frame[16] = static_cast<uint8_t>(((p[13] >> 4) & 0x0F) | (((p[13] >> 8) & 0x03) << 4) | ((p[14] & 0x03) << 6));
    frame[17] = static_cast<uint8_t>(((p[14] >> 2) & 0x3F) | (((p[14] >> 8) & 0x03) << 6));

    decode_celp_frame(frame, mode_bits, short_frame, out);
}

void DspCore::decode_celp_frame(const uint8_t frame[18], uint8_t mode_bits,
                                bool short_frame, std::vector<int16_t>& out) {
    subframe_size_ = short_frame ? 40 : 60;
    out.reserve(out.size() + subframe_size_ * 4);

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
            synthesize_subframe(sf, pitch_idx, fixpos, grid, pg_idx, cg_idx, sign0, sign1, true, out);
        } else {
            (void)br.read(1);
            int pg_idx = br.read(4);
            int cg_signed = br.read(5);
            (void)br.read(1);
            int cg_idx = cg_signed & 0x0F;
            int cg_sign = (cg_signed >> 4) & 1;
            synthesize_subframe(sf, pitch_idx, fixpos, 0, pg_idx, cg_idx, cg_sign, cg_sign, false, out);
        }
    }

    for (int i = 0; i < kOrder; ++i) {
        prev_lsp_hz_[i] = curr_lsp_hz_[i];
    }
}

void DspCore::decode_pitch(int pitch_idx, int sf) {
    int p = pitch_idx;
    if ((sf & 1) == 0) {
        prev_pitch_idx_ = p;
    } else {
        p += prev_pitch_idx_ - 16;
    }
    if (p < 0) p = 0;
    if (dual_mode_) {
        // NC2000/NC2600 dual-mode: 18..82 (1/3-sample) for p<=194, 83..143 for p>=195
        if (p <= 194) {
            pitch_period_ = p / 3 + 18;
            pitch_frac_ = p % 3;
        } else {
            pitch_period_ = p - 112;
            pitch_frac_ = 0;
        }
        if (pitch_period_ < 18) pitch_period_ = 18;
    } else {
        // PC1000/NC1020 single-mode: 13..76 (1/3-sample) for p<=191, 77..140 for p>=192
        if (p <= 191) {
            pitch_period_ = p / 3 + 13;
            pitch_frac_ = p % 3;
        } else {
            pitch_period_ = p - 115;
            pitch_frac_ = 0;
        }
        if (pitch_period_ < 13) pitch_period_ = 13;
    }
    if (pitch_period_ > kLtpHistorySize - 1) pitch_period_ = kLtpHistorySize - 1;
}

double DspCore::sample_pitch(int pos, int frac, int s, int pcount) {
    if (frac == 0) {
        pitch_buf_[s] = (pcount < 0) ? pitch_buf_[-pcount - 1] : ltp_hist_[pos];
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

void DspCore::interpolate_lsp(int sf, double out_cos[kOrder]) const {
    double w_new = (sf + 1) * 0.25;
    double w_old = 1.0 - w_new;
    for (int i = 0; i < kOrder; ++i) {
        double freq_hz = w_old * prev_lsp_hz_[i] + w_new * curr_lsp_hz_[i];
        out_cos[i] = std::cos(kPi * freq_hz / 4000.0);
    }
}

void DspCore::lsp_to_lpc(const double lsp_cos[kOrder], double lpc[kOrder + 1]) {
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

    lpc[0] = 1.0;
    for (int i = 1; i <= kOrder; ++i) {
        double pi = p_poly[i] + p_poly[i - 1];
        double qi = q_poly[i] - q_poly[i - 1];
        lpc[i] = 0.5 * (pi + qi);
    }
}

void DspCore::synthesize_subframe(int sf, int pitch_idx, int fixpos, int grid,
                                  int pitch_gidx, int inno_gidx, int sign0, int sign1,
                                  bool two_pulse_mode, std::vector<int16_t>& out) {
    decode_pitch(pitch_idx, sf);

    double lsp_cos[kOrder];
    double lpc[kOrder + 1];
    interpolate_lsp(sf, lsp_cos);
    lsp_to_lpc(lsp_cos, lpc);

    double pg = pitch_gain(pitch_gidx);
    double cg = kLegacyCGain[inno_gidx & 0x0F] * 4.0;

    double exc_inno[kMaxSubframeSize] = {};
    if (two_pulse_mode) {
        if (fixpos < 0 || fixpos >= 435) {
            if (log_level_ >= 1) std::printf("[DSP] oops fixpos %d out of bound\n", fixpos);
        }
        int p0 = 0, p1 = 1;
        decode_pulse_pair(fixpos, p0, p1);
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
        const int8_t* cb = tables().stochastic_cb;
        int cb_base = (fixpos & 0x1FF) * 2;
        double amp = sign0 ? -cg : cg;
        for (int i = 0; i < subframe_size_; ++i) {
            exc_inno[i] = cb[cb_base + i] * amp;
        }
    }
    pulse_decay_ = pg;

    int pit_pos = ltp_read_base_ + pitch_period_ - 1;
    if (pit_pos >= kLtpHistorySize) pit_pos -= kLtpHistorySize;
    int ex_pos = ltp_write_pos_;
    int pcount = pitch_period_ - 1;

    // Q8 volume gain: 0x93FF -> ~4.0x (kLegacyCGain * 4 * 4 = * 16, matching legacy dsp)
    double vol_scale = volume_q8_ / 256.0;

    for (int i = 0; i < subframe_size_; ++i, --pcount) {
        double pitch_sample = sample_pitch(pit_pos, pitch_frac_, i, pcount);
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

        int sample = static_cast<int>(std::lround(y * vol_scale));
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        out.push_back(static_cast<int16_t>(sample));

        ex_pos = (ex_pos > 0) ? (ex_pos - 1) : (kLtpHistorySize - 1);
        pit_pos = (pit_pos > 0) ? (pit_pos - 1) : (kLtpHistorySize - 1);
    }

    ltp_write_pos_ -= subframe_size_;
    if (ltp_write_pos_ < 0) ltp_write_pos_ += kLtpHistorySize;
    ltp_read_base_ = ltp_write_pos_ + 1;
    if (ltp_read_base_ >= kLtpHistorySize) ltp_read_base_ -= kLtpHistorySize;
}
