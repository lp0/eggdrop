bind filt - "\001ACTION *\001" filt_act
proc filt_act {idx text} {
  dccsimul $idx ".me [string trim [lrange $text 1 end] \001]"
}
