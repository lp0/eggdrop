#
#  wire v1.33
#    secondary encrypted communications on a botnet
#    based on wire 1.0 by Robey Pointer
#    11 September 1996 by Gord-@saFyre
#    updated 17 September 1996 by Gord-@saFyre
#    updated 31 October 1996 by Gord-@saFyre
#    updated 4 November 1996 by Gord-@saFyre
#
# requires at least eggdrop v1.0i+
#
# to initiate a secure communications link, decide on
# a common password between you and the people you want
# to talk to, then issue the command ".wire <password>"
# all lines starting with a semi-colon ";" will go
# to the wire.
#
# .wire <encrypt-key>   initiates a wire session
#
# .onwire    will return the handle@bot of all other
#            users on the same wire
#

# initialize arrays
# idx -> encrypt-key
set wire(*) "*"
# $wire_cmd -> idx list
set wired(*) "*"

if {[lindex $version 1] < 1081} {
  putlog "* Can't load wire -- needs at least v1.0h+"
} {
  bind dcc - wire do_wire
  bind filt - ";*" filt_wire
  bind chof - * chof_wire
  bind dcc - onwire onwire
  bind fil - onwire onwire
}

# join the wire
proc do_wire {hand idx arg} {
  global wire wired nick
  if {$arg == ""} {
    putdcc $idx "Usage: .wire <encrypt-key>|OFF"
    if {[info exists wire($idx)]} {
      putdcc $idx "You are currently on wire '$wire($idx)'."
    }
    return 0
  }
  if {($arg == "off") || ($arg == "OFF")} {
    if {![info exists wire($idx)]} {
      putdcc $idx "That's nice, but you weren't on the wire anyway."
      return 0
    }
    set wire_cmd "!wire[encrypt $wire($idx) "wire"]"
    set x [encrypt $wire($idx) "$hand left the wire."]
    putwire $wire_cmd $idx "----- " $x
    putallbots "$wire_cmd $nick $x"
    unset wire($idx)
    set wired($wire_cmd) [lreplace $wired($wire_cmd) [lsearch $wired($wire_cmd) $idx] [lsearch $wired($wire_cmd) $idx]]
    if {![llength $wired($wire_cmd)]} {
      unset wired($wire_cmd)
      unbind bot - $wire_cmd bot_wire
    }
    putdcc $idx "You are now off the wire."
    return 0
  }
  if {[info exists wire($idx)]} {
    putdcc $idx "Changing encryption key ..."
    set wire_cmd "!wire[encrypt $wire($idx) "wire"]"
    set x [encrypt $wire($idx) "$hand left the wire."]
    putwire $wire_cmd $idx "----- " $x
    putallbots "$wire_cmd $nick $x"
    unset wire($idx)
    set wired($wire_cmd) [lreplace $wired($wire_cmd) [lsearch $wired($wire_cmd) $idx] [lsearch $wired($wire_cmd) $idx]]
    if {![llength $wired($wire_cmd)]} {
      unset wired($wire_cmd)
      unbind bot - $wire_cmd bot_wire
    }
  }
  set wire($idx) $arg
  set wire_cmd "!wire[encrypt $wire($idx) "wire"]"
  if {[info exists wired($wire_cmd)]} {
    set wired($wire_cmd) [lappend wired($wire_cmd) $idx]
  } {
    set wired($wire_cmd) $idx
    bind bot - $wire_cmd bot_wire
  }  
  putdcc $idx "----- All text starting with ; will now go over the wire."
  putdcc $idx "----- To see who's on your wire, type '.onwire'."
  putdcc $idx "----- To leave, type '.wire off'."
  set x [encrypt $wire($idx) "$hand joined wire '$wire($idx)'."]
  putwire $wire_cmd $idx "----- " $x
  putallbots "$wire_cmd $nick $x"
  onwire $hand $idx $arg
  return 0
}

proc filt_wire {idx text} {
  global wire nick
  if {![info exists wire($idx)]} { return 0 }
  set wire_cmd "!wire[encrypt $wire($idx) "wire"]"
  set hand [idx2hand $idx]
  if {([string compare [string tolower [string range $text 1 2]] "me"] == 0) && ![isalphanum [string index $text 3]]} {
    set text [string range $text 2 end]
    set inhand "----- > $hand"
    set outhand "!${hand}@$nick"
  } {
    set inhand "----- <$hand> "
    set outhand "${hand}@$nick"
  }
  set x [encrypt $wire($idx) [string range $text 1 end]]
  putwire $wire_cmd $idx $inhand $x
  putallbots "$wire_cmd $outhand $x"
  return 1
}

