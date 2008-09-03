print.audioInstance <- function(x, ...) {
  kind <- c("player", "recorder")[.Call("audio_instance_type", x, PACKAGE="audio")]
  info <- paste(" Audio ", kind, " instance ", sprintf('%x',.Call("audio_instance_address", x, PACKAGE="audio")), " of ", .Call("audio_driver_name", x, PACKAGE="audio"), ".\n", sep = '')
  cat(info)
  invisible(info)
}
