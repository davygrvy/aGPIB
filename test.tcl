package require gpib

# one third octave bands
set freqs [list \
    15.625 19.686 24.803 31.25 39.373 49.606 62.5 78.745 99.213 125 \
    157.49 198.425 250 314.98 396.85 500 629.961 793.701 1000 \
    1259.921 1587.401 2000 2519.842 3174.802 4000 5039.684 6349.604 \
    8000 10079.368 12699.208 16000 20158.737 \
]

namespace eval ::Format:: {
    variable decimal    "."
    variable thousands  ","

    namespace export setDecimalChars dformat
 }


 # setDecimalChars --
 #    Set the decimal characters (decimal and thousands separators)
 # Arguments:
 #    decimal_char    Character to use for decimals (default: .)
 #    thousands_char  Character to use for thousands (default: nothing)
 # Result:
 #    Nothing
 # Side effect:
 #    Private variables set
 # Note:
 #    To do: simple sanity checks
 #
 proc ::Format::setDecimalChars { {decimal_char .} {thousands_char "" } } {
    variable decimal
    variable thousands

    set decimal   $decimal_char
    set thousands $thousands_char
 }

 # DecFormat --
 #    Private routine to handle the decimal characters
 # Arguments:
 #    format    Single numerical format string
 #    value     Single numerical value
 # Result:
 #    Correctly formatting string
 #
 proc ::Format::DecFormat { format value } {
    variable decimal
    variable thousands

    set string  [format $format $value]
    set posdot  [string first "." $string]
    if { $posdot >= 0 } {
       set indices [regexp -inline -indices {([0-9]+)\.} $string]
    } else {
       set indices [regexp -inline -indices {([0-9]+)} $string]
    }
    foreach {first last} [lindex $indices 1] {break}
  #  puts "$first $last -- $string ($indices)"
    set prefix  [string range $string $first $last]
    set idx     [expr {[string length $prefix]-3}]
    while { $idx > 0 } {
       set posidx [string index $prefix $idx]
       set prefix [string replace $prefix $idx $idx "$thousands$posidx"]
       incr idx -3
    }

    #
    # Be careful with these replacements: otherwise interference
    # may occur
    #
    set string [string replace $string $posdot $posdot $decimal]
    set string [string replace $string $first $last $prefix]
    return $string
 }

 # dformat --
 #    Format the given variables according to the format string
 #    keeping in mind the current decimal settings
 # Arguments:
 #    format_string   String containing formats
 #    args	    Values to be formatted
 # Result:
 #    Formatted string
 # Note:
 #    No support for %*s and the like
 #    To do: error checks
 #
 proc ::Format::dformat { format_string args } {

    set codes_re {%[^%cdfegsx]*[%cdefgsx]}
    set codes      [regexp -all -inline -indices $codes_re $format_string]
    regsub -all $codes_re $format_string "%s" new_format

    set idx 0
    set vars {}
    foreach code $codes {
       foreach {start stop} $code {break}
       set substr [string range $format_string $start $stop]
  #     puts "$code -- $substr -- [lindex $args $idx]"
       if { $substr == "%%" } {
	  lappend vars "%"
	  continue
       }
       set value [lindex $args $idx]
       if { [string first [string index $substr end] "defg"] >= 0 } {
	  set result [DecFormat $substr $value]
       } else {
	  set result [format $substr $value]
       }
       lappend vars $result
  #     puts $vars
       incr idx
    }

    return [eval format [list $new_format] $vars]
 }

namespace import -force ::Format::*


proc readDist {dev} {
    set badval "1.E+99"

    # clear status bytes and SRQ
    gpib clear -device $dev

    # ask for distortion measure starting now.
    gpib write -device $dev -message {send}

    # blocks until settled
    gpib wait -device $dev -event 144

    # timeout value?  If so, do it again.
    while {[set value [string trimright \
	    [gpib read -device $dev] {;}]] eq $badval} {}

    return [format %.4f $value]
}

set osc [gpib open -address 25]
set dist [gpib open -address 4 -timeout 8]

# enable into phase lock blocking
gpib write -device $osc -message \
	{init;pli on;disp freq;func sine;vrms 1;out off}
# enable delayed reading until settled
gpib write -device $dist -message \
	{init;dus on;func thdpct;filt lp;points 4;tol 1.5;counts 4}

# pause to let the above actions complete
after 500

# clear the powered on result of 65
if {[gpib serialpoll -device $osc] == 65} {
    puts "osc just started"
}

gpib serialpoll -device $dist

gpib write -device $osc -message {out on}

foreach freq $freqs {
    # change frequency
    gpib clear -device $osc
    gpib write -device $osc -message "freq $freq"
    # wait until PLL is locked
    gpib wait -device $osc -event 195
    foreach level {1 10} {
	gpib write -device $osc -message "vrms $level"
	set percent [readDist $dist]
#	gpib write -device $osc -message {freq?}
#	set hertz [format {%8.2f} [string range [string trimright \
#		[gpib read -device $osc] {;}] 5 end]]
	puts "${percent}% for ${freq}Hz at ${level}Vrms"
    }
}

gpib write -device $osc -message {out off}
gpib close -device all

