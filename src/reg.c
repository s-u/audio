#include <Rinternals.h>
#include <R_ext/Rdynload.h>

/* all this is already done by the NAMESPACE so the folowing is
   merely to keep the ignorant R CMD check happy. */
void R_init_audio_(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, NULL, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
