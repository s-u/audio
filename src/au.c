/* Audio device for R using AudioUnits (Mac OS X)
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
#include <sys/select.h> /* for select in millisleep */

#define kNumberOutputBuffers 3
#define kOutputBufferSize 4096

#ifndef YES
#define BOOL int
#define YES 1
#define NO 0
#endif

#ifdef NEED_AUDIO_PREFIX
#include <CoreAudio/CoreAudio.h>
#define Component AudioComponent
#define FindNextComponent AudioComponentFindNext
#define ComponentDescription AudioComponentDescription
#define OpenAComponent AudioComponentInstanceNew
#define CloseComponent AudioComponentInstanceDispose
/* Note: AudioObjectGetPropertyData will replace AudioHardwareGetProperty */
#endif

typedef struct au_instance {
	/* the following entries must be present since play_info_t inherits from audio_instance_t */
	audio_driver_t *driver;  /* must point to the driver that created this */
	int kind;                /* must be either AI_PLAYER or AI_RECORDER */
	SEXP source;
	/* private entries */
	AudioUnit outUnit;
	AudioDeviceID inDev;
	AudioStreamBasicDescription fmtOut, fmtIn;
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED>=MAC_OS_X_VERSION_10_5)
	AudioDeviceIOProcID inIOProcID;
#endif
	float sample_rate;
	double srFrac, srRun;
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
	au_instance_t *p = (au_instance_t*) inRefCon;
	/* printf("outputRenderProc, (bufs=%d, buf[0].chs=%d), buf=%p, size=%d\n", ioData->mNumberBuffers, ioData->mBuffers[0].mNumberChannels, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize); */
	int res = primeBuffer(p, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize / (p->stereo ? 4 : 2));
	/* printf(" - primed: %d samples (%d bytes)\n", res, res * (p->stereo ? 4 : 2)); */
	if (res < 0) res = 0;
	ioData->mBuffers[0].mDataByteSize = res * (p->stereo ? 4 : 2);
	if (res == 0) {
		/* printf(" - no input, stopping unit\n"); */
		AudioOutputUnitStop(p->outUnit);
	}
	return noErr;
}

static au_instance_t *audiounits_create_player(SEXP source, float rate, int flags) {
	ComponentDescription desc = { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput, kAudioUnitManufacturer_Apple, 0, 0 };
	Component comp; 
	OSStatus err;
	
	au_instance_t *ap = (au_instance_t*) calloc(sizeof(au_instance_t), 1);
	ap->source = source;
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
	memset(&ap->fmtOut, 0, sizeof(ap->fmtOut));
	ap->fmtOut.mSampleRate = ap->sample_rate;
	ap->fmtOut.mFormatID = kAudioFormatLinearPCM;
	ap->fmtOut.mChannelsPerFrame = ap->stereo ? 2 : 1;
	ap->fmtOut.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
#if __ppc__ || __ppc64__ || __BIG_ENDIAN__
	ap->fmtOut.mFormatFlags |= kAudioFormatFlagIsBigEndian;
#endif
	ap->fmtOut.mFramesPerPacket = 1;
	ap->fmtOut.mBytesPerPacket = ap->fmtOut.mBytesPerFrame = ap->fmtOut.mFramesPerPacket * ap->fmtOut.mChannelsPerFrame * 2;
	ap->fmtOut.mBitsPerChannel = 16;
	if (ap->stereo) ap->length /= 2;
	comp = FindNextComponent(NULL, &desc);
	if (!comp) Rf_error("unable to find default audio output"); 
	err = OpenAComponent(comp, &ap->outUnit);
	if (err) Rf_error("unable to open default audio (%08x)", err);
	err = AudioUnitInitialize(ap->outUnit);
	if (err) {
		CloseComponent(ap->outUnit);
		Rf_error("unable to initialize default audio (%08x)", err);
	}
	R_PreserveObject(ap->source);
	return ap;
}

static int audiounits_pause(void *usr);

