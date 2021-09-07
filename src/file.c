/* WAVE file manipulation for R
   audio R package
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

#include <stdio.h>
#include <string.h>

#define R_NO_REMAP      /* to not pollute the namespace */

#include <R.h>
#include <Rinternals.h>

/* WAVE file is essentially a RIFF file, hence the structures */

typedef struct riff_header {
	char riff[4]; /* RIFF */
	unsigned int len;
	char type[4]; /* file type (WAVE) for wav */
} riff_header_t;

typedef struct riff_chunk {
	char rci[4];
	unsigned int len;
} riff_chunk_t;

typedef struct wav_fmt {
	char rci[4]; /* RIFF chunk identifier, "fmt " here */
	unsigned int len;
	short ver, chs;
	unsigned int rate, bps;
	unsigned short byps, bips;
} wav_fmt_t;

SEXP load_wave_file(SEXP src)
{
	if (Rf_inherits(src, "connection"))
		Rf_error("sorry, connections are not supported yet");
	if (TYPEOF(src) != STRSXP || LENGTH(src) < 1)
		Rf_error("invalid file name");
	{
		const char *fName = CHAR(STRING_ELT(src, 0));
		FILE *f = fopen(fName, "rb");
		riff_header_t rh;
		wav_fmt_t fmt;
		riff_chunk_t rc;
		unsigned int to_go = 0, has_fmt = 0;
		SEXP res = R_NilValue;
		
		if (!f)
			Rf_error("unable to open file '%s'", fName);
		if (fread(&rh, sizeof(rh), 1, f) != 1) {
			fclose(f);
			Rf_error("unable to read header from '%s'", fName);
		}
		if (memcmp(rh.riff, "RIFF", 4) || memcmp(rh.type, "WAVE", 4)) {
			fclose(f);
			Rf_error("not a WAVE format");
		}
		to_go = rh.len;
		while (!feof(f) && to_go >= 8) {
			int n = fread(&rc, 1, 8, f);
			if (n < 8) {
				fclose(f);
				Rf_error("incomplete file");
			}
			to_go -= n;
			if (!memcmp(rc.rci, "fmt ", 4)) { /* format chunk */
				if (to_go < 16) {
					fclose(f);
					Rf_error("corrupt file");
				}
				memcpy(&fmt, &rc, 8);
				n = fread(&fmt.ver, 1, 16, f);
				if (n < 16) {
					fclose(f);
					Rf_error("incomplete file");
				}
				to_go -= n;
				has_fmt = 1;
			} else if (!memcmp(rc.rci, "data", 4)) {
				unsigned int samples = rc.len;
				unsigned int st = 1;
				double *d;
				if (!has_fmt) {
					fclose(f);
					Rf_error("data chunk without preceeding format chunk");
				}
				if (fmt.bips == 16) {
					st = 2; samples /= 2;
				} else if (fmt.bips == 32) {
					st = 4; samples /= 4;
				} else if (fmt.bips != 8) {
					fclose(f);
					Rf_error("unsupported smaple width: %d bits", fmt.bips);
				}
				res = Rf_allocVector(REALSXP, samples);
				n = fread(d = REAL(res), st, samples, f);
				if (n < samples) {
					fclose(f);
					Rf_error("incomplete file");
				}
				/* now convert the format to doubles, in-place */
				{
					int i = n - 1;
					switch (st) {
						case 1:
					    {
						    signed char *ca = (signed char*) REAL(res);
						    while (i >= 0) {
							    signed char c = ca[i];
							    d[i--] = (c < 0)?(((double) c) / 127.0) : (((double) c) / 128.0);
						    }
					    }
						case 2:
					    {
						    short int *sa = (short int*) REAL(res);
						    while (i >= 0) {
							    short int s = sa[i];
							    d[i--] = (s < 0)?(((double) s) / 32767.0) : (((double) s) / 32768.0);
						    }
					    }
						case 4:
					    {
						    int *sa = (int*) REAL(res);
						    while (i >= 0) {
							    int s = sa[i];
							    d[i--] = (s < 0)?(((double) s) / 2147483647.0) : (((double) s) / 2147483648.0);
						    }
					    }
					}
				}
				n *= st;
				if (n > to_go) /* it's questionable whether we should bark here - it's file inconsistency to say the least */
					to_go = 0;
				else
					to_go -= n;
			} else { /* skip any chunks we don't know */
				if (rc.len > to_go || fseek(f, rc.len, SEEK_CUR)) {
					fclose(f);
					Rf_error("incomplete file");
				}
				to_go -= rc.len;
			}
		}
		fclose(f);
		Rf_protect(res);
		{
			Rf_setAttrib(res, Rf_install("rate"), Rf_ScalarInteger(fmt.rate));
			Rf_setAttrib(res, Rf_install("bits"), Rf_ScalarInteger(fmt.bips));
			Rf_setAttrib(res, Rf_install("class"), Rf_mkString("audioSample"));
			if (fmt.chs > 1) {
				SEXP dim = Rf_allocVector(INTSXP, 2);
				INTEGER(dim)[0] = fmt.chs;
				INTEGER(dim)[1] = LENGTH(res) / fmt.chs;
				Rf_setAttrib(res, R_DimSymbol, dim);
			}
		}
		Rf_unprotect(1);
		return res;
	}
}

