\name{wave}
\alias{load.wave}
\alias{save.wave}
\title{
  WAVE file manipulations
}
\description{
  \code{load.wave} loads a sample from a WAVE file

  \code{save.wave} saves a sample into a WAVE file
}
\usage{
load.wave(where)
save.wave(what, where)
}
\arguments{
  \item{where}{file name of the file to load from or save to}
  \item{what}{audioSample object to save}
}
\value{
  \code{load.wave} returns an object of the class \code{audioSample} as loaded from the WAVE file

  \code{save.wave} always returns \code{NULL}
}
\details{
  WAVE is a RIFF (Resource Interchange File Format) widely used for
  storage of uncompressed audio data. It is often identified by the
  extension .WAV on DOS-legacy systems (such as Windows). Although
  WAVE files may contain compressed data, the above functions only
  support plain, uncompressed PCM data.
}
\seealso{
  \code{\link{audioSample}}, \code{\link{play}}, \code{\link{record}}
}
%\examples{
%\donttest{
%play(sin(1:10000/20))
%}
%}
\keyword{interface}
