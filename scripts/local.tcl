#
#  This used to be a patch to the C code by answer, but it's better
#  suited to a Tcl script using the FILT binding.  it intercepts all
#  text starting with ' from party-line users, and sends that text
#  to local users only.  -robey
#
#  Modified again by answer to include various .me things in localmode...
#
# 'me jumps about excitedly!
# => user jumps about excitedly!
# 'me, tired of living, jumps off a bridge.
# => user, tired of living, jumps off a bridge.
# 'me'll never do that again!
# => user'll never do that again!
#
# actually, any '* will work...

bind filt - '* local_chat
proc local_chat {idx text} {
  set done 0
  set text [string range $text 1 end]
  set whom [idx2hand $idx]

  set strdex [string first " " $text]
  if { $strdex < 0 } {
    set done 1
    set whom "=$whom=>"
  }

  if { !$done && ([string compare [string range $text 0 2] "me "] == 0) } {
    set done 1
    set text [string range $text 3 end]
    set whom "=> $whom"
  }

  if { !$done && ([string compare [string range $text 0 2] "me'"] == 0) } {
    set done 1
    set strdex [string first " " $text]
    if { $strdex < 0 } { return 1 }
    set whom "=> $whom'[string range $text 3 [expr $strdex-1]]"
    set text [string range $text [expr $strdex+1] end]
  }

  if { !$done && ([string compare [string range $text 0 3] "me, "] == 0) } {
    set done 1
    set text [string range $text 4 end]
    set whom "=> $whom,"
  }

  if {!$done} {
    set done 1
    set whom "=$whom=>"
  }

  foreach user [dcclist] {
    if { ([lindex $user 3] == "chat") && ([getchan $idx] >= 0) &&
         (([lindex $user 0] != $idx) || ([echo $idx])) } {
        putdcc [lindex $user 0] "$whom $text"
    }
    if { ([lindex $user 3] == "files") &&
         (([lindex $user 0] != $idx) || ([echo $idx])) } {
        putdcc [lindex $user 0] "$whom $text"
    }
  }

  return
}