# people vanishing who might still be on the wire
proc chof_wire {hand idx} {
  global wired wire nick
  if {[info exists wire($idx)]} {
    set wire_cmd "!wire[encrypt $wire($idx) "wire"]"
    set x [encrypt $wire($idx) "$hand left the wire."]
    putwire $wire_cmd $idx "----- " $x
    putallbots "$wire_cmd $nick $x"
    unset wire($idx)
    set wired($wire_cmd) [lreplace $wired($wire_cmd) [lsearch $wired($wire_cmd) $idx] [lsearch $wired($wire_cmd) $idx]]
    if {![llength $wired($wire_cmd)]} {
      unset wired($wire_cmd)
      unbind bot - $wire_cmd bot_wire
    }
  }
}

# who else is on the same wire with you?
proc onwire {handle idx arg} {
  global wire wired nick version
  if {![info exists wire($idx)]} {
    putdcc $idx "You aren't on a wire."
    return 0 
  }
  set wire_cmd "!wire[encrypt $wire($idx) "wire"]"
  set x [encrypt $wire($idx) $handle]
  putdcc $idx "----- Currently on wire '$wire($idx)':"
  putdcc $idx "----- Nick       Bot        Host"
  putdcc $idx "----- ---------- ---------- ------------------------------"
  putallbots "$wire_cmd !wirereq $idx $x"
  foreach j [dcclist] {
    append jlist " [lindex $j 0]"
  }
  foreach i $wired($wire_cmd) {
    if {($i != "*") && ([lsearch $jlist $i] != -1)} {
      set hand [idx2hand $i]
      set attr " "
      if {[matchattr $hand o]} {set attr "@"}
      if {[matchattr $hand m]} {set attr "+"}
      if {[matchattr $hand n]} {set attr "*"}
      set outline "----- ${attr}${hand}"
      for {set r [string length $hand]} {$r < 10} {incr r} {
        append outline " "
      }
      append outline $nick
      for {set r [string length $nick]} {$r < 11} {incr r} {
        append outline " "
      }
      foreach j [dcclist] {
        if {[lindex $j 0] == $i} {
          append outline [lindex $j 2]
          break
         }
      }
      if {[getdccidle $i] >= 300} {
        set idletime [expr [getdccidle $i] / 60]
        if {$idletime >= 60} {
          set idlehour [expr $idletime / 60]
          set idlemin [expr $idletime % 60]
          if {$idlehour >= 24} {
            set idleday [expr $idlehour / 24]
            set idlehour2 [expr $idlehour % 24]
            set idlehour "${idleday}d${idlehour2}"
          }
          set idletime "${idlehour}h${idlemin}"
        }
        append outline " \[idle ${idletime}m\]"
      }
      putdcc $idx $outline
      if {([lindex $version 1] >= 1094) && ([getdccaway $i] != "")} {
        putdcc $idx "-----    AWAY: [getdccaway $i]"
      }
    }
  }
}

