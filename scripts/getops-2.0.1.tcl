# 2.0.1 by beldin (1.3.x ONLY version)
# actually, iso needed to be modded for 1.3 :P, and validchan is builtin
# and I'll tidy a couple of functions up to

# 2.0 by DarkDruid
# It'll work with dynamic channels(dan is a dork for making it not..)
# It will also only ask one bot at a time for ops so there won't be any more
# annoying mode floods, and it's more secure that way
# I also took that annoying wallop and resynch stuff out :P
# And I guess this will with with 1.3.x too


# 1.9 by The_O
# will now op a bot that is in the channel with a different nick
# than the userfile handle

# getops-1.8.tcl by dtM, a GainOps-like script for eggdrop 1.1.x

# This script originated from getops-1.2.tcl by poptix, which, I found out,
# did not work. I modified it to where it worked very nicely and
# efficiently. Thanks to poptix for the ideas and something to do. :)

# thanks to beldin/david/cfusion/oldgroo and anyone else who gave me
#  suggestions/fixes :)

# [0/1] do you want your bot to request to be unbanned if it becomes banned?
set go_bot_unban 1

# [0/1] do you want GetOps to notice the channel if there are no ops?
set go_cycle 0

# set this to the notice txt for the above (go_cycle)
set go_cycle_msg "Please part the channel so the bots can cycle!"

set bns ""
proc gain_entrance {what chan} {
 global go_have_friend go_warned botnick botname go_bot_unban go_cycle go_cycle_msg go_resynch bns 
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
   set there_are_ops [checkforops $chan]
   if {$there_are_ops == 1} {
    set bot [getbot $chan]
    if {$bot == ""} {
     set bns ""
     set bot [getbot $chan]
    }
    set bns "$bns $bot"
    if {$bot != ""} {
     putbot $bot "gop op $chan $botnick"
     putlog "Requesting ops from $bot on $chan.."
    }
   } {
    if {$go_cycle == 1} {
     putserv "NOTICE $chan :$go_cycle_msg"
    }
   }
  }
 }
}
proc checkforops {chan} {
  foreach user [chanlist $chan] {
    if {[isop $user $chan]} {
      return 1
    }
  }
  return 0
}
proc getbot {chan} {
  global bns
  foreach bn [bots] {
    if {[lsearch $bns $bn] < 0} {
      if {([matchattr $bn o]) || ([matchattr $bn |o $chan])} {
        if {([onchan [hand2nick $bn $chan] $chan]) && ([isop [hand2nick $bn $chan] $chan])} {
          return $bn
          break
        }
      }
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
   if {[iso $nick $chan] && [matchattr [finduser $nick![getchanhost $nick $chan]] b]} {
    if {[botisop $chan]} {
     if {![isop $nick $chan]} {
      putbot $bot "gop_resp Opped $nick on $chan."
      pushmode $chan +o $nick
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
 return [matchattr [nick2hand $nick $chan1] o|o $chan1]
}

proc do_channels {} {
 foreach a [string tolower [channels]] {
   channel set $a need-op "gain_entrance op $a"
   channel set $a need-key "gain_entrance key $a"
   channel set $a need-invite "gain_entrance invite $a"
   channel set $a need-unban "gain_entrance unban $a"
   channel set $a need-limit "gain_entrance limit $a"
   unset a
 }
 timer 5 do_channels
}
do_channels
if {![string match "*do_channels*" [timers]]} { timer 5 do_channels }
foreach go_array {go_have_friend go_warned go_wallop_sent go_resynch} {
 foreach go_chans [string tolower [channels]] {
   set ${go_array}($go_chans) 0
 }
}

bind bot - gop botnet_request
bind bot - gop_resp gop_resp

set getops_loaded 1
putlog "GetOps v2.0.1 by dtM, The_O, and now DarkDruid(phear) loaded."

