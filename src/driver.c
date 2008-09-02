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

audio_driver_t *current_driver;

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
void load_default_audio_driver()
{
#if HAS_WMM
  current_driver = wmmaudio_audio_driver;
  return;
#endif
#if HAS_AU
  current_driver = audiounits_audio_driver;
  return;
#endif
#if HAS_PA
  current_driver = portaudio_audio_driver;
  return;
#endif
  Rf_error("no audio drivers are available");
}

SEXP audio_player(SEXP source) {
	if (!current_driver)
		load_default_audio_driver();
	audio_instance_t *p = current_driver->create_player(source);
	if (!p) Rf_error("cannot start audio driver");
	p->driver = current_driver;
	p->kind = AI_PLAYER;
	SEXP ptr = R_MakeExternalPtr(p, R_NilValue, R_NilValue);
	Rf_protect(ptr);
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

SEXP audio_close(SEXP instance) {
	if (TYPEOF(instance) != EXTPTRSXP)
		Rf_error("invalid audio instance");
	audio_instance_t *p = (audio_instance_t *) EXTPTR_PTR(instance);
	if (!p) Rf_error("invalid audio instance");
	return Rf_ScalarLogical((p->driver)->close(p));
}

