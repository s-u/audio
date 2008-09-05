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

#include "driver.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

static audio_driver_t *current_driver;

#if HAS_WMM
extern audio_driver_t wmmaudio_audio_driver;
#endif
#if HAS_PA
extern audio_driver_t portaudio_audio_driver;
#endif
#if HAS_AU
extern audio_driver_t audiounits_audio_driver;
#endif

/* if no drivers are available, must raise an Rf_error. */
static void load_default_audio_driver()
{
#if HAS_WMM
  current_driver = &wmmaudio_audio_driver;
  return;
#endif
#if HAS_AU
  current_driver = &audiounits_audio_driver;
  return;
#endif
#if HAS_PA
  current_driver = &portaudio_audio_driver;
  return;
#endif
  Rf_error("no audio drivers are available");
}

static void audio_instance_destructor(SEXP instance) {
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	p->driver->close(p);
	p->driver->dispose(p); /* it's driver's responsibility to dispose p */
}

SEXP audio_load_driver(SEXP path) {
#ifdef HAS_DLSYM
	if (TYPEOF(path) == STRSXP && LENGTH(path) > 0) {
		const char *cPath = CHAR(STRING_ELT(path, 0));
		void *(*fn)();
		void *ad, *dl = dlopen(cPath, RTLD_LAZY | RTLD_LOCAL); /* try local first */
		if (!dl) dl = dlopen(cPath, RTLD_LAZY | RTLD_GLOBAL); /* try global if local failed */
		if (!dl) Rf_error("cannot load '%s' dynamically", cPath);
		fn = dlsym(dl, "create_audio_driver");
		if (!fn) fn = dlsym(dl, "_create_audio_driver");
		if (!fn) {
			dlclose(dl);
			Rf_error("specified module is not an audio driver");
		}
		ad = fn();
		if (!ad) {
			dlclose(dl);
			Rf_error("audio driver could not be initialized");
		}
		/* FIXME: we never unload the driver module ... */
		current_driver = (audio_driver_t*) ad;
		return Rf_mkString(current_driver->name);
	} else
		Rf_error("invalid module name");
#else
	Rf_error("dynamic loading is not supported on this system");
#endif
	return R_NilValue;
}

SEXP audio_player(SEXP source, SEXP rate) {
	float fRate = -1.0;
	if (!current_driver)
		load_default_audio_driver();
	if (TYPEOF(rate) == INTSXP || TYPEOF(rate) == REALSXP)
		fRate = (float) Rf_asReal(rate);
	audio_instance_t *p = current_driver->create_player(source, fRate, 0);
	if (!p) Rf_error("cannot start audio driver");
	p->driver = current_driver;
	p->kind = AI_PLAYER;
	SEXP ptr = R_MakeExternalPtr(p, R_NilValue, R_NilValue);
	Rf_protect(ptr);
	R_RegisterCFinalizer(ptr, audio_instance_destructor);
	Rf_setAttrib(ptr, Rf_install("class"), Rf_mkString("audioInstance"));
	Rf_unprotect(1);
	return ptr;	
}

SEXP audio_recorder(SEXP source, SEXP rate, SEXP channels) {
	float fRate = -1.0;
	int chs = Rf_asInteger(channels);
	if (!current_driver)
		load_default_audio_driver();
	if (TYPEOF(rate) == INTSXP || TYPEOF(rate) == REALSXP)
		fRate = (float) Rf_asReal(rate);
	if (chs < 1) chs = 1;
	if (!current_driver->create_recorder)
		Rf_error("the currently used audio driver doesn't support recording");
	audio_instance_t *p = current_driver->create_recorder(source, fRate, chs, 0);
	if (!p) Rf_error("cannot start audio driver");
	p->driver = current_driver;
	p->kind = AI_RECORDER;
	SEXP ptr = R_MakeExternalPtr(p, R_NilValue, R_NilValue);
	Rf_protect(ptr);
	R_RegisterCFinalizer(ptr, audio_instance_destructor);
	Rf_setAttrib(ptr, Rf_install("class"), Rf_mkString("audioInstance"));
	Rf_unprotect(1);
	return ptr;
}

SEXP audio_start(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarLogical((p->driver)->start(p));
}

SEXP audio_pause(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarLogical((p->driver)->pause(p));
}

SEXP audio_resume(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarLogical((p->driver)->resume(p));
}

SEXP audio_rewind(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarLogical((p->driver)->rewind(p));
}

SEXP audio_close(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarLogical((p->driver)->close(p));
}

SEXP audio_driver_name(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_mkString(p->driver->name);
}

SEXP audio_instance_type(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarInteger(p->kind);
}

SEXP audio_instance_source(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return p->source;
}

SEXP audio_instance_address(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarInteger((int) p);	
}