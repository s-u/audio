/* Audio device for R using PulseAudio library
   based on PortAudio library code: Copyright(c) 2008 Simon Urbanek

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   * The above copyright notice and this permission notice shall be
     included in all copies or substantial portions of the Software.
 
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT 0T LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND ON
   INFRINGEMENT. 
   IN 0 EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
   ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
   The text above constitutes the entire license; however, the
   PortAudio community also makes the following non-binding requests:

   * Any person wishing to distribute modifications to the Software is
     requested to send the modifications to the original developer so
     that they can be incorporated into the canonical version. It is
     also requested that these non-binding requests be included along
     with the license above.

 */

#include "driver.h"
#include "riff.h"
#if HAS_PULSE

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#define BUFSIZE 2048

typedef struct pulse_instance {
  /* the following entries must be present since pulse_instance_t inherits from audio_instance_t */
  audio_driver_t *driver;  /* must point to the driver that created this */
  int kind;                /* must be either AI_PLAYER or AI_RECORDER */
  SEXP source;
  /* custom entries */
  pa_sample_spec ss;
  pa_simple *s;
  int stereo;
  int done;
  unsigned int length;
} pulse_instance_t;
  

static audio_instance_t *pulseaudio_create_player(SEXP source, float rate, int flags) {
  pulse_instance_t *ap = (pulse_instance_t*) calloc(sizeof(pulse_instance_t), 1);
  ap->source = source;
  unsigned int size = LENGTH(ap->source) * 2;
  R_PreserveObject(ap->source);
  ap->ss.format = PA_SAMPLE_S16LE;
  ap->ss.rate = (uint32_t)rate;
  ap->done = 0;
  ap->length = LENGTH(source);
  ap->stereo = 0;
  { /* if the source is a matrix with 2 rows then we'll use stereo */
    SEXP dim = Rf_getAttrib(source, R_DimSymbol);
    if (TYPEOF(dim) == INTSXP && LENGTH(dim) > 0 && INTEGER(dim)[0] == 2)
      ap->stereo = 1;
  }
  if (ap->stereo == 1) {
    ap->length /= 2;
    ap->ss.channels = 2;
  } else {
    ap->ss.channels = 1;
  }
  return (audio_instance_t *)ap; /* pulse_instance_t is a superset of audio_instance_t */
}

static int pulseaudio_start(void *usr) {
  pulse_instance_t *ap = (pulse_instance_t*) usr;
  char msg[1024];
  int i, error;
  size_t r;
  unsigned int bits = 16, bps = 2, chs = 1, rate;
  unsigned int size = LENGTH(ap->source);
  ap->s = pa_simple_new(NULL, "R", PA_STREAM_PLAYBACK, NULL,
                "R audio playback", &ap->ss, NULL, NULL, NULL);
  if(!ap->s) Rf_error("cannot initialize PulseAudio system");
  if(ap->stereo == 1) chs = 2;
  rate = ap->ss.rate; /* already unsigned int, no conversion needed */
  SEXP dim =  Rf_getAttrib(ap->source, Rf_install("bits"));
  if (TYPEOF(dim) == INTSXP || TYPEOF(dim) == REALSXP) {
    i = Rf_asInteger(dim);
    if (i == 8) {
      size /= 2;
      bits = 8;
      bps = 1;
    } else if (i == 32) {
      size *= 2;
      bits = 32;
      bps = 4;
    }
  }
  bps *= chs;
  riff_header_t rh = { "RIFF", size + 36, "WAVE" };
  wav_fmt_t fmt = { "fmt ", 16, 1, chs, rate, rate * bps, bps, bits };
  riff_chunk_t rc = { "data", size };
  /* send header to Pulse Audio */
  r = sizeof(riff_header_t);
  if (pa_simple_write(ap->s, &rh, r, &error) < 0) {
    Rf_error("PulseAudio write error");
  }
  r = sizeof(wav_fmt_t);
  if (pa_simple_write(ap->s, &fmt, r, &error) < 0) {
    Rf_error("PulseAudio write error");
  }
  r = sizeof(riff_chunk_t);
  if (pa_simple_write(ap->s, &rc, r, &error) < 0) {
    Rf_error("PulseAudio write error");
  }
  {
    if (bits == 8) {
      double *d = REAL(ap->source);
      short int buf[BUFSIZE];
      int i = 0, j = LENGTH(ap->source), k = 0;
      while (i < j) {
        R_CheckUserInterrupt();
        buf[k++] = (signed char) (d[i++] * 127.0);
        if (k == BUFSIZE) {
          if (pa_simple_write(ap->s, buf, sizeof(*buf) * k, &error) < 0) {
            Rf_error("PulseAudio write error");
          }
          k = 0;
        }
      }
      if (pa_simple_write(ap->s, buf, sizeof(*buf) * k, &error) < 0) {
        Rf_error("PulseAudio write error");
      }
    } else if (bits == 16) {
      double *d = REAL(ap->source);
      short int buf[BUFSIZE];
      int i = 0, j = LENGTH(ap->source), k = 0;
      while (i < j) {
        R_CheckUserInterrupt();
        buf[k++] = (short int) (d[i++] * 32767.0);
        if (k == BUFSIZE) {
          if (pa_simple_write(ap->s, buf, sizeof(*buf) * k, &error) < 0) {
            Rf_error("PulseAudio write error");
          }
          k = 0;
        }
      }
      if (pa_simple_write(ap->s, buf, sizeof(*buf) * k, &error) < 0) {
        Rf_error("PulseAudio write error");
      }
    } else {
      double *d = REAL(ap->source);
      int buf[BUFSIZE];
      int i = 0, j = LENGTH(ap->source), k = 0;
      while (i < j) {
        R_CheckUserInterrupt();
        buf[k++] = (int) (d[i++] * 2147483647.0);
        if (k == BUFSIZE) {
          if (pa_simple_write(ap->s, buf, sizeof(*buf) * k, &error) < 0) {
            Rf_error("PulseAudio write error");
          }
          k = 0;
        }
      }
      if (pa_simple_write(ap->s, buf, sizeof(*buf) * k, &error) < 0) {
        Rf_error("PulseAudio write error");
      }
    }
  }
  if (pa_simple_drain(ap->s, &error) < 0) Rf_error("PulseAudio write error");
  return 1;
}

static int pulseaudio_pause(void *usr) {
  R_ShowMessage("not supported yet");
  return 1;
}

static int pulseaudio_resume(void *usr) {
  R_ShowMessage("not supported yet");
  return 1;
}

static int pulseaudio_rewind(void *usr) {
  R_ShowMessage("not supported yet");
  return 1;
}

static int pulseaudio_wait(void *usr, double timeout) {
  R_ShowMessage("not supported yet");
  return 1;
}

static int pulseaudio_close(void *usr) {
  pulse_instance_t *p = (pulse_instance_t*) usr;
  if(p->s) pa_simple_flush(p->s, NULL);
  return 1;
}

static void pulseaudio_dispose(void *usr) {
  pulse_instance_t *p = (pulse_instance_t*) usr;
  if(p->s) pa_simple_free(p->s);
  free(usr);
}

/* define the audio driver */
audio_driver_t pulseaudio_audio_driver = {
  sizeof(audio_driver_t),

  "pulseaudio",
  "PulseAudio driver",
  "Copyright(c) 2020 Bryan Lewis",

  pulseaudio_create_player,
  0, /* recorder is currently unimplemented */
  pulseaudio_start,
  pulseaudio_pause,
  pulseaudio_resume,
  pulseaudio_rewind,
  pulseaudio_wait,
  pulseaudio_close,
  pulseaudio_dispose
};

#endif
