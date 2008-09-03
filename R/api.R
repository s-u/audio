play <- function(x, ...) UseMethod("play")
pause <- function(x, ...) UseMethod("pause")
resume <- function(x, ...) UseMethod("resume")
rewind <- function(x, ...) UseMethod("rewind")
record <- function(x, ...) stop("recording is not yet supported")

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

