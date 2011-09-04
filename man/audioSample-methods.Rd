\name{audioSample-methods}
\alias{$.audioSample}
\alias{$<-.audioSample}
\alias{[.audioSample}
\alias{as.Sample.audioSample}
\alias{print.audioSample}
\title{
  Audio sample class methods
}
\description{
  \code{audioSample} methods behave in the same way as the underlying
  methods of numeric vectors and matrices except that they preserve
  the attributes and class of the objects.
}
\usage{
  \method{$}{audioSample}(x, name)
  \method{$}{audioSample}(x, name) <- value
  \method{[}{audioSample}(x, \dots, drop = FALSE)
  \method{as.Sample}{audioSample}(x, ...) 
  \method{print}{audioSample}(x, ...)
}
\arguments{
  \item{x}{sample object}
  \item{name}{name of the attribute to get/set}
  \item{value}{value to set}
  \item{drop}{see \code{`[`} operator documentation}
  \item{\dots}{parameters passed to the object-specific method}
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
\examples{
x <- audioSample(sin(1:8000/10), 8000)
x$rate
x[1:10]
}
\keyword{interface}
