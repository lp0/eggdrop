#
# detect floodnets  v2e
#   by robey, butchbub, gord-
#

bind msgm - * check_floodnet
bind ctcp - * check_floodnet_ctcp

# last 5 msgs i received
if {![info exists floodmsglist]} {
  set floodmsglist {{0 nobody x} {0 nobody z} {0 nobody v} {0 nobody y} {0 nobody w}}
}

set floodlistlen 5
set floodalert 0
# this means 3 of the previous 4 msgs have to be identical:
set floodtrigger 4
# the timespan between them has to be this small (seconds):
set floodtime 10

proc check_floodnet_ctcp {nick uhost hand dest keyword text} {
  check_floodnet $nick $uhost $hand "CTCP $keyword $text"
  return 0
}

proc check_floodnet {nick uhost hand text} {
  global floodmsglist floodalert floodlistlen floodtime floodtrigger
  # rotate floodmsglist
  set floodmsglist [lreplace $floodmsglist 0 0]
  lappend floodmsglist [list [unixtime] $nick!$uhost $text]
  # timespan satisfied?
  if {[unixtime]-[lindex [lindex $floodmsglist 0] 0] > $floodtime} {
    if {$floodalert} { check_end_flood [expr $floodlistlen-1] }
    return
  }
  # check for multiple
  set count 0
  for {set i 0} {$i < $floodlistlen-1} {incr i} {
    if {[string compare [string tolower $text] \
    [string tolower [lindex [lindex $floodmsglist $i] 2]]] == 0} { incr count }
  }
  if {$count < $floodtrigger} {
    if {$floodalert} { check_end_flood [expr $floodlistlen-2] }
    return
  }
  # okay this one counts
  if {!$floodalert} {
    # new flood
    set floodalert 1
    putlog "(**) I am being flooded, possibly by a floodnet."
    putlog "(**) Entering dike mode."
    trample_oldflood $text
  }
  add_floodnet $nick!$uhost
}

bind bot - floodnotice floodnet_notice
proc floodnet_notice {from cmd rest} {
  set mask [lindex $rest 0]
  set fullhost [lindex $rest 1]
  putlog "($from) floodnet ignore: $mask ($fullhost)"
  newignore $mask $from "(dike) floodnet: $fullhost" 0
}

proc add_floodnet {fullhost} {
  regsub "^\\*!" [maskhost $fullhost] "*!*" new
  if {![isignore $new]} {
    # uh, if it was on ignore, how did it trigger msgm?
    putlog "floodnet ignore: $new ($fullhost)"
    putallbots "floodnotice $new $fullhost"
    newignore $new "dike" "floodnet: $fullhost" 0
  }
}

# every text from msg that matches the most recent one: part of the floodnet
proc trample_oldflood {text} {
  global floodlistlen floodmsglist
  for {set i 0} {$i < $floodlistlen-2} {incr i} {
    set theysaid [lindex [lindex $floodmsglist $i] 2]
    set theysaid [string tolower $theysaid]
    if {[string compare $theysaid [string tolower $text]] == 0} {
      # match!
      add_floodnet [lindex [lindex $floodmsglist $i] 1]
    }
  }
}

proc check_end_flood {howfar} {
  global floodalert floodlistlen floodmsglist
  # if the previous message was ALSO different, we may be ok
  set prevmsg [string tolower [lindex [lindex $floodmsglist $howfar] 2]]
  set ok 1
  for {set i 0} {$i < $howfar} {incr i} {
    if {[string compare [string tolower [lindex [lindex $floodmsglist $i] 2]] $prevmsg] == 0} { set ok 0 }
  }
  if {! $ok} { return }
  # must be over
  putlog "(**) Floodnet bombardment seems to be over; leaving dike mode."
  set floodalert 0
}

if {![info exists dike_timer]} {
  set dike_timer [timer 2 dike_check]
}
proc dike_check {} {
  global dike_timer floodmsglist floodlistlen floodalert
  if {$floodalert} {
    # if last msg was 120 seconds ago, all is clear
    set lastmsg [lindex [lindex $floodmsglist [expr $floodlistlen-1]] 0]
    if {[unixtime]-$lastmsg > 120} {
      check_end_flood 0
    }
  }
  set dike_timer [timer 2 dike_check]
}

# Procedure to kick/ban recognized floodnet bots from the 
# channel on join.
#       by Gord-
# to enable this option, set to 1  
# to disble, set to 0
set enable_kb_floodnet 1

proc join_ignore_kb {nick uhost handle channel} {
  if {![isignore $uhost]} {return 0}
  foreach item [ignorelist] {
    set hostmask [lindex $item 0]
    set comment [lindex $item 1]
    if {[string match $hostmask "$nick!$uhost"]} {
      if {([string first "floodnet" [string tolower $comment]] != -1) || ([string first "fludnet" [string tolower $comment]] != -1)} {
        newchanban $channel $hostmask "floodban" "Floodnet bots not welcome here."
        putcmdlog "Floodnet bot $nick!uhost joined $channel - repelled!!"
      }
      break
    }
  }
  return 0
}

if {$enable_kb_floodnet} {
  bind join - * join_ignore_kb
}

putlog "dike (v2e) loaded; floodnet patrol armed & ready"
