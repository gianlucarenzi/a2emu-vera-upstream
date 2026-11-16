/* a2run_audio.c -- a2run's audio capture (plan E9).
 *
 *   --wav FILE     the VERA PSG + PCM + YM2151 mix (vc_audio_take) as a 16-bit stereo WAV
 *                  at VERA's native rate, 25 MHz / 512 = 48,828.125 Hz (header:
 *                  48828).  The VERA makes a sample every 512 of its clocks as
 *                  CPU time passes (veracore.c audio_clock), so register writes
 *                  take effect at the sample they happen in; this file only
 *                  collects them once per VERA frame.
 *   --psglog FILE  the 64 PSG register bytes ($1F9C0-$1F9FF) after every VERA
 *                  frame in which they changed: "<frame> <128 hex digits>\n"
 *                  (frame = a2run's count of completed frames, 1-based; frame 0
 *                  is the state at start).
 *   --psgvlog FILE the same, but sampled at each VSYNC before the CPU takes the
 *                  VSYNC IRQ: "<n> <128 hex>" with n = the VSYNC count, i.e. the
 *                  registers after the previous IRQ's work however long it ran (a
 *                  tick of the PSG music may outlast the 45 lines to the frame end).
 * Both are deterministic: the PSG model (incl. its noise LFSR) starts from reset
 * and only depends on the register writes and the frame count. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "veracore.h"
#include "a2run_audio.h"
#include "ym2151.h"

#define WAV_RATE      48828
#define PSG_REGS      0x1F9C0

static FILE *wav_f, *log_f, *vlog_f;
static uint8_t vlast[64];
static int vstarted;
static uint32_t wav_samples;
static uint8_t last_regs[64];
static int log_started;

static void le16(uint8_t *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void le32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

static void wav_header(FILE *f, uint32_t samples) {
	uint8_t h[44];
	uint32_t data = samples * 4;
	memcpy(h, "RIFF", 4); le32(h + 4, 36 + data); memcpy(h + 8, "WAVEfmt ", 8);
	le32(h + 16, 16); le16(h + 20, 1); le16(h + 22, 2); le32(h + 24, WAV_RATE);
	le32(h + 28, WAV_RATE * 4); le16(h + 32, 4); le16(h + 34, 16);
	memcpy(h + 36, "data", 4); le32(h + 40, data);
	fwrite(h, 1, 44, f);
}

int a2audio_open_wav(const char *path) {
	wav_f = fopen(path, "wb");
	if (!wav_f) return 0;
	wav_header(wav_f, 0);
	wav_samples = 0;
	ym_audio_on();
	return 1;
}

int a2audio_open_psglog(const char *path) {
	log_f = fopen(path, "w");
	return log_f != NULL;
}

static void log_regs(long frame) {
	const uint8_t *r = vc_vram() + PSG_REGS;
	if (log_started && !memcmp(r, last_regs, 64)) return;
	log_started = 1;
	memcpy(last_regs, r, 64);
	fprintf(log_f, "%ld ", frame);
	for (int i = 0; i < 64; i++) fprintf(log_f, "%02x", r[i]);
	fputc('\n', log_f);
}

int a2audio_open_psgvlog(const char *path) {
	vlog_f = fopen(path, "w");
	return vlog_f != NULL;
}

void a2audio_vsync(long n) {
	if (!vlog_f) return;
	const uint8_t *r = vc_vram() + PSG_REGS;
	if (vstarted && !memcmp(r, vlast, 64)) return;
	vstarted = 1;
	memcpy(vlast, r, 64);
	fprintf(vlog_f, "%ld ", n);
	for (int i = 0; i < 64; i++) fprintf(vlog_f, "%02x", r[i]);
	fputc('\n', vlog_f);
}

void a2audio_frame(long frame) {
	if (log_f) {
		if (!log_started) log_regs(0);
		log_regs(frame);
	}
	if (wav_f) {
		static int16_t buf[2 * 1024];
		int n;
		while ((n = vc_audio_take(buf, 1024)) > 0) {   /* what VERA time has produced */
			ym_mix(buf, n, 48828.125);                   /* + the YM2151 (E14), to this cycle (25 MHz / 512) */
			uint8_t out[4 * 1024];
			for (int i = 0; i < 2 * n; i++) le16(out + 2 * i, (uint16_t)buf[i]);
			fwrite(out, 1, 4 * (size_t)n, wav_f);
			wav_samples += n;
		}
	}
}

void a2audio_close(void) {
	if (wav_f) {
		fseek(wav_f, 0, SEEK_SET);
		wav_header(wav_f, wav_samples);
		fclose(wav_f);
		wav_f = NULL;
	}
	if (log_f) { fclose(log_f); log_f = NULL; }
	if (vlog_f) { fclose(vlog_f); vlog_f = NULL; }
}