static OSStatus inputRenderProc(AudioDeviceID inDevice, 
				const AudioTimeStamp*inNow, 
				const AudioBufferList*inInputData, 
				const AudioTimeStamp*inInputTime, 
				AudioBufferList*outOutputData, 
				const AudioTimeStamp*inOutputTime, 
				void*inClientData) {
	float *s = (float*) inInputData->mBuffers[0].mData;
	unsigned int len = inInputData->mBuffers[0].mDataByteSize / sizeof(float), i = 0, ichs = inInputData->mBuffers[0].mNumberChannels;
	au_instance_t *ap = (au_instance_t*) inClientData;
	/* Rprintf("inputRenderProc, (bufs=%d, buf[0].chs=%d), buf=%p, size=%d [%d samples]\n", inInputData->mNumberBuffers, inInputData->mBuffers[0].mNumberChannels, inInputData->mBuffers[0].mData, inInputData->mBuffers[0].mDataByteSize, len); */
	if (TYPEOF(ap->source) == REALSXP) {
		double *d = REAL(ap->source), srr = ap->srRun, srf = ap->srFrac;
		unsigned int chs = ap->stereo ? 2 : 1;
		/* FIXME: we're assuming that channels can only be 1 or 2 */
		while (ap->position < ap->length && i < len) {
			srr += srf;
			if (srr >= 1.0) {
				if (ichs > chs) d[ap->position++] = (s[i] + s[i + 1]) / 2; 
				else {
					if (ichs < chs) d[ap->position++] = s[i];
					d[ap->position++] = s[i];
				}
				srr -= 1.0;
			};
			i++;
		}
		ap->srRun = srr;
	}
	/* pause the unit when the recording is complete */
	if (ap->position >= ap->length) {
		ap->done = YES;
		audiounits_pause(ap);
	}
	return 0;
}

static au_instance_t *audiounits_create_recorder(SEXP source, float rate, int chs, int flags) {
	UInt32 propsize=0;
	OSStatus err;
	AudioObjectPropertyAddress aopAddress;

	au_instance_t *ap = (au_instance_t*) calloc(sizeof(au_instance_t), 1);
	ap->source = source;
	ap->sample_rate = rate;
	ap->done = NO;
	ap->position = 0;
	ap->length = LENGTH(source);
	ap->stereo = (chs == 2) ? YES : NO;
	
	propsize = sizeof(ap->inDev);
	aopAddress = (AudioObjectPropertyAddress) { kAudioHardwarePropertyDefaultInputDevice,
		       kAudioObjectPropertyScopeGlobal,
		       kAudioObjectPropertyElementMaster };

	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopAddress, 0, NULL,
					 &propsize, &ap->inDev);
	if (err) {
		free(ap);
		Rf_error("unable to find default audio input (%08x)", err);
	}

	propsize = sizeof(ap->fmtIn);
	aopAddress = (AudioObjectPropertyAddress) { kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput, 0 };
	err = AudioObjectGetPropertyData(ap->inDev, &aopAddress, 0, NULL,
					 &propsize, &ap->fmtIn);
	if (err) {
		free(ap);
		Rf_error("unable to retrieve audio input format (%08x)", err);
	}

	/* Rprintf(" recording format: %f, chs: %d, fpp: %d, bpp: %d, bpf: %d, flags: %x\n", ap->fmtIn.mSampleRate, ap->fmtIn.mChannelsPerFrame, ap->fmtIn.mFramesPerPacket, ap->fmtIn.mBytesPerPacket, ap->fmtIn.mBytesPerFrame, ap->fmtIn.mFormatFlags); */
	
	ap->srFrac = 1.0;
	if (ap->fmtIn.mSampleRate != ap->sample_rate) ap->srFrac = ap->sample_rate / ap->fmtIn.mSampleRate;
	ap->srRun = 0.0;
	
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED>=MAC_OS_X_VERSION_10_5)
	err = AudioDeviceCreateIOProcID(ap->inDev, inputRenderProc, ap, &ap->inIOProcID );
#else
	err = AudioDeviceAddIOProc(ap->inDev, inputRenderProc, ap);
#endif
	if (err) {
		free(ap);
		Rf_error("unable to register recording callback (%08x)", err);
	}
	R_PreserveObject(ap->source);
	Rf_setAttrib(ap->source, Rf_install("rate"), Rf_ScalarInteger((int)rate)); /* we adjust the rate */
	Rf_setAttrib(ap->source, Rf_install("bits"), Rf_ScalarInteger(16)); /* we say it's 16 because we don't know - float is always 32-bit */
	Rf_setAttrib(ap->source, Rf_install("class"), Rf_mkString("audioSample"));
	if (ap->stereo) {
		SEXP dim = Rf_allocVector(INTSXP, 2);
		INTEGER(dim)[0] = 2;
		INTEGER(dim)[1] = LENGTH(ap->source) / 2;
		Rf_setAttrib(ap->source, R_DimSymbol, dim);
	}
	return ap;
}

