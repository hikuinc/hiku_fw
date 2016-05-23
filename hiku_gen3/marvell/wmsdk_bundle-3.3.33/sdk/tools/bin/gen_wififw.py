#!/usr/bin/env python
# Copyright (C) 2015 Marvell International Ltd.
# All Rights Reserved.

# Wifi firmware generation script
# This script prefixes input wifi firmware bianry with 'WLFW' followed by firmware
# length in little endian. The output wifi firmware can be used for direct flashing
# such as openocd flash commands.

import sys, getopt, struct

def gen_wififw(ip_file, op_file):
    with open(ip_file, 'rb') as ifile:
        wififw = ifile.read()

    with open(op_file, 'wb') as ofile:
        ofile.write("\x57\x4c\x46\x57")
        ofile.write(struct.pack('<L', len(wififw)))
        ofile.write(wififw)

def print_usage():
    print ""
    print "Usage:"
    print sys.argv[0] + " [<-i | --infile> <input/wififw/bin>] [<-o | --outfile> <output/wififw/bin>]"
    print "          Converts input wifi firmware into wifi firmware ready for direct flashing"
    print ""
    print "Optional Usage:"
    print " [-h | --help]"
    print "          Display usage"
    sys.stdout.flush()

if __name__ == "__main__":
    ip_file = ''
    op_file = ''

    try:
        opts, args = getopt.gnu_getopt(sys.argv[1:], "i:o:h",["infile=","outfile=","help"])
        if len(args):
            print_usage()
            sys.exit()
    except getopt.GetoptError as e:
        print e
        print_usage()
        sys.exit()

    for opt, arg in opts:
        if opt in ("-i", "--infile"):
            ip_file = arg
        elif opt in ("-o", "--outfile"):
            op_file = arg
        elif opt in ("-h", "--help"):
            print_usage()
            sys.exit()

    if not ip_file or not op_file:
        print_usage()
        sys.exit()

    try:
        gen_wififw(ip_file, op_file)
    except Exception as e:
        print e
        sys.exit()
