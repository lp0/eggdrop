# getops-1.1.tcl a gainops type script for v1.1 bots (better than gainops)
# v1.0 - not released
# v1.1 - cleaned up a bit, release. 
# All global variables start with go_
# yes yes yes I know! this should be pushmode down below but it doesnt allow 
# modes that negate each other!
# Have fun :) -poptix@WildStar.Net

set go_warned 0
set go_have_friend 0
bind bot - gop botnet_request
bind bot - gop_resp gop_resp
proc gain_entrance {what chan} {
  global go_have_friend go_warned botnick
  foreach user [userlist] { 
    if {$what=="limit"} {
      foreach b [bots] {
        if {$user==$b} {
          putbot $user "gop limit $chan $botnick"
          putlog "Getops: Requested limit raise from $user on $chan"
          set go_have_friend 1
          set go_warned 0
        }
      }
    }
    if {$what=="invite"} {
      foreach b [bots] {
        if {$user==$b} {
          putbot $user "gop invite $chan $botnick"
          putlog "Getops: Requested invite from $user for $chan"
          set go_have_friend 1
          set go_warned 0
        }
      }  
    }
    if {$what=="unban"} {
      foreach a [channels] {
        if {[onchan $botnick $a]} {
          set bothost [getchanhost $botnick $a] 
        }
      }
      if {![info exists bothost]} {
        putlog "Getops: Doh! I cant find my host :("
        putlog "Getops: Try putting me on a hidden channel by myself.."
        return 0
      }
      foreach b [bots] {
        if {$user==$b} {
          putbot $user "gop unban $chan $botnick!$bothost"
          putlog "Getops: Requested unban on $chan from $user"
        }
      }
      set go_have_friend 1
      set go_warned 0
    }
    if {$what=="key"} {
      foreach b [bots] {
        if {$user==$b} {
          putbot $user "gop key $chan $botnick"
          putlog "Getops: Requested key on $chan from $user"
          set go_have_friend 1
          set go_warned 0
        }
      }
    }
    if {[matchattr $user b] && [matchchanattr $user o $chan]} {
      foreach prospect [chanlist $chan] {
        set temp $prospect![getchanhost $prospect $chan]
        if {[finduser $temp]==$user && $user!=$botnick && [isop $user $chan]} {
          if {$what=="op"} {
            putbot $user "gop op $chan $botnick"
            putlog "Getops: Requested Ops from $user on $chan"
            set go_have_friend 1
            set go_warned 0
          }
        }
      }
    }
  }
  if {!$go_warned && !$go_have_friend} {
    putlog "Getops: I couldn't find an @'d +ob user for $chan to get ops from :("
    set go_warned 1
    set go_have_friend 0
    timer 1 "set go_warned 0"
    return 0
  } else {
    return 1
  }
}

proc botnet_request {bot com args} {
global botnick subcom
  set args [lindex $args 0]
  set subcom [lindex $args 0]
  set chan [lindex $args 1]
  set nick [lindex $args 2]
  if {![validchan $chan]} {
    putbot $bot "gop_resp I'm not on that channel!"
    return 0
  }
  if {$subcom=="op"} {
    putlog "Getops: $bot requested ops on $chan"
    if {[matchchanattr [finduser $nick![getchanhost $nick $chan]] o $chan] && [matchattr [finduser $nick![getchanhost $nick $chan]] b]} {
      if {[isop $botnick $chan]} {
        if {![isop $nick $chan]} {
          putbot $bot "gop_resp Sent a +o at [ctime [unixtime]]"
          pushmode $chan +o $nick
        } else {
          putbot $bot "gop_resp You are +o on $chan, attempting resync"
          putserv "mode $chan -o+o $nick $nick" 
        }
      } else {
        putbot $bot "gop_resp I am not +o on $chan"
      }
    } else {
      putbot $bot "gop_resp You aren't +o in my userlist, sorry."
    }
    return 1
  } 
  if {$subcom=="unban"} {
    putlog "$bot requested that I unban him on $chan"
    foreach ban [chanbans $chan] {
      if {[string compare $nick $ban]} {
        putserv "MODE $chan -b $ban"
      }
    }
    return 1
  }
  if {$subcom=="invite"} {
    putlog "Getops: $bot asked for an invite to $chan"
    if {[matchattr $bot b]} {
      putserv "invite $nick $chan"
    }
    return 1
  }
  if {$subcom=="limit"} {
    putlog "Getops: $bot asked for a limit raise on $chan"
    if {[matchattr $bot b]} {
      set count 0
      foreach lamer [chanlist $chan] {
        incr count 1
      }
      incr count 2
      putserv "mode $chan +l $count"
    }
    return 1
  }
  if {$subcom=="key"} {
    putlog "Getops: $bot requested the key on $chan"
    if {[string compare [lindex [getchanmode $chan] 0] *k*]} {
      putbot $bot "gop takekey $chan [lindex [getchanmode $chan] 1]"
    } else {a
      putbot $bot "gop_resp There isn't a key on $chan!"
    }
    return 1
  }
  if {$subcom=="takekey"} {
    putlog "Getops: $bot gave me the key to $chan! ($nick)"
    foreach channel [channels] {
      if {$chan==$channel} {
        putserv "JOIN $channel $nick"
        return 1
      }
    }
  }
  putlog "Getops: ALERT! $bot sent fake 'gop' message! ($subcom)"
}
proc gop_resp {bot com stuff} {
  putlog "Getops: $bot says $stuff"
  return 1
}

proc validchan {chan} {
  foreach channel [channels] {
    if {$chan==$channel} {
      return 1
    }
  }
  return 0
}

proc do_channels {} {
  foreach a [channels] {
    channel set $a need-op "gain_entrance op $a"
    channel set $a need-key "gain_entrance key $a"
    channel set $a need-invite "gain_entrance invite $a"
    channel set $a need-unban "gain_entrance unban $a"
    channel set $a need-limit "gain_entrance limit $a"
    unset a
  }
  timer 5 do_channels
}
if {![info exists getops_loaded]} {
  timer 5 do_channels
}
foreach a [channels] {
  channel set $a need-op "gain_entrance op $a"
  channel set $a need-key "gain_entrance key $a"
  channel set $a need-invite "gain_entrance invite $a"
  channel set $a need-unban "gain_entrance unban $a"
  channel set $a need-limit "gain_entrance limit $a"
  unset a
}
# this MUST BE AT THE END OF THIS FILE!
set getops_loaded 1
