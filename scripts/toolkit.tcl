#
# some Tcl procs that might be useful for script packages:
# (some of these require v0.9r or later)
#
# newflag <flag>
#   creates a new flag in the next empty slot.  on success it returns
#   1.  if all the user-defined flags are full (currently there are 10
#   available), or if the flag you're asking for is invalid or already
#   being used, it returns 0.
#
# user-set <handle> <key> <data>
#   stores data about a user in the 'xtra' field of the user record.
#   for example:
#     userstore robey points 5
#   puts "5" under "points" for robey.  there's a limited amount of
#   space in the 'xtra' field for a user record so don't go crazy.
#
# user-get <handle> <key>
#   gets data that was previously stored with 'userstore'.  if there
#   was no info stored under that key, a blank string is returned.
#
# putmsg <nick> <text>
#   sends a message to someone on irc
#
# putnotc <nick> <text>
#   sends a notice to someone on irc
#
# putchan <channel> <text>
#   sends a public message to a channel
#
# putact <channel> <text>
#   does a public action to a channel
#

set toolkit_loaded 1

proc newflag {flag} {
  foreach i {1 2 3 4 5 6 7 8 9 0} {
    global flag$i
    if {[eval set flag$i] == $i} {
      set flag$i $flag
      if {[eval set flag$i] != $flag} { return 0 }
      return 1
    }
  }
  return 0
}

proc user-get {handle key} {
  set xtra [getxtra $handle]
  for {set i 0} {$i < [llength $xtra]} {incr i} {
    set this [lindex $xtra $i]
    if {[string compare [lindex $this 0] $key] == 0} {
      return [lindex $this 1]
    }
  }
  return ""
}

proc user-set {handle key data} {
  set xtra [getxtra $handle]
  # is key already there?
  for {set i 0} {$i < [llength $xtra]} {incr i} {
    set this [lindex $xtra $i]
    if {[string compare [lindex $this 0] $key] == 0} {
      set this [list $key $data]
      setxtra $handle [lreplace $xtra $i $i $this]
      return
    }
  }
  lappend xtra [list $key $data]
  setxtra $handle $xtra
}

proc putmsg {nick text} {
  putserv "PRIVMSG $nick :$text"
}

proc putnotc {nick text} {
  putserv "NOTICE $nick :$text"
}

proc putchan {chan text} {
  putserv "PRIVMSG $chan :$text"
}

proc putact {chan text} {
  putserv "PRIVMSG $chan :\001ACTION $text\001"
}
