\name{record}
\alias{record}
\title{
  Record audio
}
\description{
  \code{record} record audio using the current audio device
}
\usage{
record(where, rate, channels) 
}
\arguments{
  \item{where}{object to record into or the number of samples to record}
  \item{rate}{sample rate. If ommitted it will be taken from the \code{where} object or default to 44100}
  \item{channels}{number of channels to record. If ommitted it will be taken from the \code{where} object or default to 2. Note that most devices only support 1 (mono) or 2 (stereo).}
}
\value{
  Returns an audio instance object which can be used to control the recording subsequently.
}
\details{
  The \code{record} function creates an audio instance of the current
  audio driver to start audio recording. The recording is performed
  asynchronously and the function returns immediately after the
  recording is started.

  \code{where} can either be a numeric vector of the storage mode
  `double' and length greater than 1 or a numberic vector of length
  one specifying the number of samples to record. The the former case
  the audio data is recorded directly to the vector, in the latter
  case a new object (initialized with \code{NA}) is created internally
  (and thus only accessible using \code{a$data} where \code{a} is the
  audio instance).

  The recording is automatically stopped after the \code{where} object
  has been completely filled. Nonetheless \code{\link{pause}} can be
  used to stop the recoding at any time.
}
%\seealso{
%  \code{\link{.jcall}}, \code{\link{.jnull}}
%}
\examples{
\donttest{
x <- rep(NA_real_, 16000)
# start recording into x
record(x, 8000, 1)
# monitor the recording progress
par(ask=FALSE) # for continuous plotting
while (is.na(x[length(x)])) plot(x, type='l', ylim=c(-1, 1))
# play the recorded audio
play(x)
}
}
\keyword{interface}
