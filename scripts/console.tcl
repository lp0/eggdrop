#
# save a user's console when she leaves, restore it when she returns
# (modified to work with eggdrop 1.0)
#
# ok, so there are basically two ways you can do this:
# 1) console mode is automatically saved when you leave
#    advantages: automatic -- any console mode changes you do will stick
#    disadvantages: if you're on the bot twice, whichever session exits
#       last will be the one to get saved
# 2) console mode is saved by typing '.store'
#    advantages: can set console the way you like it, store it that way,
#       and then make temporary changes later as you like
#    disadvantages: won't save changes if you forget to use '.store'
#
#
# 07-12-96 cmwagner  Added in force-channel variable, to force the newbies
#                    into a set channel, not to expose their lameness, or
#                    just not to expose the party line to your users, also
#                    added in saving and restoring of saved channel.
# 12-28-96 cmwagner  Added in info-party variable, to allow the info lines
#                    to be broadcasted across the party line channel the
#                    user joins.
# 13-1-97  billyjoe  Added 'strip' setting to saved console settings.
#                    Added code to remove the '@' from the beginning of an
#                    info line when displayed as a greet on the partyline.

# set which mode you want here:
# (0 = use '.store')  (1 = automatic save when you leave)
if {![info exists console-autosave]} {set console-autosave 0}

# set which channel you want to force users in if they don't have one
# stored:  (0 = party line)
if {![info exists force-channel]} {set force-channel 0}

# set this if you want to advertise info lines when users join then
# party line.  (0 = no)  (1 = yes)
if {![info exists info-party]} {set info-party 0}

##########################################################################

if {![info exists toolkit_loaded]} {
  catch {source scripts/toolkit.tcl}
  if {![info exists toolkit_loaded]} {
    catch {source toolkit.tcl}
    if {![info exists toolkit_loaded]} {
      error "Can't load Tcl toolkit!"
    }
  }
}

if {${console-autosave}} {
  bind chof - * save_console
} {
  bind dcc - store save_console_dcc
}
bind chon - * restore_console

proc restore_console {handle idx} {
  global force-channel info-party

  set cons [user-get $handle console]
  set conchan [user-get $handle conchan]
  if {![matchattr $handle m]} {
    foreach x {c x r o} {
      regsub -all $x $cons "" cons
    }
  }
  if {$cons != ""} {
    if {$conchan != ""} {
      console $idx $conchan $cons
    } {
      console $idx $cons
    }
  }
  set strip [user-get $handle strip]
  if {$strip != ""} { strip $idx $strip }
  set echo [user-get $handle echo]
  if {$echo != ""} { echo $idx $echo }
  set chan [user-get $handle chan]
  if {$chan == ""} {
     set chan ${force-channel}
  }
  setchan $idx $chan

  set info [string trimleft [getinfo $handle] @]
  if {${info-party} && $info != ""} {
     dccputchan $chan "\[$handle\] $info"
  }
}

proc save_console {handle idx} {
  set cons [console $idx]
  user-set $handle conchan [lindex $cons 0]
  user-set $handle console [lindex $cons 1]
  user-set $handle echo [echo $idx]
  user-set $handle strip [strip $idx] 
  user-set $handle chan [getchan $idx] 
}

proc save_console_dcc {handle idx param} {
  set cons [console $idx]
  set chan [getchan $idx]
  set strip [strip $idx]
  user-set $handle conchan [lindex $cons 0]
  user-set $handle console [lindex $cons 1]
  user-set $handle echo [echo $idx]
  user-set $handle strip [strip $idx]
  user-set $handle chan $chan
  set newcons "[lindex $cons 0]: [lindex $cons 1]"
  if {[echo $idx]} {
    putdcc $idx "Saved your console mode as $newcons (echo on) (chan $chan) (strip $strip)"
  } {   
    putdcc $idx "Saved your console mode as $newcons (echo off) (chan $chan) (strip $strip)"
  }
  return 1
}
