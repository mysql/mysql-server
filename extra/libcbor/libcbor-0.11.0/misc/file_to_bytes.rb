#!/usr/bin/env ruby

lst = (ARGV.empty? ? STDIN.read : IO.binread(ARGV[0])).bytes.map {|_| '0x%02X' % _ }
puts lst.size
puts lst.join(', ')
