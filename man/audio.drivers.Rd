\name{audio.drivers}
\alias{audio.drivers}
\alias{load.audio.driver}
\alias{set.audio.driver}
\alias{current.audio.driver}
\title{
  Audio Drivers
}
\description{
  \code{audio.drivers} lists all currently loaded and available audio
  drivers.

  \code{current.audio.driver} returns the name of the currently active
  audio driver or \code{NULL} if no audio drivers is available.

  \code{set.audio.driver} selects an audio driver as the current
  driver.

  \code{load.audio.driver} attempts to load a modular audio driver and,
  if succeessful, makes it the current audio driver.
}
\usage{
audio.drivers()
current.audio.driver()
set.audio.driver(name)
load.audio.driver(path)
}
\arguments{
  \item{name}{name of the driver to load (as it appears in the
  \code{name} column of \code{audio.drivers()}) or \code{NULL} to load
  the default audio driver}
  \item{path}{path to the dynamic module to load}
}
\value{
  \code{audio.drivers} returns a data frame lising all availbale
  drivers

  \code{set.audio.driver} and \code{current.audio.driver} return the
  name of the active driver or \code{NULL} if no drivers ar avaliable.

  \code{load.audio.driver} returns the name of the loaded driver.
}
\details{
  The audio package comes with several built-in audio drivers
  (currently "wmm": WindowsMultiMedia for MS Windows, "macosx":
  AudioUnits for Mac OS X and "portaudio": PortAudio for unix), but it
  also supports 3rd-party drivers to be loaded (e.g. from other
  packages).

  All operations that create new audio instances (\code{\link{play}}
  and \code{\link{record}}) use the current audio driver. The audio
  package allows the user to switch between different audio
  drivers. Each audio instance is tied to the driver it was created
  with even if the current driver was changed in the meantime.

  Drivers are references by its short name listed in the \code{name}
  column in the list of available drivers (obtainable via
  \code{audio.drivers}).

  An audio driver is any shared module that provides a C function
  \code{create_audio_driver} which returns a pointer to a populated
  structure \code{audio_driver} as defined in \code{driver.h}.
}
\seealso{
  \code{\link{record}}, \code{\link{play}}
}
\examples{
audio.drivers()
}
\keyword{interface}
