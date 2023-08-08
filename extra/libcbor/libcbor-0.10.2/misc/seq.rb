#!/usr/bin/env ruby

Integer($*[0]).times {|i| print "0x%02X, " % ((Integer($*[1]) + i) % 0xFF) }
