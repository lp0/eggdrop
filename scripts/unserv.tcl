#
# ###  unserv <server[:port]>
#    removes a server from the internal server list.  if the port
#    is omitted, it's assumed to be 6667.  if the bot is currently
#    sitting on the server you want to remove, it will jump.
#

bind dcc m unserver dcc_unserv

proc dcc_unserv {hand idx name} {
  global server servers
  if {$name == ""} {
    putdcc $idx "Usage: unserv <server[:port]>"
    return 0
  }
  if {[string first : $name] < 0} {
    set name "$name:6667"
  }
  set x [lsearch $servers $name]
  if {$x < 0} {
    putdcc $idx "$name isn't in the server list."
    return 0
  }
  if {$server == $name} {
    putdcc $idx "I'm on $name!  Jumping..."
    jump
  }
  set servers [lreplace $servers $x $x]
  putdcc $idx "Removed server $name"
  return 1
}
