#!/usr/bin/env ruby

puts $*[0][2..-1].split('').each_slice(2).map {|_| '0x%02X' % _.join.to_i(16) }.join(', ')
