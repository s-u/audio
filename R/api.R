play <- function(x, ...) UseMethod("play")
pause <- function(x, ...) UseMethod("pause")
resume <- function(x, ...) UseMethod("resume")
rewind <- function(x, ...) UseMethod("rewind")

record <- function(where, rate, channels) {
  if (missing(rate)) {
    rate <- attr(where, "rate", TRUE)
    if (is.null(rate)) rate <- 44100
  }
  if (missing(channels))
    channels <- if (is.null(dim(where))) 2 else dim(where)[1]
  channels <- as.integer(channels)
  if (length(channels) != 1 || (channels != 1 && channels != 2))
    stop("channels must be 1 (mono) or 2 (stereo)")
  if (length(where) == 1) where <- if (channels == 2) matrix(NA_real_, 2, where) else rep(NA_real_, where)
  a <- .Call("audio_recorder", where, as.double(rate), as.integer(channels), PACKAGE="audio")
  .Call("audio_start", a, PACKAGE="audio")
  invisible(a)
}

pause.audioInstance <- function(x, ...)
  invisible(.Call("audio_pause", x, PACKAGE="audio"))

resume.audioInstance <- function(x, ...)
  invisible(.Call("audio_resume", x, PACKAGE="audio"))

rewind.audioInstance <- function(x, ...)
  invisible(.Call("audio_rewind", x, PACKAGE="audio"))

close.audioInstance <- function(x, ...)
  invisible(.Call("audio_close", x, PACKAGE="audio"))

play.default <- function(x, rate=44100, ...) {
  a <- .Call("audio_player", x, rate, PACKAGE="audio")
  .Call("audio_start", a, PACKAGE="audio")
  invisible(a)
}

play.Sample <- function(x, ...) play(x$sound, x$rate)

play.audioSample <- function(x, rate, ...) {
  if (missing(rate)) rate <- attr(x, "rate", TRUE)
  play.default(x, rate, ...)
}

`[.audioSample` <- function(x, ..., drop = FALSE) {
  y <- NextMethod("[")
  attr(y, "rate") <- attr(x, "rate", TRUE)
  attr(y, "bits") <- attr(x, "bits", TRUE)
  class(y) <- class(x)
  y
}

play.audioInstance <- function(x, ...) stop("you cannot play an audio instance - try play(a$data) if a is a recorded instance")

`$.audioInstance` <- function(x, name) if (isTRUE(name == "data")) .Call("audio_instance_source", x) else NULL

`$.audioSample` <- function(x, name) attr(x, name)
`$<-.audioSample` <- function(x, name, value) .Primitive("attr<-")
