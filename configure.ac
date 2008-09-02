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

: ${PKGCONFIG=pkg-config}
if ${PKGCONFIG} --version >/dev/null 2>&1; then
   if ${PKGCONFIG} portaudio-2.0; then
      CPPFLAGS="${CPPFLAGS} `${PKGCONFIG} --cflags portaudio-2.0`"
      LIBS="${CPPFLAGS} `${PKGCONFIG} --libs portaudio-2.0`"
   fi
fi

AC_CHECK_HEADERS([portaudio.h],[
  AC_SEARCH_LIBS(Pa_Initialize, portaudio, [AC_DEFINE(HAS_PA, 1, [defined if PortAudio is available])])
])

CPPFLAGS="-DHAS_CONFIG_H=1 ${CPPFLAGS}"

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
