# getops-1.8.tcl by dtM, a GainOps-like script for eggdrop 1.1.x

# This script originated from getops-1.2.tcl by poptix, which, I found out,
# did not work. I modified it to where it worked very nicely and
# efficiently. Thanks to poptix for the ideas and something to do. :)

# thanks to beldin/david/cfusion/oldgroo and anyone else who gave me
#  suggestions/fixes :)

# [0/1] do you want your bot to request to be unbanned if it becomes banned?
set go_bot_unban 1

# [0/1] do you want GetOps to wallop if there aren't any bots to talk to?
set go_wallop 1

# set this to the wallop msg for the above (go_wallop)
set go_wallop_msg "Please op one of the bots :)"

# [0/1] do you want GetOps to notice the channel if there are no ops?
set go_cycle 1

# set this to the notice txt for the above (go_cycle)
set go_cycle_msg "Please part the channel so the bots can cycle!"

# set this to the max number of msg/notice recipients you want
set go_max_recips 10

proc gain_entrance {what chan} {
 global go_have_friend go_warned botnick botname go_wallop go_wallop_message go_bot_unban go_wallop go_wallop_msg go_cycle go_cycle_msg go_max_recips go_resynch
 switch -exact $what {
  "limit" {
   foreach bs [lbots] {
    putbot $bs "gop limit $chan $botnick"
    putlog "GetOps: Requested limit raise from $bs on $chan."
    set go_have_friend($chan) 1
   }
  }
  "invite" {
   foreach bs [lbots] {
    putbot $bs "gop invite $chan $botnick"
    putlog "GetOps: Requested invite from $bs for $chan."
    set go_have_friend($chan) 1
   }  
  }
  "unban" {
   if {$go_bot_unban} {
    foreach bs [lbots] {
     putbot $bs "gop unban $chan $botname"
     putlog "GetOps: Requested unban on $chan from $bs."
    }
    set go_have_friend($chan) 1
   }
  }
  "key" {
   foreach bs [lbots] {
    putbot $bs "gop key $chan $botnick"
    putlog "GetOps: Requested key on $chan from $bs."
    set go_have_friend($chan) 1
   }
  }
  "op" {
   foreach bs [lbots] {
    if {[iso $bs $chan]} {
     foreach prospect [chanlist $chan] {
      set temp $prospect![getchanhost $prospect $chan]
      if {[finduser $temp] == $bs && $bs != $botnick && [isop $bs $chan]} {
       putbot $bs "gop op $chan $botnick"
       putlog "GetOps: Requested Ops from $bs on $chan."
       set go_have_friend($chan) 1
      }
     }
    }
   }
  }
 }
 if {[info exists go_warned($chan)]} {
  if {$go_warned($chan) == 0 && $go_have_friend($chan) == 0} {
   putlog "GetOps: Couldn't find an opped +ob user for $chan to get ops from."
   if {$go_wallop} {
    set recips 0
    set go_wallop_sent($chan) 0
    foreach user1 [chanlist $chan] {
     if {![onchansplit $user1 $chan] && [isop $user1 $chan] && [iso $user1 $chan]} {
      if {![info exists oplist]} {
       set oplist $user1
      } {
       set oplist $oplist,$user1
      }
      set recips [expr $recips + 1]
      if {$recips == $go_max_recips} {
       putserv "NOTICE $oplist :($chan) $go_wallop_msg"
       set go_wallop_sent($chan) 1
       unset oplist
       set recips 0
      }
     }
    }
   }
   if {[info exists oplist]} {
    putserv "NOTICE $oplist :($chan) $go_wallop_msg"
    set go_wallop_sent($chan) 1
   }
   if {$go_wallop_sent($chan) == 0 && $go_cycle} {
    putlog "GetOps: No ops on $chan, sending cycle message."
    putserv "NOTICE $chan :($chan) $go_cycle_msg"
   }
   set go_warned($chan) 1
   set go_have_friend($chan) 0
   timer 2 "set go_warned($chan) 0"
   return 0
  } {
   return 1
  }
 } 
}

