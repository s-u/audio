# Process this file with autoconf to produce a configure script.
AC_INIT(audio, 0.1, Simon.Urbanek@r-project.org)
AC_CONFIG_SRCDIR([src/driver.c])
AC_CONFIG_HEADER([src/config.h])

# find R home and set CC/CFLAGS
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
  echo "could not determine R_HOME"
  exit 1
fi
RBIN="${R_HOME}/bin/R"
CC=`"${RBIN}" CMD config CC`;
## since R 3.6 R CMD config doesn't support CPP
CPP="${CC} -E"
CFLAGS=`"${RBIN}" CMD config CFLAGS`
CPPFLAGS=`"${RBIN}" CMD config CPPFLAGS`
if test -n "${PKG_CPPFLAGS}"; then
   CPPFLAGS="${CPPFLAGS} ${PKG_CPPFLAGS}"
fi

AC_LANG(C)
AC_PROG_CC

LIBS="${PKG_LIBS}"

# try AudioUnits
save_LIBS="${LIBS}"
LIBS="${LIBS} -framework AudioUnit -framework CoreServices"
AC_MSG_CHECKING([AudioUnits])
has_au=no
AC_LINK_IFELSE([AC_LANG_SOURCE([#include <AudioUnit/AudioUnit.h>
int
main ()
{
  Component comp = 0; 
  ComponentInstance outUnit;
  OpenAComponent(comp, &outUnit);
  AudioUnitInitialize(outUnit);
  return 0;
}
])], [has_au=yes
AC_DEFINE(HAS_AU, 1, [defined if AudioUnits are available])],
[## try with new API that removes Components
AC_LINK_IFELSE([AC_LANG_SOURCE([#include <AudioUnit/AudioUnit.h>
int
main ()
{
  AudioComponent comp = 0;
  AudioComponentInstance outUnit;
  AudioComponentInstanceNew(comp, &outUnit);
  AudioUnitInitialize(outUnit);
  return 0;
}
])], [has_au=yes
AC_DEFINE(HAS_AU, 1, [defined if AudioUnits are available])
AC_DEFINE(NEED_AUDIO_PREFIX, 1, [defined if AU Components need Audio prefix])],
[LIBS="${save_LIBS}"])
])
AC_MSG_RESULT([${has_au}])

# find PortAudio (first try pkg-config)
: ${PKGCONFIG=pkg-config}
if ${PKGCONFIG} --version >/dev/null 2>&1; then
   if ${PKGCONFIG} portaudio-2.0; then
      CPPFLAGS="${CPPFLAGS} `${PKGCONFIG} --cflags portaudio-2.0`"
      LIBS="${CPPFLAGS} `${PKGCONFIG} --libs portaudio-2.0`"
   fi
fi

AC_CHECK_HEADERS([dlfcn.h])
AC_SEARCH_LIBS(dlsym, dl, [AC_DEFINE(HAS_DLSYM, 1, [defined if the system supports dlsym])])

has_pa=no
AC_CHECK_HEADERS([portaudio.h],[
  AC_SEARCH_LIBS(Pa_Initialize, portaudio, [
AC_MSG_CHECKING([for working PortAudio (API>=2)])
AC_LINK_IFELSE([AC_LANG_SOURCE([#include "portaudio.h"
static int paPlayCallback(const void *inputBuffer, void *outputBuffer,
 unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
 PaStreamCallbackFlags statusFlags, void *userData )
{ return 1; };
int
main ()
{
  PaStream *stream;
  PaError err = Pa_Initialize();
  err = Pa_OpenDefaultStream(&stream,0,1,paFloat32,44100.0,1024,paPlayCallback,0);
  return 0;
}
])], [has_pa=yes
AC_MSG_RESULT([yes])
AC_DEFINE(HAS_PA, 1, [defined if PortAudio is available])
  ], [AC_MSG_RESULT([no])])
  ])
])

# in any case configure produces config.h so we want to use it
CPPFLAGS="-DHAS_CONFIG_H=1 ${CPPFLAGS}"

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
