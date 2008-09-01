play <- function(x, rate=44100) {
 a = .Call("audio_player", x, PACKAGE="audio")
 .Call("audio_start", a, PACKAGE="audio")
 a
}
