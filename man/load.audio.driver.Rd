\name{load.audio.driver}
\alias{load.audio.driver}
\title{
  Audio drivers
}
\description{
  \code{load.dudio.driver} attempts to load a modular audio driver and,
  if succeessful, makes it the current audio driver.

}
\usage{
load.dudio.driver(path)
}
\arguments{
  \item{path}{path to the dynamic module to load}
}
\value{
  Returns the name of the driver.
}
\details{
  The audio package comes with several built-in audio drivers (WinMM
  for Windows, AudioUnits for Mac OS X and PortAudio for unix), but it
  also supports 3rd-party drivers to be loaded (e.g. from other
  packages).

  An audio driver is any shared module that provides a C function
  \code{create_audio_driver} which returns a pointer to a populated
  structure \code{audio_driver} as defined in \code{driver.h}. Once an
  audio driver is loaded, it will be used for all subsequent audio
  commands.
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
%\examples{
%play(sin(1:10000/20))
%}
\keyword{interface}
