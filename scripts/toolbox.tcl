# toolbox.tcl -- useful procedures for use in tcl
# by cmwagner@sodre.net -- any other suggestions email me

# so other scripts can bitch if toolbox.tcl is not loaded
set toolbox_loaded 1

# so other scripts can bitch if toolbox.tcl is obsolete
set toolbox_revision 1004

# string tolower
proc strlwr {string} {
   return [string tolower $string]
}

# string toupper
proc strupr {string} {
   return [string toupper $string]
}

# string compare
proc strcmp {string1 string2} {
   return [string compare $string1 $string2]
}

# string compare (insensitive to case)
proc stricmp {string1 string2} {
   return [string compare [strlwr $string1] [strlwr $string2]]
}

# string length
proc strlen {string} {
   return [string length $string]
}

# string index
proc stridx {string index} {
   return [string index $string $index]
}

# is a certain command a valid tcl command
proc iscommand {command} {
   if {[lsearch -exact [strlwr [info commands]] [strlwr $command]] != -1} {
      return 1
   }

   return 0
}

# check to see if a timer for a certain procedure exists
proc timerexists {timer_proc} {
   foreach j [timers] {
      if {[string compare [lindex $j 1] $timer_proc] == 0} {
         return [lindex $j 2]
      }
   }
}

# check to see if a utimer for a certain procedure exists
# cmwagner@soder.net
if {[iscommand utimers]} {
   proc utimerexists {timer_proc} {
      foreach j [utimers] {
         if {[string compare [lindex $j 1] $timer_proc] == 0} {
            return [lindex $j 2]
         }
      }
   }
}

# is a bot in the chain
proc inchain {bot} {
   if {[lsearch -exact [strlwr [bots]] [strlwr $bot]] != -1} {
      return 1
   }

   return 0
}

# generate a string with random characters in it
proc randstring {count} {
  set rs ""
  for {set j 0} {$j < $count} {incr j} {
     set x [rand 62]
     append rs [string range "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" $x $x]
  }

  unset x
  unset j
  return $rs
}

# send text to all dcc users
proc putdccall {msg} {
   foreach j [dcclist] {
      putdcc [lindex $j 0] $msg
   }
}

# send text to all dcc users but idx
proc putdccbut {idx msg} {
   foreach j [dcclist] {
      if {[lindex $j 0] != $idx} {
         putdcc [lindex $j 0] $msg
      }
   }
}

# kill all dcc users
proc killdccall {} {
   foreach j [dcclist] {
      killdcc [lindex $j 0]
   }
}

# kill dcc users but idx
proc killdccbut {idx} {
   foreach j [dcclist] {
      if {[lindex $j 0] != $idx} {
         killdcc [lindex $j 0]
      }
   }
}

# check to see if idx is in dcclist, if it is returns 1
proc valididx {idx} {
   set r 0
   foreach j [dcclist] {
      if {[lindex $j 0] == $idx} {
          set r 1
          break
      }
   }

   return $r
}
