#
# some examples of extending eggdrop with TCL
#
# the following dcc commands are added:
#   bind <type> <flags> <command> <proc>
#     (does exactly what the Tcl 'bind' command does)
#   unbind <type> <flags> <command> <proc>
#     (removes a binding)
#   report
#     (an alias for 'status' -- but only requires the +o flag)
#
# the following public command is added (for fun & example):
#   gross
#     (makes the bot say "yeah, gross!")
#
# the following msg command is added (for cuteness):
#   rose <nickname> [optional message]
#     (sends a rose to someone)
#

proc putchan {chan what} {
  putserv "privmsg $chan :$what"
}

# the bind command
proc cmd_bind {hand idx arg} {
  dccsimul $idx ".tcl bind $arg"
}
bind dcc n bind cmd_bind

# the unbind command
proc cmd_unbind {hand idx arg} {
  dccsimul $idx ".tcl unbind $arg"
}
bind dcc n unbind cmd_unbind

# make a dcc command called 'report' that is just an alias for 'status',
# but only requires +o to use
bind dcc o report *dcc:status

# respond to "gross" on a channel :)
proc pub_gross {nick uhost hand chan args} {
  putchan $chan "yeah, gross!"
  return 1
}
bind pub - gross pub_gross

# the "rose" command via msg
proc putnot {nick msg} { puthelp "NOTICE $nick :$msg" }
proc msg_rose {nick uhost hand rest} {
  global botnick
  if {$rest == ""} {
    putnot $nick "Usage: /msg $botnick rose <nick> \[message\]"
    putnot $nick "  sends a rose to <nick>, with the optional \[message\] on"
    putnot $nick "  an attached card"
    return 0
  }
  set who [lindex $rest 0]
  set msg [lrange $rest 1 end]
  putnot $who "$nick sends you a single rose:"
  putnot $who "   ---<-'-@"
  if {$msg != ""} {
    putnot $who "There is an attached note which reads:"
    putnot $who "  $msg"
  }
  putnot $nick "Rose delivery attempted to $who! :-)"
  return 1
}
bind msg - rose msg_rose

