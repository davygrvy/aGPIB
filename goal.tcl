package require agpib

# open the Tek DM5010
set chan [agpib::open -board 0 -device 16]

# set channel options
fconfigure $chan -term eio -timeout 1000 -waitcond srq

# send it initial commands
puts $chan {acv; dig 4.5; rqs on; opc on; monitor on; mode run}

# activate the readable script to fire when SRQ is set
fileevent $chan readable [list read_dmm $chan]

proc read_dmm {chan} {
   switch -- [fconfigure $chan -spoll] {
       65 {#power on}
       67 {#user button pressed}
       132 {
          # device has a measurement ready
          set acv [format %f [string trimright [gets $chan] {;}]]
       }
   }
}
