package require agpib

# open the Tektronix DM5010
set chan [agpib::open -brd 0 -dev 16]

# set channel options
fconfigure $chan -term eio -timeout 1000

# send it initial commands
set cmds {init; acv; dig 4.5; rqs on; opc on; monitor on; mode run}
puts $chan $cmds; puts "<DM5010: $cmds"

# activate the readable script to fire when SRQ (the hardware interrupt
# line) is raised by this device (see FindRQS in the 488.2 docs).
fileevent $chan readable [list read_dmm $chan]

proc read_dmm {chan} {
   switch -- [fconfigure $chan -stb] {
       65 {#power on}
       67 {#user button pressed}
       132 {
          # device has a measurement ready
          set acv [format %f [string trimright [read $chan] {;}]]
          puts ">DM5010: ${acv}V"
       }
   }
}
