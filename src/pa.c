/* Audio device for R using PortAudio library
   Copyright(c) 2008 Simon Urbanek

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
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND ON
   INFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
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
#if HAS_PA

#include "portaudio.h"

#ifdef __WIN32__
#include <windows.h>
#else
#include <Rconfig.h> /* for HAVE_AQUA needed in wait */
#include <sys/select.h>
#include <R_ext/eventloop.h>
#ifndef unix
#define unix 1
#endif
#endif

#define kNumberOutputBuffers 2
/* #define USEFLOAT 1 */

#define BOOL int
#ifndef YES
#define YES 1
#define NO  0
#endif

typedef signed short int SInt16;

typedef struct play_info {
	/* the following entries must be present since play_info_t inherits from audio_instance_t */
	audio_driver_t *driver;  /* must point to the driver that created this */
	int kind;                /* must be either AI_PLAYER or AI_RECORDER */
	SEXP source;
	/* private entries */
	PaStream *stream;
	float sample_rate;
	BOOL stereo, loop, done;
	unsigned int position, length;
} play_info_t;
	
static int paPlayCallback(const void *inputBuffer, void *outputBuffer,
						  unsigned long framesPerBuffer,
						  const PaStreamCallbackTimeInfo* timeInfo,
						  PaStreamCallbackFlags statusFlags,
						  void *userData )
{
	play_info_t *ap = (play_info_t*)userData; 
	if (ap->done) return paAbort;
	/* Rprintf("paPlayCallback(in=%p, out=%p, fpb=%d, usr=%p)\n", inputBuffer, outputBuffer, (int) framesPerBuffer, userData);
	//Rprintf(" - (sample_rate=%f, stereo=%d, loop=%d, done=%d, pos=%d, len=%d)\n", ap->sample_rate, ap->stereo, ap->loop, ap->done, ap->position, ap->length); */
	if (ap->position == ap->length && ap->loop)
		ap->position = 0;
	unsigned int index = ap->position;
	unsigned int rem = ap->length - index;
	unsigned int spf = ap->stereo ? 2 : 1;
	if (rem > framesPerBuffer) rem = framesPerBuffer;
	/* printf("position=%d, length=%d, (LEN=%d), rem=%d, cap=%d, spf=%d\n", ap->position, ap->length, LENGTH(ap->source), rem, framesPerBuffer, spf); */
	index *= spf;
	/* there is a small caveat - if a zero-size buffer comes along it will stop the playback since rem will be forced to 0 - but then that should not happen ... */
	if (rem > 0) {
		unsigned int samples = rem * spf; // samples (i.e. SInt16s)
#ifdef USEFLOAT
		float *iBuf = (float*) outputBuffer;
		float *sentinel = iBuf + samples;
		/* printf(" iBuf = %p, sentinel = %p, diff = %d bytes\n", iBuf, sentinel, ((char*)sentinel) - ((char*)iBuf)); */
		if (TYPEOF(ap->source) == INTSXP) {
			int *iSrc = INTEGER(ap->source) + index;
			while (iBuf < sentinel)
				*(iBuf++) = ((float) *(iSrc++)) / 32768.0;
		} else if (TYPEOF(ap->source) == REALSXP) {
			double *iSrc = REAL(ap->source) + index;
			while (iBuf < sentinel)
				*(iBuf++) = (float) *(iSrc++);
			/* { int i = 0; while (i < framesPerBuffer) printf("%.2f ", ((float*) outputBuffer)[i++]); Rprintf("\n"); } */
		} /* FIXME: support functions as sources... */
#else
		SInt16 *iBuf = (SInt16*) outputBuffer;
		SInt16 *sentinel = iBuf + samples;
		if (TYPEOF(ap->source) == INTSXP) {
			int *iSrc = INTEGER(ap->source) + index;
			while (iBuf < sentinel)
				*(iBuf++) = (SInt16) *(iSrc++);
		} else if (TYPEOF(ap->source) == REALSXP) {
			double *iSrc = REAL(ap->source) + index;
			while (iBuf < sentinel)
				*(iBuf++) = (SInt16) (32767.0 * (*(iSrc++)));
		} /* FIXME: support functions as sources... */
#endif
		ap->position += rem;
	} else {
		/* printf(" rem ==0 -> stop queue\n"); */
		ap->done = YES;
		return paComplete;
	}
	return 0;
}

