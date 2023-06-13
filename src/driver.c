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
#ifdef __WIN32__
#include <windows.h>
#else
#include <sys/select.h> /* for millisleep */
#endif

static audio_driver_t *current_driver;

typedef struct audio_driver_list {
	audio_driver_t *driver;
	struct audio_driver_list *next;
} audio_driver_list_t;

#if HAS_WMM
extern audio_driver_t wmmaudio_audio_driver;
#endif
#if HAS_PA
extern audio_driver_t portaudio_audio_driver;
#endif
#if HAS_AU
extern audio_driver_t audiounits_audio_driver;
#endif

static audio_driver_list_t audio_drivers;

static void set_audio_driver(audio_driver_t *driver) {
	if (audio_drivers.driver == NULL) {
		current_driver = audio_drivers.driver = driver;
		return;
	} else {
		audio_driver_list_t *l = &audio_drivers;
		while (l) {
			if (l->driver == driver) {
				current_driver = driver;
				return;
			}
			if (l->next == NULL) {
				l->next = (audio_driver_list_t*) malloc(sizeof(audio_driver_list_t));
				if (!l->next) Rf_error("out of memory");
				current_driver = l->next->driver = driver;
				l->next->next = 0;
				return;
			}
			l = l->next;
		}
		/* we should never reach this line */
	}
}

/* if no drivers are available, must raise an Rf_error. */
static void load_default_audio_driver(int silent)
{
	/* load the drivers in the order of precedence such that the first one is the default one */
#if HAS_WMM
	set_audio_driver(&wmmaudio_audio_driver);
#endif
#if HAS_AU
	set_audio_driver(&audiounits_audio_driver);
#endif
#if HAS_PA
	set_audio_driver(&portaudio_audio_driver);
#endif
	/* pick the first one - it will be NULL if there are no drivers */
	current_driver = audio_drivers.driver;
	if (!silent && !current_driver)
		Rf_error("no audio drivers are available");
}

static void audio_instance_destructor(SEXP instance) {
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	p->driver->close(p);
	p->driver->dispose(p); /* it's driver's responsibility to dispose p */
}

SEXP audio_drivers_list(void) {
	int n = 0;
	SEXP res = Rf_allocVector(VECSXP, 3), sName, sDesc, /* sCopy, */ sCurr, sLN, sRN;
	audio_driver_list_t *l = &audio_drivers;
	if (!current_driver)
		load_default_audio_driver(1);
	Rf_protect(res);
	if (l->driver) {
		while (l) {
			n++;
			l = l->next;
		}
	}
	sName = Rf_allocVector(STRSXP, n); SET_VECTOR_ELT(res, 0, sName);
	sDesc = Rf_allocVector(STRSXP, n); SET_VECTOR_ELT(res, 1, sDesc);
	sCurr = Rf_allocVector(LGLSXP, n); SET_VECTOR_ELT(res, 2, sCurr);
	/* sCopy = Rf_allocVector(STRSXP, n); SET_VECTOR_ELT(res, 3, sCopy); */
	if (n) {
		n = 0;
		l = &audio_drivers;
		while (l) {
			const char *s = l->driver->name;
			SET_STRING_ELT(sName, n, Rf_mkChar(s ? s : ""));
			s = l->driver->descr;
			SET_STRING_ELT(sDesc, n, Rf_mkChar(s ? s : ""));
			s = l->driver->copyright;
			/* SET_STRING_ELT(sCopy, n, Rf_mkChar(s ? s : "")); */
			LOGICAL(sCurr)[n] = (l->driver == current_driver) ? 1 : 0;
			l = l->next;
			n++;
		}
	}
	sLN = Rf_allocVector(STRSXP, 3);
	Rf_setAttrib(res, R_NamesSymbol, sLN);
	SET_STRING_ELT(sLN, 0, Rf_mkChar("name"));
	SET_STRING_ELT(sLN, 1, Rf_mkChar("description"));
	SET_STRING_ELT(sLN, 2, Rf_mkChar("current"));
	/* SET_STRING_ELT(sLN, 3, Rf_mkChar("author")); */
	sRN = Rf_allocVector(INTSXP, 2);
	INTEGER(sRN)[0] = R_NaInt;
	INTEGER(sRN)[1] = -n;
	Rf_setAttrib(res, R_RowNamesSymbol, sRN);
	Rf_setAttrib(res, R_ClassSymbol, Rf_mkString("data.frame"));
	Rf_unprotect(1);
	return res;	
}

SEXP audio_current_driver(void) {
	return current_driver ? Rf_mkString(current_driver->name) : R_NilValue;
}

