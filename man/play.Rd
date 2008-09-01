\name{play}
\alias{play}
\title{
  Play audio
}
\description{
  \code{play} plays audio
}
\usage{
play(x, rate=44100)
}
\arguments{
  \item{x}{data to play}
  \item{rate}{sample rate (in Hz)}
}
\value{
  Returns an audio object.
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
\examples{
play(sin(1:10000/1000))
}
\keyword{interface}