static audio_instance_t *portaudio_create_player(SEXP source, float rate, int flags) {
	PaError err = Pa_Initialize();
	if( err != paNoError ) Rf_error("cannot initialize audio system: %s\n", Pa_GetErrorText( err ) );
	play_info_t *ap = (play_info_t*) calloc(sizeof(play_info_t), 1);
	ap->source = source;
	R_PreserveObject(ap->source);
	ap->sample_rate = rate;
	ap->done = NO;
	ap->position = 0;
	ap->length = LENGTH(source);
	ap->stereo = NO;
	{ /* if the source is a matrix with 2 rows then we'll use stereo */
		SEXP dim = Rf_getAttrib(source, R_DimSymbol);
		if (TYPEOF(dim) == INTSXP && LENGTH(dim) > 0 && INTEGER(dim)[0] == 2)
			ap->stereo = YES;
	}
	ap->loop = (flags & APFLAG_LOOP) ? YES : NO;
	if (ap->stereo) ap->length /= 2;
	return (audio_instance_t*) ap; /* play_info_t is a superset of audio_instance_t */
}

static int portaudio_start(void *usr) {
	play_info_t *p = (play_info_t*) usr;
	PaError err;
	p->done = NO;
	
	err = Pa_OpenDefaultStream(&p->stream,
							   0, /* in ch. */
							   p->stereo ? 1 : 2, /* out ch */
#ifdef USEFLOAT
							   paFloat32,
#else
							   paInt16,
#endif
							   p->sample_rate,
							   // paFramesPerBufferUnspecified,
							   1024,
							   paPlayCallback,
							   p );

	if( err != paNoError ) Rf_error("cannot open audio for playback: %s\n", Pa_GetErrorText( err ) );
	err = Pa_StartStream( p->stream );
	if( err != paNoError ) Rf_error("cannot start audio playback: %s\n", Pa_GetErrorText( err ) );
	return YES;
}

static int portaudio_pause(void *usr) {
	play_info_t *p = (play_info_t*) usr;
	PaError err = Pa_StopStream( p->stream );
	return (err == paNoError);
}

static int portaudio_resume(void *usr) {
	play_info_t *p = (play_info_t*) usr;
	PaError err = Pa_StartStream( p->stream );
	return (err == paNoError);
}

static int portaudio_rewind(void *usr) {
	play_info_t *p = (play_info_t*) usr;
	p->position = 0;
	return 1;
}

#ifdef unix
/* helper function - precise sleep */
static void millisleep(double tout) {
	struct timeval tv;
	tv.tv_sec  = (unsigned int) tout;
	tv.tv_usec = (unsigned int)((tout - ((double)tv.tv_sec)) * 1000000.0);
	select(0, 0, 0, 0, &tv);
}
#endif

static int portaudio_wait(void *usr, double timeout) {
	play_info_t *p = (play_info_t*) usr;
	if (timeout < 0) timeout = 9999999.0; /* really a dummy high number */
	while (p == NULL || !p->done) {
		/* use 100ms slices */
		double slice = (timeout > 0.1) ? 0.1 : timeout;
		if (slice <= 0.0) break;
#ifdef unix
		millisleep(slice);
#if HAVE_AQUA
		R_ProcessEvents();
#else
		R_checkActivity(0, 0); /* FIXME: we should adjust for time spent processing events */
#endif
#else
		Sleep((DWORD) (slice * 1000));
		R_ProcessEvents();
#endif
		timeout -= slice;
	}
	return (p && p->done) ? WAIT_DONE : WAIT_TIMEOUT;
}

static int portaudio_close(void *usr) {
	play_info_t *p = (play_info_t*) usr;
    PaError err = Pa_CloseStream( p->stream );
	return (err == paNoError);
}

static void portaudio_dispose(void *usr) {
	Pa_Terminate();
	free(usr);
}

/* define the audio driver */
audio_driver_t portaudio_audio_driver = {
	sizeof(audio_driver_t),

	"portaudio",
	"PortAudio driver",
	"Copyright(c) 2008 Simon Urbanek",

	portaudio_create_player,
	0, /* recorder is currently unimplemented */
	portaudio_start,
	portaudio_pause,
	portaudio_resume,
	portaudio_rewind,
	portaudio_wait,
	portaudio_close,
	portaudio_dispose
};

#endif
