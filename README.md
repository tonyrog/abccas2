# abccas2
ABC 80 data casette audio transmitter

## usage: abccas2 [<options>] [<file>[.bas|.bac|.*]]
### OPTIONS
         -h             display help and exit
         -v             verbose
         -k             translate \n to \r
         -b 700|2400    baud rate (700)
         -r >1400       audio file sample rate (11200)
         -f wav|au|raw  audio format (wav)
         -z 8|16|24|32  bits per channel (8)
         -o <filename>  audio output filename (stdout)