# incoming from other bots
proc bot_wire {from cmd text} {
  global wire wired nick version
  set header [lindex $text 0]
  set encrypted [lindex $text 1]
  if {[string first @ $header] == -1} {
    if {$header == "!wirereq"} {
      set fidx [lindex $text 1]
      set fhand [lindex $text 2]
      if {![info exists wired($cmd)]} {return 0}
      set jlist "*"
      foreach j [dcclist] {
        append jlist " [lindex $j 0]"
      }
      foreach i $wired($cmd) {
        if {($i != "*") && ([lsearch $jlist $i] != -1)} {
          set hand [idx2hand $i]
          set attr " "
          if {[matchattr $hand o]} {set attr "@"}
          if {[matchattr $hand m]} {set attr "+"}
          if {[matchattr $hand n]} {set attr "*"}
          set outline "----- ${attr}${hand}"
          for {set r [string length $hand]} {$r < 10} {incr r} {
            append outline " "
          }
          append outline $nick
          for {set r [string length $nick]} {$r < 11} {incr r} {
            append outline " "
          }
          foreach j [dcclist] {
            if {[lindex $j 0] == $i} {
              append outline [lindex $j 2]
              break
             }
          }
          if {[getdccidle $i] >= 300} {
            set idletime [expr [getdccidle $i] / 60]
            if {$idletime >= 60} {
              set idlehour [expr $idletime / 60]
              set idlemin [expr $idletime % 60]
              if {$idlehour >= 24} {
                set idleday [expr $idlehour / 24]
                set idlehour2 [expr $idlehour % 24]
                set idlehour "${idleday}d${idlehour2}"
              }
              set idletime "${idlehour}h${idlemin}"
            }
            append outline " \[idle ${idletime}m\]"
          }
          set x [encrypt $wire($i) $outline]
          putbot $from "$cmd !wireresp $fidx $fhand $x"
          if {([lindex $version 1] >= 1094) && ([getdccaway $i] != "")} {
            set x [encrypt $wire($i) "-----    AWAY: [getdccaway $i]"]
            putbot $from "$cmd !wireresp $fidx $fhand $x"
          }
        }
      }
      return 0
    }
    if {$header == "!wireresp"} {
      set fidx [lindex $text 1]
      set xhand [lindex $text 2]
      set xresp [lindex $text 3]
      if {![info exists wired($cmd)]} {return 0}
      if {[lsearch -exact $wired($cmd) $fidx] == -1} {return 0}
      if {[idx2hand $fidx] != [decrypt $wire($fidx) $xhand]} {return 0}
      putdcc $fidx [decrypt $wire($fidx) $xresp]
      return 0
    }
    set header "($header) "
  } {
    if {[string index $header 0] == "!"} {
      set header "> [string range $header 1 end]"
    } {
      set header "<$header> "
    }
  }
  putwire $cmd -1 "----- $header" $encrypted
}

# display to local user (still encrypted)
proc putwire {cmd idx header text} {
  global wire wired
  if {![info exists wired($cmd)]} {return 0}
  set jlist "*"
  set clist "*"
  foreach j [dcclist] {
    append jlist " [lindex $j 0]"
    if {[lindex $j 3] == "chat"} {
      append clist " [lindex $j 0]"
    }
  }
  foreach i $wired($cmd) {
    if {($i != "*") && ([lsearch $jlist $i] != -1)} {
      if {($i != $idx) || ([lsearch $clist $i] == -1) || ([echo $i] == 1)} {
        set x [decrypt $wire($i) $text]
        putdcc $i "${header}$x"
      }
    }
  }
}

proc isalphanum {arg} {
  if {[string length $arg] == 0} {return 0}
  set ctr 0
  while {$ctr < [string length $arg]} {
    if {![string match \[0-9a-zA-Z\] [string index $arg $ctr]]} {return 0}
    set ctr [expr $ctr + 1]
  }
  return 1
}

proc gordver_wire {} {
  return "wire.tcl v1.33"
}

#### These commands will be included in all scriptpacks released by
#### Gord-@saFyre as of 15 April 1996.  In combination with a 
#### gordver_* process for each scriptpack, it will provide you
#### with a way to determine what versions you or other bots
#### currently have loaded.  The command .checkver will return
#### a list of the loaded scripts and versions. The command
#### .checkver <bot> will return that info for the remote <bot>

proc checkver {} {
  set response { }
  foreach process [info procs gordver*] {
    lappend response [$process]
  }
  return [lsort $response]
}

bind dcc m checkver dcc_checkver
proc dcc_checkver {hand idx arg} {
  global botnick
  putcmdlog "#$hand# checkver $arg"
  if {[llength $arg] == 0} {
    putdcc $idx "*** The following scriptpacks by Gord-@saFyre are loaded:"
    foreach pack [checkver] {
      putdcc $idx "*** $pack"
    }
    putdcc $idx "*** End of list."
    return 0
  }
  foreach bot $arg {
    if {[lsearch [string tolower [bots]] [string tolower $bot]] == -1} {
      putdcc $idx "*** $bot is not a linked bot!"
    } {
      putbot $bot "checkver $hand"
    }
  }
  return 0
}

bind bot - checkver bot_checkver
proc bot_checkver {bot cmd arg} {
  global botnick
  set from [lindex $arg 0]
  sendnote $botnick "$from@$bot" "The following scriptpacks by Gord-@saFyre are loaded:"
  foreach pack [checkver] {
    sendnote $botnick "$from@$bot" $pack
  }
  sendnote $botnick "$from@$bot" "End of list."
  return 0
}