SEXP save_wave_file(SEXP where, SEXP what) {
	unsigned int size = LENGTH(what) * 2; /* use 16 bits by default */
	unsigned int rate = 44100;
	unsigned int chs = 1;
	unsigned int bits = 16, bps = 2;
	
	SEXP dim = Rf_getAttrib(what, R_DimSymbol);
	if (TYPEOF(dim) == INTSXP && LENGTH(dim) > 1 && INTEGER(dim)[0] == 2) chs = 2;
	dim = Rf_getAttrib(what, Rf_install("bits"));
	if (TYPEOF(dim) == INTSXP || TYPEOF(dim) == REALSXP) {
		int b = Rf_asInteger(dim);
		if (b == 8) { size /= 2; bits = 8; bps = 1; } else if (b == 32) { size *= 2; bits = 32; bps = 4; }
	}
	bps *= chs;
	dim = Rf_getAttrib(what, Rf_install("rate"));
	if (TYPEOF(dim) == INTSXP || TYPEOF(dim) == REALSXP)
		rate = Rf_asInteger(dim);
	if (TYPEOF(what) != REALSXP)
		Rf_error("saved object must be in real form");
	
	if (Rf_inherits(where, "connection"))
		Rf_error("sorry, connections are not supported yet");
	if (TYPEOF(where) != STRSXP || LENGTH(where) < 1)
		Rf_error("invalid file name");
	
	{
		const char *fName = CHAR(STRING_ELT(where, 0));
		riff_header_t rh = { "RIFF", size + 36, "WAVE" };
		wav_fmt_t fmt = { "fmt ", 16, 1, chs, rate, rate * bps, bps, bits };
		riff_chunk_t rc = { "data", size };
		FILE *f = fopen(fName, "wb");
		if (!f)
			Rf_error("unable to create file '%s'", fName);
		if (fwrite(&rh, sizeof(rh), 1, f) != 1 ||
		    fwrite(&fmt, sizeof(fmt), 1, f) != 1 ||
		    fwrite(&rc, sizeof(rc), 1, f) != 1) {
			fclose(f);
			Rf_error("write error");
		}
		{
			if (bits == 8) {
				double *d = REAL(what);
				signed char buf[2048];
				int i = 0, j = LENGTH(what), k = 0;
				while (i < j) {
					buf[k++] = (signed char) (d[i++] * 127.0);
					if (k == 2048) {
						if (fwrite(buf, sizeof(*buf), k, f) != k) {
							fclose(f);
							Rf_error("write error");
						}
						k = 0;
					}
				}
				if (k && fwrite(buf, sizeof(*buf), k, f) != k) {
					fclose(f);
					Rf_error("write error");
				}
			} else if (bits == 16) {
				double *d = REAL(what);
				short int buf[2048];
				int i = 0, j = LENGTH(what), k = 0;
				while (i < j) {
					buf[k++] = (short int) (d[i++] * 32767.0);
					if (k == 2048) {
						if (fwrite(buf, sizeof(*buf), k, f) != k) {
							fclose(f);
							Rf_error("write error");
						}
						k = 0;
					}
				}
				if (k && fwrite(buf, sizeof(*buf), k, f) != k) {
					fclose(f);
					Rf_error("write error");
				}
			} else {
				double *d = REAL(what);
				int buf[2048];
				int i = 0, j = LENGTH(what), k = 0;
				while (i < j) {
					buf[k++] = (int) (d[i++] * 2147483647.0);
					if (k == 2048) {
						if (fwrite(buf, sizeof(*buf), k, f) != k) {
							fclose(f);
							Rf_error("write error");
						}
						k = 0;
					}
				}
				if (k && fwrite(buf, sizeof(*buf), k, f) != k) {
					fclose(f);
					Rf_error("write error");
				}
			}
		}
		fclose(f);
	}
	return R_NilValue;
}
