#!/usr/bin/env ruby

(Integer($*[0])..Integer($*[1])).each {|i| puts "case 0x%02X:" % i}
