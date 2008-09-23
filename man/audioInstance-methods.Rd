\name{audioInstance-methods}
\alias{$.audioInstance}
\alias{close.audioInstance}
\alias{pause.audioInstance}
\alias{play.audioInstance}
\alias{print.audioInstance}
\alias{resume.audioInstance}
\alias{rewind.audioInstance}
\title{
  Audio instance class methods
}
\description{
  \code{audioInstance}s supports most methods relevant to them. See
  the corresponding generics help for details.

  Noteworthy is that \code{$data} is the canonical way to get data
  associated with an audio instance - i.e. played or recorded content.
}
\usage{
`$.audioInstance`(x, name)
resume.audioInstance(x, ...)
pause.audioInstance(x, ...)
rewind.audioInstance(x, ...)
close.audioInstance(con, ...)
play.audioInstance(x, ...)
print.audioInstance(x, ...)
}
\arguments{
  \item{x}{audio instance}
  \item{name}{name of the attribute - currently only "data" is supported}
  \item{con}{audio instance (the name is unfortuantely defined in the \code{close} generic like this)}
  \item{...}{ignored}
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
%\examples{
%x <- audioSample(sin(1:8000/10), 8000)
%x$rate
%x[1:10]
%}
\keyword{interface}
