# gainops1.tcl - sends and handles requests for ops 
# to or from other tandem bots on the same channel.
# If installed on all bots on the channel, this makes
# the perfect "need-op" package.  Added security features
# require that all bots have valid nick!user@host records
# for the other bots on the channel in order for this
# script to work.
# 29 January 1996 by Gord-@saFyre (ggold@panix.com)
# Modified 10 February 1996 by Gord- to send private
# notice requesting ops from current ops on channel if
# no bots are currently opped.
#
# Modified 1 March 1996 for eggdrop v1.0 bots! 
#    security patch, 1 june 1996 (cmwagner)
# version 1.1 patch, 11 jan 1997 (Jonte)

# The last three lines of this file causes the script to
# be properly set for every channel your bot is set up to
# join in its config file.

# This procedure logs the response of the op request
# from each connected, opped tandem bot on the channel

bind bot - opresp bot_op_response
proc bot_op_response {bot cmd response } { 
  putlog "$bot: $response"
  return 0
}

# This procedure handles the incoming op request from
# a connected tandem bot

bind bot - opreq bot_op_request
proc bot_op_request {bot cmd arg} {
  global botnick
  set opnick [lindex $arg 0]
  set channel [lindex $arg 1]
  if {$bot == $botnick} {
    return 0
  }
  if {![botisop $channel]} {
    putbot $bot "opresp I am not an op on $channel."
    return 0
  }
  if {[isop $opnick $channel]} {
    putbot $bot "opresp $opnick is already an op on $channel."
    return 0
  }
  if {![onchan $opnick $channel]} {
    putbot $bot "opresp $opnick is not on $channel."
    return 0
  }
  if {[onchansplit $opnick $channel]} {
    putbot $bot "opresp $opnick is split away from $channel."
    return 0
  }

  set uhost [getchanhost $opnick $channel]
  set hand [finduser ${opnick}!${uhost}]
  if {(![matchattr $hand b]) ||
      ((![matchchanattr $hand o $channel]) && (![matchattr $hand o]))} {
    putbot $bot "opresp $opnick is not +o or +b on my userlist."
    return 0
  }

  putcmdlog "$bot: OP $opnick $channel"
  putserv "MODE $channel +o $opnick"
  return 0
}

# This is the procedure that should be called in
# the need-op section of your bot's config file to 
# have it request ops from the other tandem bots on
# its channel. If there are no linked, opped bots
# on the channel, then it begs (via private notice)
# the current channel ops for ops.  If there are no
# ops on the channel, it asks everyone to leave so
# it can re-gain ops.

proc gain-ops {channel} {
  global botnick
  set botops 0
  foreach bot [chanlist $channel b] {
    if {(![onchansplit $bot $channel]) && [isop $bot $channel] && ([string first [string tolower [nick2hand $bot $channel]] [string tolower [bots]]] != -1)} {
      set botops 1
      putbot [nick2hand $bot $channel] "opreq $botnick $channel"
    }
  }
  if {$botops} {return 0}
  set chanops ""
  foreach user [chanlist $channel] {
    if {(![onchansplit $user $channel]) && [isop $user $channel]} {
      append chanops $user ","
    }
  }
  set chanops [string trim $chanops ","]
  if {[string length $chanops]} {
    putserv "NOTICE $chanops :Please op the bot. Thank-you."
  } else {
    putserv "NOTICE $channel :No channel ops?!?  Please leave the channel so we can re-gain ops. Thank-you."
  }
}

# This sets the script to work for every channel defined
# in your bot's config file
foreach channel [channels] {
  channel set $channel need-op "gain-ops $channel"
}
