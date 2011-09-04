\name{wait}
\alias{wait}
\alias{wait.default}
\alias{wait.audioInstance}
\title{
  Wait for an event
}
\description{
  \code{wait} waits until an event occurs or times out
}
\usage{
wait(x, \dots)
\method{wait}{default}(x, timeout, \dots)
\method{wait}{audioInstance}(x, timeout=NA, \dots)
}
\arguments{
  \item{x}{event to wait for}
  \item{timeout}{longest period to wait for (in seconds, real number). A value of 0 causes \code{wait} to just check for the event, values \code{NA} and less than zero mean to wait indefinitely until the even occurs.}
  \item{\dots}{optional arguments passed to the method specific to the object being monitored}
}
\value{
  Returns the result.
}
\details{
  The default method allows \code{x} to specify the timeout, i.e., if \code{timeout} is not specified and \code{x} is numeric then the timeout is set to \code{x}.
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
\examples{
\donttest{
# play a sound and wait until the playback is done
wait(play(sin(1:10000/20)))
# wait for 2.5 seconds unconditionally
wait(2.5)
}
}
\keyword{interface}
