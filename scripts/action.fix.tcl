bind filt - "\001ACTION *\001" filt_act
proc filt_act {idx text} {
  return ".me [string trim [join [lrange [split $text] 1 end]] \001]"
}

bind filt - "/me *" filt_telnet_act
proc filt_telnet_act {idx text} {
  return ".me [join [lrange [split $text] 1 end]]"
}
