# initial test script for 'async GPIB'
# using a Tektronix DM 5010
#
# author: David Gravereaux <davygrvy@pobox.com>

set verbose yes

package require agpib 1.0

set DMM [aGPIB::open -board 0 -sad 16 -timeout 1ms -eoi yes]
fconfigure $DMM -encoding ascii -translation binary
fileevent $DMM readable [list readDMM $DMM]

proc readDMM {chan} {
    switch -- {[dm5010_spoll_parse [fconfigure $chan -spoll]]} {
        err {
            # get a more detailed error code
            puts $chan {ERR?}
	    # do a blocking read
	    puts "<DM5010: ERROR: [list join [dm5010_equery_parse [read $chan]]]"
        }
        power_on {
            DMM_init $chan
        }
	btn_press {
	    puts "<DM5010: BOINK! you pressed the 'INST ID' key"
	}
        data {
            DMM_parse [read $chan]
        }
    }
}

proc DMM_init {chan} {
    global verbose
	
    if {$verbose} {
	puts $chan {ID?}
    	# should be "ID TEK/DM5010,V79.1 Fxx"
    	# where xx is the firmware version
    	puts "<DM5010: [read $chan]"
    }

    # RQS enables interupt mode
    puts $chan {RQS ON;OPC ON;OVER ON;USER ON}
    # make it go
    puts $chan {DIG 4.5;ACV;MODE RUN}
}

# parse serial poll.  This is device specific to a DM5010
#
proc dm5010_spoll_parse {code} {

    # bit 5: The message processor is busy
    if {int(pow(2,4)) & $code} {return "busy"}

    switch -- {[expr int(pow(2,4)) ^ $code]} {
	97 - 99 {return "err"}
	65  {return "power_on"}
	66  {return "complete"}
	67  {return "btn_press"}
        102 {return "over"}
        193 {return "below"}
        195 {return "above"}
	132 - 140 {return "data"}
    }
    error "case not handled: $code"
}

# parse ERR? command result
#
proc dm5010_equery_parse {reply} {
    set code [string trimright \
	[string trimleft $reply {ERR  }] {;}]

    # these are the device specific errors
    switch -- $code {
	0 {set msg "No error"}

	101 {set msg "Invalid command header"}
	102 {set msg "Header delimiter error"}
	103 {set msg "Argument error"}
	104 {set msg "Argument delimiter error"}
	106 {set msg "Missing argument"}
	107 {set msg "Invalid message unit delimiter"}

	201 {set msg "Not exutable in local mode"}
	202 {set msg "Settings lost due to rtl"}
	203 {set msg "Input and Output buffers full"}
	205 {set msg "Argument out of range"}
	206 {set msg "Group Execute Trigger ignored"}
	231 {set msg "Not in calibrate mode"}
	232 {set msg "Beyond calibration or null capability"}

	301 {set msg "Interrupt fault"}
	302 {set msg "System error"}
	303 {set msg "Math pack error"}
	311 {set msg "Converter time-out"}
	317 {set msg "Front panel timeout"}
	318 {set msg "Bad ohmt calibration constant"}
	351 {set msg "Calibration chksum error"}

	401 {set msg "Power on"}
	402 {set msg "Operation complete"}
	403 {set msg "User ID request"}

	601 {set msg "Over-range"}

	701 {set msg "Below limits"}
	702 {set msg "Above limits"}
    }
    return [list $code $msg]
}

proc DMM_parse {data} {
    # TODO
    puts "<DM5010: $data"
}

DMM_init $DMM