proc botnet_request {bot com args} {
 global botnick subcom go_bot_unban go_resynch
 set args [lindex $args 0]
 set subcom [lindex $args 0]
 set chan [string tolower [lindex $args 1]]
 set nick [lindex $args 2]
 if {[validchan $chan] == 0} {
  putbot $bot "gop_resp I'm not on that channel."
  return 0
 }
 switch -exact $subcom {
  "op" {
   putlog "GetOps: $bot requested ops on $chan."
   if {[iso [finduser $nick![getchanhost $nick $chan]] $chan] && [matchattr [finduser $nick![getchanhost $nick $chan]] b]} {
    if {[botisop $chan]} {
     if {![isop $nick $chan]} {
      putbot $bot "gop_resp Opped $nick on $chan."
      pushmode $chan +o $nick
     } {
      if {$go_resynch($chan) == 0} {
       putbot $bot "gop_resp You are already +o on $chan, attempting resynch."
       putserv "MODE $chan -o+o $nick $nick"
       set go_resynch($chan) 1
      } {
       putbot $bot "gop_resp You are +o on $chan, already sent resynch modes."
       timer 1 "set go_resynch($chan) 0"
      }
     }
    } {
     putbot $bot "gop_resp I am not +o on $chan."
    }
   } {
    putbot $bot "gop_resp You aren't +o in my userlist for $chan, sorry."
   }
   return 1
  }
  "unban" {
   if {$go_bot_unban} {
    putlog "GetOps: $bot requested that I unban him on $chan."
    foreach ban [chanbans $chan] {
     if {[string compare $nick $ban]} {
      pushmode $chan -b $ban
     }
    }
    return 1
   } {
    putlog "GetOps: Refused request to unban $bot ($nick) on $chan."
    putbot $bot "gop_resp Sorry, not accepting unban requests."
   }
  }
  "invite" {
   putlog "GetOps: $bot asked for an invite to $chan."
   if {[matchattr $bot b]} {
    putserv "invite $nick $chan"
   }
   return 1
  }
  "limit" {
   putlog "GetOps: $bot asked for a limit raise on $chan."
   if {[matchattr $bot b]} {
    pushmode $chan +l [expr [llength [chanlist $chan]] + 2]
   }
   return 1
  }
  "key" {
   putlog "GetOps: $bot requested the key on $chan."
   if {[string match *k* [lindex [getchanmode $chan] 0]]} {
    putbot $bot "gop takekey $chan [lindex [getchanmode $chan] 1]"
   } {
    putbot $bot "gop_resp There isn't a key on $chan!"
   }
   return 1
  }
  "takekey" {
   putlog "GetOps: $bot gave me the key to $chan! ($nick)"
   foreach channel [string tolower [channels]] {
    if {$chan == $channel} {
     putserv "JOIN $channel $nick"
     return 1
    }
   }
  }
  default {
   putlog "GetOps: ALERT! $bot sent fake 'gop' message! ($subcom)"
  }
 }
}

proc gop_resp {bot com msg} {
 putlog "GetOps: $bot: $msg"
 return 1
}

proc lbots {} {
 set unf ""
 foreach users [userlist b] {
  foreach bs [bots] {
   if {$users == $bs} {
    lappend unf $users
   }
  }
 }
 return $unf
}

proc iso {nick chan1} {
 if {[matchattr [nick2hand $nick $chan1] o] || [matchchanattr [nick2hand $nick $chan1] o $chan1]} {
  return 1
  break
 }
 return 0
}

proc validchan {chan} {
 foreach channel [string tolower [channels]] {
  if {([string tolower $chan] == $channel) && (![isdynamic $channel])} {
   return 1
  }
 }
 return 0
}

proc do_channels {} {
 foreach a [string tolower [channels]] {
  if {![isdynamic $a]} {
   channel set $a need-op "gain_entrance op $a"
   channel set $a need-key "gain_entrance key $a"
   channel set $a need-invite "gain_entrance invite $a"
   channel set $a need-unban "gain_entrance unban $a"
   channel set $a need-limit "gain_entrance limit $a"
   unset a
  }
 }
 timer 5 do_channels
}

if {![string match "*do_channels*" [timers]]} { timer 5 do_channels }

foreach go_array {go_have_friend go_warned go_wallop_sent go_resynch} {
 foreach go_chans [string tolower [channels]] {
  if {![isdynamic $go_chans]} {
   set ${go_array}($go_chans) 0
  }
 }
}

bind bot - gop botnet_request
bind bot - gop_resp gop_resp

set getops_loaded 1
putlog "GetOps v1.8 by dtM loaded."
