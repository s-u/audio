load.audio.driver <- function(path) .Call("audio_load_driver", as.character(path), PACKAGE="audio")

