/* Audio device for R using AudioUnits (Mac OS X)
   Note: this is now a skeleton only
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

#if HAS_AU
#include <AudioUnit/AudioUnit.h>

#define kNumberOutputBuffers 3
#define kOutputBufferSize 4096

typedef struct au_instance {
	/* the following entries must be present since play_info_t inherits from audio_instance_t */
	audio_driver_t *driver;  /* must point to the driver that created this */
	int kind;                /* must be either AI_PLAYER or AI_RECORDER */
	AudioUnit outUnit;
	char *bufOut[kNumberOutputBuffers];
	float sample_rate;
	SEXP source;
	BOOL stereo, loop, done;
	unsigned int position, length;
} au_instance_t;
	
/* fill a buffer and return the number of frames filled */
static int primeBuffer(au_instance_t *ap, void *outputBuffer, unsigned int framesPerBuffer)
{
	if (ap->position == ap->length && ap->loop)
		ap->position = 0;
	unsigned int index = ap->position;
	unsigned int rem = ap->length - index;
	unsigned int spf = ap->stereo ? 2 : 1;
	if (rem > framesPerBuffer) rem = framesPerBuffer;
	//printf("position=%d, length=%d, (LEN=%d), rem=%d, cap=%d, spf=%d\n", ap->position, ap->length, LENGTH(ap->source), rem, framesPerBuffer, spf);
	index *= spf;
	// there is a small caveat - if a zero-size buffer comes along it will stop the playback since rem will be forced to 0 - but then that should not happen ...
	if (rem > 0) {
		unsigned int samples = rem * spf; // samples (i.e. SInt16s)
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
		} // FIXME: support functions as sources...
		ap->position += rem;
	} else {
		// printf(" rem ==0 -> stop queue\n");
		ap->done = YES;
		return 0;
	}
	return rem;
}

static OSStatus outputRenderProc(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumFrames, AudioBufferList *ioData)
{
	au_instance_t *ap = (au_instance_t*) inRefCon;
	// ioData->mBuffers[0].mDataByteSize = amt;
}

static au_instance_t *audiounits_create_player(SEXP source) {
	au_instance_t *ap = (au_instance_t*) calloc(sizeof(au_instance_t), 1);
	ap->source = source;
	R_PreserveObject(ap->source);
	ap->sample_rate = 44100.0;
	ap->done = NO;
	ap->position = 0;
	ap->length = LENGTH(source);
	ap->stereo = NO; // FIXME: support dim[2] = 2
	ap->loop = NO;
	if (ap->stereo) ap->length /= 2;
	return ap;
}

static int audiounits_start(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	p->done = NO;
	/* open audio */
	/* allocate and prime buffers */
	return 1;
}
	

static int audiounits_pause(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	return 1;
}

static int audiounits_resume(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	return 1;
}

static int audiounits_close(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	p->done = YES;
	return 1;
}

static void audiounits_dispose(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	int i = 0;
	while (i < kNumberOutputBuffers) {
		if (p->bufOut[i]) { free(p->bufOut[i]); p->bufOut[i] = 0; }
		i++;
	}
	free(usr);
}

/* define the audio driver */
audio_driver_t audiounits_audio_driver = {
	audiounits_create_player,
	0, /* recorder is currently unimplemented */
	audiounits_start,
	audiounits_pause,
	audiounits_resume,
	audiounits_close,
	audiounits_dispose
};

#endif
