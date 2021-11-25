/* Audio device for R using Windows MultiMedia (winmm) library
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

#if HAS_WMM
#include <windows.h>

#define kNumberOutputBuffers 3
#define kOutputBufferSize 4096

#define kNumberInputBuffers 3
#define kInputBufferSize 4096

typedef struct wmm_instance {
	/* the following entries must be present since play_info_t inherits from audio_instance_t */
	audio_driver_t *driver;  /* must point to the driver that created this */
	int kind;                /* must be either AI_PLAYER or AI_RECORDER */
	SEXP source;
	/* private entries */
	HWAVEOUT hout;
	HWAVEIN hin;
	char *bufOut[(kNumberOutputBuffers > kNumberInputBuffers) ? kNumberOutputBuffers : kNumberInputBuffers];
	WAVEHDR bufOutHdr[(kNumberOutputBuffers > kNumberInputBuffers) ? kNumberOutputBuffers : kNumberInputBuffers];
	float sample_rate;
	BOOL stereo, loop, done;
	unsigned int position, length;
	int dequeued; /* set to non-zero if any buffers have been dequeued (e.g. at the end of playback) */
} wmm_instance_t;
	
/* legacy from OS X API .. */
typedef signed short int SInt16;
#ifndef YES
#define YES 1
#define NO  0
#endif

/* fill a buffer and return the number of frames filled */
static int primeBuffer(wmm_instance_t *ap, void *outputBuffer, unsigned int framesPerBuffer)
{
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
		unsigned int samples = rem * spf; /* samples (i.e. SInt16s) */
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
		ap->position += rem;
	} else {
		/* printf(" rem ==0 -> stop queue\n"); */
		ap->done = YES;
		return 0;
	}
	return rem;
}

/* the sole purpose of the feede thread is to feed prepared buffers to the wave device since this is not allowed in the callback */
HANDLE feederThread;
DWORD  feederThreadId;

#define MSG_OUTBUFFER 0x410
#define MSG_QUIT      0x408

static DWORD WINAPI feederThreadProc(LPVOID usr) {
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		switch (msg.message) {
			case MSG_OUTBUFFER:
		    {
			    wmm_instance_t *p = (wmm_instance_t*) msg.lParam;
			    unsigned int bufId = msg.wParam;
			    waveOutWrite(p->hout, &p->bufOutHdr[bufId], sizeof(WAVEHDR));
		    }
				break;
			case MSG_QUIT:
				return 0;
		}
	}
	return 0;
}

static void CALLBACK waveOutProc(HWAVEOUT hwo,      
			  UINT uMsg,         
			  DWORD_PTR dwInstance,  
			  DWORD_PTR dwParam1,    
			  DWORD_PTR dwParam2     
			  ) {
	switch (uMsg) {
		case WOM_CLOSE:
			break;
		case WOM_OPEN:
			break;
		case WOM_DONE:
	    {
		    WAVEHDR *hdr = (WAVEHDR*) dwParam1;
		    wmm_instance_t *ap = (wmm_instance_t*) hdr->dwUser;
		    unsigned int bufSize = hdr->dwBufferLength;
		    unsigned int bpf = ap->stereo ? 4 : 2;
		    int res = primeBuffer(ap, hdr->lpData, bufSize / bpf);
		    if (res > 0) {
			    unsigned int bufId = 0;
			    while (bufId < kNumberOutputBuffers && &ap->bufOutHdr[bufId] != hdr) bufId++;
			    hdr->dwBytesRecorded = ((unsigned int) res) * bpf;
			    PostThreadMessage(feederThreadId, MSG_OUTBUFFER, bufId, (LPARAM) ap);			    
		    } else ap->dequeued++;
	    }
		    break;
	}
}

static void CALLBACK waveInProc(
				HWAVEIN hwi,       
				UINT uMsg,         
				DWORD_PTR dwInstance,  
				DWORD_PTR dwParam1,    
				DWORD_PTR dwParam2) {
	switch (uMsg) {
		case WIM_CLOSE:
			break;
		case WIM_OPEN:
			break;
		case WIM_DATA:
		{
			WAVEHDR *hdr = (WAVEHDR*) dwParam1;
			wmm_instance_t *ap = (wmm_instance_t*) hdr->dwUser;
			signed short int *si = (signed short int*) hdr->lpData;
			unsigned int len = hdr->dwBytesRecorded / 2;
			if (TYPEOF(ap->source) == REALSXP) {
				double *d = REAL(ap->source);
				unsigned int cp = ap->position, lp = LENGTH(ap->source), i = 0;
				while (i < len && cp < lp)
					d[cp++] = ((double)si[i++]) / 32768.0;
				ap->position = cp;
			}
			if (ap->position >= ap->length) /* pause if we reach the end */
				waveInStop(ap->hin);
			hdr->dwBytesRecorded = 0;
			hdr->dwLoops = 0;
			hdr->dwBufferLength = kInputBufferSize;
			hdr->dwFlags &= WHDR_PREPARED;	/* reset all bits except prepared */
			if ((hdr->dwFlags & WHDR_PREPARED) == 0)
				waveInPrepareHeader(ap->hin, hdr, sizeof(*hdr));
			/* re-enqueue the buffer */
			waveInAddBuffer(ap->hin, hdr, sizeof(*hdr));
		}
	}
}

