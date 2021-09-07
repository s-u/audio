/* Audio device infrastructure for R
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

#ifndef AUDIO_DRIVER_H__
#define AUDIO_DRIVER_H__

#ifdef HAS_CONFIG_H
#include "config.h"
#endif

#define R_NO_REMAP      /* to not pollute the namespace */
#include <R.h>
#include <Rinternals.h>

#define R_AUDIO_API 1.0

#define APFLAG_LOOP   0x0001

#define WAIT_DONE     1
#define WAIT_TIMEOUT  2
#define WAIT_ERROR   -1

/* define driver structure */
typedef struct audio_driver {
	unsigned int length; /* length of the driver structure, i.e., sizeof(audio_driver_t) */
	const char *name;  /* short identifier */
	const char *descr; /* description */
	const char *copyright; /* copyright (optional) */

	struct audio_instance *(*create_player)(SEXP, float, int); /* source, rate (if applicable), flags */
	struct audio_instance *(*create_recorder)(SEXP, float, int, int); /* target, rate, channels, flags; (optional) */
	int (*start)(void *);
	int (*pause)(void *);
	int (*resume)(void *);
	int (*rewind)(void *);
	int (*wait)(void *, double timeout);
	int (*close)(void *);
	void (*dispose)(void *);
} audio_driver_t;

#define AI_PLAYER   1
#define AI_RECORDER 2

/* define audio instance structure. individual implementations
   are free to add their own fields, but those listed below must be
   common to all instances */
typedef struct audio_instance {
	audio_driver_t *driver;  /* must point to the driver that created this */
	int kind;                /* must be either AI_PLAYER or AI_RECORDER */
	SEXP source;             /* source (player) or target (recorder) */ 
} audio_instance_t;

#endif
