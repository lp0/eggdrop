#
# scripts to implement the old '+friend' etc commands
# for compatability with REALLY old versions of eggdrop
#

bind dcc m +op "do_chattr +o"
bind dcc m -op "do_chattr -o"
bind dcc m +friend "do_chattr +f"
bind dcc m -friend "do_chattr -f"
bind dcc m +deop "do_chattr +d"
bind dcc m -deop "do_chattr -d"
bind dcc m +party "do_chattr +p"
bind dcc m -party "do_chattr -p"
bind dcc m +xfer "do_chattr +x"
bind dcc m -xfer "do_chattr -x"
bind dcc m +master "do_chattr +m"
bind dcc m -master "do_chattr -m"
bind dcc m +kick "do_chattr +k"
bind dcc m -kick "do_chattr -k"

proc do_chattr {change handle idx who} {
  if {$who == ""} {
    putdcc $idx "You need to specify a handle."
    return 0
  }
  set atr [chattr $who $change]
  putdcc $idx "Flags for $who are now: $atr"
  return 1
}
