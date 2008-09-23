\name{play}
\alias{play}
\alias{play.default}
\alias{play.audioSample}
\alias{play.Sample}
\title{
  Play audio
}
\description{
  \code{play} plays audio
}
\usage{
play(x, ...)
play.audioSample(x, rate, ...)
play.Sample(x, ...) 
play.default(x, rate=44100, ...)
}
\arguments{
  \item{x}{data to play}
  \item{rate}{sample rate - it is inferred from the object (where possible) if not specified}
  \item{...}{optional arguments passed to the method specific to the object being played}
}
\value{
  Returns an audio instance object which can be used to control the playback subsequently.
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
\examples{
\donttest{
play(sin(1:10000/20))
}
}
\keyword{interface}