static wmm_instance_t *wmmaudio_create_player(SEXP source, float rate, int flags) {
	wmm_instance_t *ap = (wmm_instance_t*) calloc(sizeof(wmm_instance_t), 1);
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
	if (!feederThread)
		feederThread = CreateThread(0, 0, feederThreadProc, 0, 0, &feederThreadId);
	return ap;
}

static wmm_instance_t *wmmaudio_create_recorder(SEXP source, float rate, int channels, int flags) {
	wmm_instance_t *ap = (wmm_instance_t*) calloc(sizeof(wmm_instance_t), 1);
	ap->source = source;
	ap->sample_rate = rate;
	ap->done = NO;
	ap->position = 0;
	ap->length = LENGTH(source);
	ap->stereo = (channels == 2) ? YES : NO;
	ap->loop = (flags & APFLAG_LOOP) ? YES : NO;
	/* if (ap->stereo) ap->length /= 2; we're bad - we us eposition as a raw pointer in the samples */
	MMRESULT res;
	WAVEFORMATEX fmt = {
		WAVE_FORMAT_PCM, 
		ap->stereo ? 2 : 1,
		(unsigned int) ap->sample_rate,
		((unsigned int) ap->sample_rate) * (ap->stereo ? 4 : 2),
		ap->stereo ? 4 : 2,
		16,
		0
	};
	ap->done = NO;
	
	/* open audio */
	res = waveInOpen(&ap->hin, WAVE_MAPPER, &fmt, (DWORD_PTR)waveInProc, 0, CALLBACK_FUNCTION);
	if (res) Rf_error("unable to open WMM audio for recording (%d)", res);
	
	/* allocate and prepare buffers */
	{
		unsigned int bufferSize = kInputBufferSize;
		int i = 0;
		while (i < kNumberInputBuffers) {
			ap->bufOut[i] = (char*) malloc(bufferSize);
			memset(&ap->bufOutHdr[i], 0, sizeof(ap->bufOutHdr[i]));
			ap->bufOutHdr[i].lpData = (LPSTR) ap->bufOut[i];
			ap->bufOutHdr[i].dwBufferLength = bufferSize;
			ap->bufOutHdr[i].dwBytesRecorded = 0;
			ap->bufOutHdr[i].dwUser = (DWORD_PTR) ap;
			res = waveInPrepareHeader(ap->hin, &ap->bufOutHdr[i], sizeof(ap->bufOutHdr[i]));
			if (res || waveInAddBuffer(ap->hin, &ap->bufOutHdr[i], sizeof(ap->bufOutHdr[i]))) {
				while (i >= 0) {
					free(ap->bufOut[i]); ap->bufOut[i] = 0;
					i--;
					if (i >= 0)
						waveInUnprepareHeader(ap->hin, &ap->bufOutHdr[i], sizeof(ap->bufOutHdr[i]));
				}
				waveInClose(ap->hin);
				ap->hin = 0;
				Rf_error("unable to prepare WMM audio buffer %d for recording (%d)", i, res);
			}
			i++;
		}
	}
	    
	R_PreserveObject(ap->source);
	
	Rf_setAttrib(ap->source, Rf_install("rate"), Rf_ScalarInteger(rate)); /* we adjust the rate */
        Rf_setAttrib(ap->source, Rf_install("bits"), Rf_ScalarInteger(16)); /* we always use 16-bit for recording */
        Rf_setAttrib(ap->source, Rf_install("class"), Rf_mkString("audioSample"));
        if (ap->stereo) {
                SEXP dim = Rf_allocVector(INTSXP, 2);
                INTEGER(dim)[0] = 2;
                INTEGER(dim)[1] = LENGTH(ap->source) / 2;
                Rf_setAttrib(ap->source, R_DimSymbol, dim);
        }
	
	return ap;
}

