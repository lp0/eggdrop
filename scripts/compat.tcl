#
# This script just quickly maps old tcl functions to the new ones,
# use this is you are to lazy to get of your butt and update your scripts :D
#

proc gethosts {handle} {
  return [getuser $handle HOSTS]
}

proc addhost {handle host} {
  setuser $handle HOSTS $host
}

proc chpass {handle newpass} {
  setuser $handle PASS $newpass
}

# setxtra is no longer relevant 

proc getxtra {handle} {
   return [getuser $handle XTRA]
}

proc setinfo {handle info} {
   setuser $handle INFO $info
}

proc getinfo {handle} {
   return [getuser $handle INFO]
}

proc getaddr {handle} {
   return [getuser $handle BOTADDR]
}

proc setaddr {handle addr} {
   setuser $handle BOTADDR $addr
}

proc getdccdir {handle} {
   return [getuser $handle DCCDIR]
}

proc setdccdir {handle dccdir} {
   setuser $handle DCCDIR $dccdir
}

proc getcomment {handle} {
   return [getuser $handle COMMENT]
}

proc setcomment {handle comment} {
   setuser $handle COMMENT $comment
}

proc getemail {handle} {
   return [getuser $handle XTRA email]
}

proc setemail {handle email} {
   setuser $handle XTRA EMAIL $email
}

proc getchanlaston {handle} {
   return [lindex [getuser $handle LASTON] 1]
}

# as you can see it takes a lot of effort to simulate all the old commands
# and adapting your scripts will take such an effort you better include
# this file forever and a day :D
