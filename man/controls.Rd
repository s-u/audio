\name{controls}
\alias{pause}
\alias{rewind}
\alias{resume}
\title{
  Control audio instance
}
\description{
  \code{pause} pauses (stops) audio recording or playback

  \code{rewind} rewinds audio recording or playback, i.e., makes the
  beginning of the source (or target) object the current audio
  position.

  \code{resume} resumes previously paused audio recording or playback
}
\usage{
pause(x, ...)
rewind(x, ...)
resume(x, ...)
}
\arguments{
  \item{x}{instance object}
  \item{...}{optional arguments passed to the method specific to the object}
}
\value{
  All functions return TRUE on success and FALSE on failure. All
  methods are generics and intended to apply to similar asynchronous
  operations (e.g. movie playback etc.).
}
\seealso{
  \code{\link{play}}, \code{\link{record}}
}
%\examples{
%play(sin(1:10000/20))
%}
\keyword{interface}