SEXP audio_use_driver(SEXP sName) {
	if (sName == R_NilValue) { /* equivalent to saying 'load default driver' */
		if (!current_driver) load_default_audio_driver(1);
		current_driver = audio_drivers.driver;
		if (!current_driver || !current_driver->name) {
			Rf_warning("no audio drivers are available");
			return R_NilValue;
		}
		return Rf_mkString(current_driver->name);
	}
	if (TYPEOF(sName) != STRSXP || LENGTH(sName) < 1)
		Rf_error("invalid audio driver name");
	else {
		const char *drv_name = CHAR(STRING_ELT(sName, 0));
		audio_driver_list_t *l = &audio_drivers;
		if (!current_driver)
			load_default_audio_driver(1);
		while (l && l->driver) {
			if (l->driver->name && !strcmp(l->driver->name, drv_name)) {
				current_driver = l->driver;
				return sName;
			}
			l = l->next;
		}			
		Rf_warning("driver '%s' not found", drv_name);
	}
	return R_NilValue;
}

SEXP audio_load_driver(SEXP path) {
#ifdef HAS_DLSYM
	if (TYPEOF(path) == STRSXP && LENGTH(path) > 0) {
		const char *cPath = CHAR(STRING_ELT(path, 0));
		audio_driver_t *drv;
		void *(*fn)(void);
		void *ad, *dl = dlopen(cPath, RTLD_LAZY | RTLD_LOCAL); /* try local first */
		if (!dl) dl = dlopen(cPath, RTLD_LAZY | RTLD_GLOBAL); /* try global if local failed */
		if (!dl) Rf_error("cannot load '%s' dynamically", cPath);
		fn = (void *(*)(void)) dlsym(dl, "create_audio_driver");
		if (!fn) fn = (void *(*)(void)) dlsym(dl, "_create_audio_driver");
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
		drv = (audio_driver_t*) ad;
		if (!drv) Rf_error("unable to initialize the audio driver");
		if (drv->length != sizeof(audio_driver_t)) Rf_error("the driver is incompatible with this version of the audio package");
		current_driver = drv;		
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
		load_default_audio_driver(0);
	if (TYPEOF(rate) == INTSXP || TYPEOF(rate) == REALSXP)
		fRate = (float) Rf_asReal(rate);
	audio_instance_t *p = current_driver->create_player(source, fRate, 0);
	if (!p) Rf_error("cannot start audio driver");
	p->driver = current_driver;
	p->kind = AI_PLAYER;
	SEXP ptr = R_MakeExternalPtr(p, R_NilValue, R_NilValue);
	Rf_protect(ptr);
	R_RegisterCFinalizer(ptr, audio_instance_destructor);
	Rf_setAttrib(ptr, R_ClassSymbol, Rf_mkString("audioInstance"));
	Rf_unprotect(1);
	return ptr;	
}

SEXP audio_recorder(SEXP source, SEXP rate, SEXP channels) {
	float fRate = -1.0;
	int chs = Rf_asInteger(channels);
	if (!current_driver)
		load_default_audio_driver(0);
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
	Rf_setAttrib(ptr, R_ClassSymbol, Rf_mkString("audioInstance"));
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

SEXP audio_driver_descr(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_mkString(p->driver->descr);
}

SEXP audio_instance_type(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarInteger(p->kind);
}

#if __WIN32__
#define millisleep(X) Sleep((DWORD)(((double)(X))*1000.0))
#else
static void millisleep(double tout) {
	struct timeval tv;
	tv.tv_sec  = (unsigned int) tout;
	tv.tv_usec = (unsigned int)((tout - ((double)tv.tv_sec)) * 1000000.0);
	select(0, 0, 0, 0, &tv);
}
#endif

static int fallback_wait(double timeout) {
	if (timeout < 0) timeout = 9999999.0; /* really a dummy high number */
	while (1) {
		/* use 100ms slices */
		double slice = (timeout > 0.1) ? 0.1 : timeout;
		if (slice <= 0.0) break;
		millisleep(slice);
		R_CheckUserInterrupt(); /* FIXME: we should adjust for time spent processing events */
		timeout -= slice;
	}
	return WAIT_TIMEOUT;
}

SEXP audio_instance_source(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return p->source;
}

SEXP audio_wait(SEXP instance, SEXP timeout) {
	if (instance == R_NilValue) { /* unlike other functions we allow NULL for a system-wide sleep without any event */
		if (current_driver && current_driver->wait) return Rf_ScalarInteger(current_driver->wait(NULL, Rf_asReal(timeout)));
		return Rf_ScalarInteger(fallback_wait(Rf_asReal(timeout)));
	}
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarInteger(p->driver->wait ? p->driver->wait(p, Rf_asReal(timeout)) : WAIT_ERROR);
}

SEXP audio_instance_address(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarInteger((int) (size_t) p);
}
