load.wave <- function(where) invisible(.Call("load_wave_file", where))

save.wave <- function(what, where) invisible(.Call("save_wave_file", where, what))

