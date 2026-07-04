package require agpib

# open the device on address 16 of the first board (/dev/gpib0)
set dmm [agpib::open -brd 0 -pad 16]

# set channel options (generic set)
fconfigure $dmm -blocking yes -encoding ascii -translation none

# set channel options (GPIB type specific)
fconfigure $dmm -term eoi -timeout 300u

# who are we connected to?
set cmds {id?}
puts "<DM5010: $cmds"; puts $dmm $cmds
puts ">DM5010: [read $dmm]"

proc bgerror {message} {
    puts "background error in handler script: $message"
}

# Set up the exception script to manage STB from the incoming
# SRQ/RQS interrupts on the bus.
#
fileevent $dmm exception \
        [list dmm_stb $dmm [subst -noc {[fconfigure $dmm -pop_stb]}]]

# Our exception (STB - status byte) handler script
#
proc dmm_stb {chan stb} {

    # Split the STB ubyte into the informational components as described on
    # page 3-24 of the Tektronix DM 5010 manual
    # https://w140.com/tekwiki/images/5/5e/070-2994-01.pdf
    #
    binary scan [binary format c $stb] B8 bstr
    lassign [list [string index $bstr 0] \
              [string index $bstr 1] \
              [string index $bstr 2] \
              [string index $bstr 3] \
              [string range $bstr 4 end]] class RQS abnormal busy event

    # Ignore $RQS as we already know this will be set, as this is where we
    # came from as an SPOLL response to the SRQ interrupt that has now
    # been cleared.

    if {$class} {
        # DEVICE STATUS

        # split event bits
        lassign [list [string index $event 0] \
                [string index $event 1] \
                [string range $event 2 end]] trigger readable status

        # alert for a readable condition interrupt happens here
        if {$readable} {
            set acv [format %f [string trimright [read $chan] {;}]]
            puts ">DM5010: ${acv} Vrms"
        }

        # fall through ->

        # $trigger is available, but serves no purpose for us here.
        # Possible use is to untrigger now if we got a readable
        # notification, too.

        # $busy would indicate the message processor is in a busy
        # state, at the moment, and probably isn't able to receive
        # a new command right now.

        set code [expr "0b$status"]    ;# [scan] doesn't do this
        switch -- $code {
            0 {
                #no errors or events
            }
            1 {puts ">DM5010: Below limits device status"}
            3 {puts ">DM5010: Above limits device status"}
            default { puts ">DM5010: Unhandled device status code: $code" }
        }
        
    } else {
        # EVENT CLASS

        if {$abnormal} {
            # ask what error
            puts $chan {err?}
            set errCode [string trimright [read $chan] {;}]

            switch -- $errCode {
                101 {set err "Invalid command header"}
                102 {set err "Header delimiter error"}
                103 {set err "Argument error"}
                104 {set err "Argument delimiter error"}
                106 {set err "Missing argument"}
                107 {set err "Invalid message unit delimiter"}
                201 {set err "Not executable in local mode"}
                202 {set err "Settings lost due to RTL"}
                203 {set err "Input and output buffers full"}
                205 {set err "Argument out of range"}
                206 {set err "Group Execute Trigger ignored"}
                231 {set err "Not in calibrate mode"}
                232 {set err "Beyond calibration or null capability"}
                301 {set err "Interrupt fault"}
                302 {set err "System error"}
                303 {set err "Math pack error"}
                311 {set err "Converter time-out"}
                317 {set err "Front panel time-out"}
                318 {set err "Bad ohms calibration constant"}
                351 {set err "Calibration checksum error"}
                default {set err "Unknown instrument error"}
            }
            puts ">DM5010: ERROR $errCode: $err"
            return
        }

        # normal events

        # $busy would indicate the message processor is in a busy
        # state, at the moment.
        
        set code [expr "0b$event"]    ;# [scan] doesn't do this
        switch -- $code {
            1 {puts ">DM5010: Power-on detected."; dmm_init $chan}
            2 {
                # we could possibly use this to trigger a writable event
                # as we've just been told the last command finished.
                puts ">DM5010: Operation complete."
            }
            3 {puts ">DM5010: You pressed the 'inst id' key."}
            6 {puts ">DM5010: Over-range condition detected."}
            default { puts ">DM5010: Unhandled normal event code: $code" }
        }
    }
}

proc dmm_init {chan} {
    # Send it initial commands. RQS is the important one for enabling
    # interrupts.
    #
    set cmds {init; acv; dig 4.5; rqs on; opc on; monitor on; mode run}
    puts "<DM5010: $cmds"; puts $chan $cmds
}

dmm_init $dmm

if {![info exists tk_version] && ![info exists tcl_service]} {vwait forever}
 
results will be:
<DM5010: id?
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