static int wmmaudio_start(void *usr) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	if (p->kind == AI_RECORDER) {
		if (p->hin) return waveInStart(p->hin) ? NO : YES;
		return NO;
	}
	
	MMRESULT res;
	WAVEFORMATEX fmt = {
		WAVE_FORMAT_PCM, 
		p->stereo ? 2 : 1,
		(unsigned int) p->sample_rate,
		((unsigned int) p->sample_rate) * (p->stereo ? 4 : 2),
		p->stereo ? 4 : 2,
		16,
		0
	};
	p->done = NO;
	/* open audio */
	res = waveOutOpen(&p->hout, WAVE_MAPPER, &fmt, (DWORD_PTR)waveOutProc, 0, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
	if (res) Rf_error("unable to open WMM audio for output (%d)", res);
	{
		/* allocate and prime buffers */
		unsigned int bufferSize = kOutputBufferSize;
		int i = 0;
		while (i < kNumberOutputBuffers) {
			p->bufOut[i] = (char*) malloc(bufferSize);
			memset(&p->bufOutHdr[i], 0, sizeof(p->bufOutHdr[i]));
			p->bufOutHdr[i].lpData = (LPSTR) p->bufOut[i];
			p->bufOutHdr[i].dwBufferLength = p->bufOutHdr[i].dwBytesRecorded = bufferSize;
			p->bufOutHdr[i].dwUser = (DWORD_PTR) p;
			res = waveOutPrepareHeader(p->hout, &p->bufOutHdr[i], sizeof(p->bufOutHdr[i]));
			if (res) {
				while (i >= 0) {
					free(p->bufOut[i]); p->bufOut[i] = 0;
					i--;
					if (i >= 0)
						waveOutUnprepareHeader(p->hout, &p->bufOutHdr[i], sizeof(p->bufOutHdr[i]));
				}
				waveOutClose(p->hout);
				Rf_error("unable to prepare WMM audio buffer %d for output (%d)", i, res);
			}
			{
				int pres = primeBuffer(p, p->bufOut[i], bufferSize / (p->stereo ? 4 : 2));
				if (pres < 0) pres = 0;
				p->bufOutHdr[i].dwBytesRecorded = pres * (p->stereo ? 4 : 2);
			}
			i++;
		}
		
		/* enqueue all (non-empty) buffers */
		i = 0;
		while (i < kNumberOutputBuffers && p->bufOutHdr[i].dwBytesRecorded > 0) 
			waveOutWrite(p->hout, &p->bufOutHdr[i++], sizeof(p->bufOutHdr[0]));
	}
	return 1;
}
	

static int wmmaudio_pause(void *usr) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	if (p->hout)
		waveOutPause(p->hout);
	if (p->hin)
		waveInStop(p->hin);
	return 1;
}

static int wmmaudio_resume(void *usr) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	if (p->kind == AI_RECORDER) {
		if (p->hin)
			return waveInStart(p->hin) ? NO : YES;
		return NO;
	}
	/* if buffers have been dequeued before, we need to enqueue them back */
	if (p->dequeued && p->position < p->length) {
		unsigned int bufferSize = kOutputBufferSize;
		int i = 0;
		while (i < kNumberOutputBuffers) {
			int pres = primeBuffer(p, p->bufOut[i], bufferSize / (p->stereo ? 4 : 2));
			if (pres < 1) break;
			p->bufOutHdr[i].dwBytesRecorded = pres * (p->stereo ? 4 : 2);			
			waveOutWrite(p->hout, &p->bufOutHdr[i++], sizeof(p->bufOutHdr[0]));
		}
		p->dequeued = 0;
	}

	if (p->hout)
		waveOutRestart(p->hout);
	return 1;
}

static int wmmaudio_rewind(void *usr) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	p->position = 0;
	return 1;
}

static int wmmaudio_wait(void *usr, double timeout) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	if (timeout < 0) timeout = 9999999.0; /* really a dummy high number */
	while (p == NULL || !p->done) {
		/* use 100ms slices */
		double slice = (timeout > 0.1) ? 0.1 : timeout;
		if (slice <= 0.0) break;
		Sleep((DWORD) (slice * 1000));
		R_ProcessEvents(); /* FIXME: we should adjust for time spent processing events */
		timeout -= slice;
	}
	return (p && p->done) ? WAIT_DONE : WAIT_TIMEOUT;
}

static int wmmaudio_close(void *usr) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	p->done = YES;
	if (p->hout)
		waveOutClose(p->hout);
	if (p->hin)
		waveInClose(p->hin);
	p->hout = 0;
	p->hin = 0;
	return 1;
}

static void wmmaudio_dispose(void *usr) {
	wmm_instance_t *p = (wmm_instance_t*) usr;
	wmmaudio_close(usr);
	unsigned int i = 0, j = (p->kind == AI_PLAYER) ? kNumberOutputBuffers : kNumberInputBuffers;
	while (i < j) {
		if (p->bufOut[i]) { free(p->bufOut[i]); p->bufOut[i] = 0; }
		i++;
	}
	free(usr);
}

/* define the audio driver */
audio_driver_t wmmaudio_audio_driver = {
	sizeof(audio_driver_t),	       

	"wmm",
	"Windows MultiMedia audio driver",
	"Copyright(c) 2008 Simon Urbanek",

	(create_player_t) wmmaudio_create_player,
	(create_recorder_t) wmmaudio_create_recorder,
	wmmaudio_start,
	wmmaudio_pause,
	wmmaudio_resume,
	wmmaudio_rewind,
	wmmaudio_wait,
	wmmaudio_close,
	wmmaudio_dispose
};

#endif