static int audiounits_start(void *usr) {
	au_instance_t *ap = (au_instance_t*) usr;
	OSStatus err;
	if (ap->kind == AI_RECORDER) {
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED>=MAC_OS_X_VERSION_10_5)
		err = AudioDeviceStart(ap->inDev, ap->inIOProcID);
#else
		err = AudioDeviceStart(ap->inDev, inputRenderProc);
#endif
		if (err) Rf_error("unable to start recording (%08x)", err);
	} else {
		AURenderCallbackStruct renderCallback = { outputRenderProc, usr };
		ap->done = NO;
		/* set format */
		ap->fmtOut.mSampleRate = ap->sample_rate;
		err = AudioUnitSetProperty(ap->outUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &ap->fmtOut, sizeof(ap->fmtOut));
		if (err) Rf_error("unable to set output audio format (%08x)", err);
		/* set callback */
		err = AudioUnitSetProperty(ap->outUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &renderCallback, sizeof(renderCallback));
		if (err) Rf_error("unable to register audio callback (%08x)", err);
		/* start audio */
		err = AudioOutputUnitStart(ap->outUnit);
		if (err) Rf_error("unable to start playback (%08x)", err);
	}
	return 1;
}

static int audiounits_pause(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED>=MAC_OS_X_VERSION_10_5)
	if (p->kind == AI_RECORDER)
		return AudioDeviceStop(p->inDev, p->inIOProcID) ? 0 : 1;
#else
	if (p->kind == AI_RECORDER)
		return AudioDeviceStop(p->inDev, inputRenderProc) ? 0 : 1;
#endif
	return AudioOutputUnitStop(p->outUnit) ? 0 : 1;
}

static int audiounits_rewind(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	p->position = 0;
	return 1;
}

static int audiounits_resume(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	return AudioOutputUnitStart(p->outUnit) ? 0 : 1;
}

/* helper function - precise sleep */
static void millisleep(double tout) {
	struct timeval tv;
	tv.tv_sec  = (unsigned int) tout;
	tv.tv_usec = (unsigned int)((tout - ((double)tv.tv_sec)) * 1000000.0);
	select(0, 0, 0, 0, &tv);
}

static int audiounits_wait(void *usr, double timeout) {
	au_instance_t *p = (au_instance_t*) usr;
	if (timeout < 0) timeout = 9999999.0; /* really a dummy high number */
	while (p == NULL || !p->done) {
		/* use 100ms slices */
		double slice = (timeout > 0.1) ? 0.1 : timeout;
		if (slice <= 0.0) break;
		millisleep(slice);
		R_CheckUserInterrupt(); /* FIXME: we should adjust for time spent processing events */
		timeout -= slice;
	}
	return (p && p->done) ? WAIT_DONE : WAIT_TIMEOUT;
}

static int audiounits_close(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	p->done = YES;
	/* Rprintf(" closing audiounit %p\n", usr); */
	if (p->outUnit) {
		AudioOutputUnitStop(p->outUnit);
		AudioUnitUninitialize(p->outUnit);
		CloseComponent(p->outUnit);
		p->outUnit = 0;
	}
	if (p->inDev) {
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED>=MAC_OS_X_VERSION_10_5)
		AudioDeviceDestroyIOProcID(p->inDev, p->inIOProcID);
#else
		AudioDeviceRemoveIOProc(p->inDev, inputRenderProc);
#endif
		p->inDev = 0;
	}
	return 1;
}

static void audiounits_dispose(void *usr) {
	au_instance_t *p = (au_instance_t*) usr;
	if (p->outUnit || p->inDev) audiounits_close(usr);
#if 0
	int i = 0;
	while (i < kNumberOutputBuffers) {
		if (p->bufOut[i]) { free(p->bufOut[i]); p->bufOut[i] = 0; }
		i++;
	}
#endif
	free(usr);
}

/* define the audio driver */
audio_driver_t audiounits_audio_driver = {
	sizeof(audio_driver_t),
	"macosx",
	"AudioUnits (Mac OS X) driver",
	"Copyright(c) 2008 Simon Urbanek",
	(create_player_t) audiounits_create_player,
	(create_recorder_t) audiounits_create_recorder,
	audiounits_start,
	audiounits_pause,
	audiounits_resume,
	audiounits_rewind,
	audiounits_wait,
	audiounits_close,
	audiounits_dispose
};

#endif
