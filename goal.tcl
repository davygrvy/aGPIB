package require agpib

# open the device on address 16 of the first board
set chan [agpib::open -brd 0 -pad 16]

# set channel options
fconfigure $chan -term eio -timeout 300ms -blocking no

# who are we connected to?
puts $chan {id?}
puts ">DM5010: [read $chan]"

# send it initial commands
set cmds {init; acv; dig 4.5; rqs on; opc on; monitor on; mode run}
puts $chan $cmds; puts "<DM5010: $cmds"

# set the exception script to return the STB from the reulting RQS/SRQ
fileevent $chan exception \
        [list dmm_stb $chan [subst -noc {[fconfigure $chan -stb]}]]

proc dmm_stb {chan stb} {
   switch -- $stb {
       65 {#power on}
       67 {puts "you pressed the 'inst id' key"}
       132 - 140 {
          # device has a measurement ready
          set acv [format %f [string trimright [read $chan] {;}]]
          puts ">DM5010: ${acv} Vrms"
       }
   }
}

if {![info exists tk_version] && ![info exists tcl_service]} {vwait forever}
 
results will be:
>DM5010: ID TEK/DM5010,V79.1 F1.1
<DM5010: init; acv; dig 4.5; rqs on; opc on; monitor on; mode run
>DM5010: 3.12u Vrms
>DM5010: 3.14u Vrms
>DM5010: 3.19u Vrms
>DM5010: 3.11u Vrms
>DM5010: 3.12u Vrms
you pressed the 'inst id' key
>DM5010: 3.11u Vrms
...
