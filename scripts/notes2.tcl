#
# notes2.tcl - v2.0.0 - released by MHT <mht@mygale.org>
#                     - a bind apart script from #TSF
#                     - for eggdrop 1.3.14+mht and 1.3.15+
#
####
#
# Check your notes on every shared bot of the hub.
#
# .notes [bot|all] index
# .notes [bot|all] read  <#|all>
# .notes [bot|all] erase <#|all>
#
# # may be numbers and/or intervals separated by ;
# ex: .notes erase 2-4;8;16-
#     .notes noBOTy read all
#


########
unbind dcc  - notes        *dcc:notes
bind   dcc  - notes        *dcc:notes2
bind   chon - *            *chon:notes2
bind   bot  - notes2:      *bot:notes2
bind   bot  - notes2reply: *bot:notes2reply

########
proc n2_notesindex {bot handle} {
    global nick
    switch [notes $handle] {
	-2 { putbot $bot "notes2reply: $handle Notefile failure." }
	-1 { putbot $bot "notes2reply: $handle I don't know you." }
	0  { putbot $bot "notes2reply: $handle You have no messages." }
	default {
	    putbot $bot "notes2reply: $handle ### You have the following notes waiting:"
	    set index 0
	    foreach note [notes $handle "-"] {
		if {($note != 0)} {
		    incr index
		    set sender [lindex $note 0]
		    set date [strftime "%b %d %H:%M" [lindex $note 1]]
		    putbot $bot "notes2reply: $handle %$index. $sender ($date)"
		}
	    }
	    putbot $bot "notes2reply: $handle ### Use '.notes $nick read' to read them."
	}
    }
}

########
proc n2_notesread {bot handle numlist} {
    if {($numlist == "")} { set numlist "-" }
    switch [notes $handle] {
	-2 { putbot $bot "notes2reply: $handle Notefile failure." }
	-1 { putbot $bot "notes2reply: $handle I don't know you." }
	0  { putbot $bot "notes2reply: $handle You have no messages." }
	default {
	    set count 0
	    set list [listnotes $handle $numlist]
	    foreach note [notes $handle $numlist] {
		if {($note != 0)} {
		    set index [lindex $list $count]
		    set sender [lindex $note 0]
		    set date [strftime "%b %d %H:%M" [lindex $note 1]]
		    set msg [lrange $note 2 end]
		    incr count
		    putbot $bot "notes2reply: $handle $index. $sender ($date): $msg"
		}
	    }
	}
    }
}

########
proc n2_noteserase {bot handle numlist} {
    switch [notes $handle] {
	-2 { putbot $bot "notes2reply: $handle Notefile failure." }
	-1 { putbot $bot "notes2reply: $handle I don't know you." }
	0  { putbot $bot "notes2reply: $handle You have no messages." }
	default {
	    set erased [erasenotes $handle $numlist]
	    set remaining [notes $handle]
	    if {($remaining == 0) && ($erased == 0)} {
		putbot $bot "notes2reply: $handle You have no messages."
	    } elseif {($remaining == 0)} {
		putbot $bot "notes2reply: $handle Erased all notes."
	    } elseif {($erased == 0)} {
		putbot $bot "notes2reply: $handle You don't have that many messages."
	    } else {
		putbot $bot "notes2reply: $handle Erased #$erased, $remaining left."
	    }
	}
    }
}

########
proc *bot:notes2 {handle idx arg} {
    if {(![matchattr $handle s])} {
	return
    }
    set nick [lindex $arg 0]
    set cmd  [lindex $arg 1]
    set num  [lindex $arg 2]
    if {$num == "all"} { set num "-" }
    switch $cmd {
	"index" { n2_notesindex $handle $nick }
	"read"  { n2_notesread $handle $nick $num }
	"erase" { n2_noteserase $handle $nick $num }
    }
    putcmdlog "#$nick@$handle# notes $cmd $num"
}

########
proc *bot:notes2reply {handle idx arg} {
    set idx [hand2idx [lindex $arg 0]]
    set reply [lrange $arg 1 end]
    if {([string range $reply 0 0] == "%")} {
	set reply "   [string range $reply 1 end]"
    }
    putidx $idx "($handle) $reply"
}

########
proc *chon:notes2 {handle idx} {
    putallbots "notes2: $handle index"
    return 0
}

########
proc *dcc:notes2 {handle idx arg} {
    global nick
    if {$arg == ""} {
	putidx $idx "Usage: notes \[bot|all\] index"
	putidx $idx "       notes \[bot|all\] read <#|all>"
	putidx $idx "       notes \[bot|all\] erase <#|all>"
	putidx $idx "       # may be numbers and/or intervals separated by ;"
	putidx $idx "       ex: notes erase 2-4;8;16-"
	putidx $idx "           notes $nick read all"
    } else {
	set bot [string tolower [lindex $arg 0]]
	set cmd [string tolower [lindex $arg 1]]
	set num [string tolower [lindex $arg 2]]
	if {($bot != "all") && ([lsearch [string tolower [bots]] $bot] < 0)} {
	    if {($cmd != "index") && ($cmd != "read") && ($cmd != "erase")} {	    
		if {($bot == [string tolower $nick])} {
		    return [*dcc:notes $handle $idx [lrange $arg 1 end]]
		} else {
		    return [*dcc:notes $handle $idx $arg]
		}
	    } else {
		putidx $idx "I don't know anybot by that name."
		return 0
	    }
	} elseif {($cmd != "index") && ($cmd != "read") && ($cmd != "erase")} {
	    putdcc $idx "Function must be one of INDEX, READ, or ERASE."
	} elseif {$bot == "all"} {
	    #*dcc:notes $handle $idx [lrange $arg 1 end]
	    putallbots "notes2: $handle $cmd $num"
	} else {
	    putbot $bot "notes2: $handle $cmd $num"
	}
	putcmdlog "#$handle# notes@$bot $cmd $num"
    }
}

########
putlog "Notes 2.0.0 - Released by MHT <mht@mygale.org>"

####
