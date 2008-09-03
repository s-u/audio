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
CPP=`"${RBIN}" CMD config CPP`;
CFLAGS=`"${RBIN}" CMD config CFLAGS`
CPPFLAGS=`"${RBIN}" CMD config CPPFLAGS`
if test -n "${PKG_CPPFLAGS}"; then
   CPPFLAGS="${CPPFLAGS} ${PKG_CPPFLAGS}"
fi

AC_PROG_CC

LIBS="${PKG_LIBS}"

# try AudioUnits
save_LIBS="${LIBS}"
LIBS="${LIBS} -framework AudioUnit -framework CoreServices"
AC_MSG_CHECKING([AudioUnits])
has_au=no
AC_LINK_IFELSE([[#include <AudioUnit/AudioUnit.h>
int
main ()
{
  Component comp = 0; 
  ComponentInstance outUnit;
  OpenAComponent(comp, &outUnit);
  AudioUnitInitialize(outUnit);
  return 0;
}
]], [has_au=yes
AC_DEFINE(HAS_AU, 1, [defined if AudioUnits are available])])
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
  AC_SEARCH_LIBS(Pa_Initialize, portaudio, [has_pa=yes
AC_DEFINE(HAS_PA, 1, [defined if PortAudio is available])])
])

# in any case configure produces config.h so we want to use it
CPPFLAGS="-DHAS_CONFIG_H=1 ${CPPFLAGS}"

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
