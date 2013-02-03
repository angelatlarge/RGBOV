#!/usr/bin/ruby
require 'rubygems'
require 'chunky_png'

COLOR_BITS=2
COLOR_DIVISOR = 256.0 / (2**(COLOR_BITS-1))

class Integer
  def hexstring(minchars = 2, include_0x = true)
    (include_0x ? "0x" : "") + self.to_s(16).upcase.rjust(minchars,'0')
  end
end

def color_map(pixel)
  red = (((pixel & 0xff000000) >> 24) / COLOR_DIVISOR).ceil
  green = (((pixel & 0x00ff0000) >> 16) / COLOR_DIVISOR).ceil
  blue = (((pixel & 0x0000ff00) >> 8) / COLOR_DIVISOR).ceil
  red << (COLOR_BITS*2) | green << COLOR_BITS | blue
end

pixel_data = ChunkyPNG::Image.from_file(ARGV[0])

pixels = pixel_data.pixels.map{ |p| color_map(p) }

puts "{\n{" + pixels.map(&:hexstring).each_slice(pixel_data.width).map{ |line| line.join(", ")}.join("},\n{") + "}\n}"

