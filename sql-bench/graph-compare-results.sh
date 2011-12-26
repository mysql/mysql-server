#!/usr/bin/perl
####
#### Hello ... this is a heavily hacked script by Luuk 
#### instead of printing the result it makes a nice gif
#### when you want to look at the code ... beware of the 
#### ugliest code ever seen .... but it works ...
#### and that's sometimes the only thing you want ... isn't it ...
#### as the original script ... Hope you like it
####
#### Greetz..... Luuk de Boer 1997.
####

## if you want the seconds behind the bar printed or not ...
## or only the one where the bar is too big for the graph ...
## look at line 535 of this program and below ...
## look in sub calculate for allmost all hard/soft settings :-)

# a little program to generate a table of results
# just read all the RUN-*.log files and format them nicely
# Made by Luuk de Boer
# Patched by Monty

use Getopt::Long;
use GD;

$opt_server="mysql";
$opt_cmp="mysql,pg,solid";
$opt_cmp="msql,mysql,pg,solid";
$opt_cmp="empress,mysql,pg,solid";
$opt_dir="output";
$opt_machine="";
$opt_relative=$opt_same_server=$opt_help=$opt_Information=$opt_skip_count=0;

GetOptions("Information","help","server=s","cmp=s","machine=s","relative","same-server","dir=s","skip-count") || usage();

usage() if ($opt_help || $opt_Information);

if ($opt_same_server)
{
  $files="$opt_dir/RUN-$opt_server-*$opt_machine";
}
else
{
  $files="$opt_dir/RUN-*$opt_machine";
}
$files.= "-cmp-$opt_cmp" if (length($opt_cmp));

$automatic_files=0;
if ($#ARGV == -1)
{
  @ARGV=glob($files);
  $automatic_files=1;
}


#
# Go trough all RUN files and gather statistics.
#

foreach (@ARGV)
{
  $filename = $_;
  next if (defined($found{$_})); # remove duplicates
  $found{$_}=1;
  /RUN-(.*)$/;
  $prog = $1;
  push(@key_order,$prog);
  $next = 0;
  open(TMP, "<$filename") || die "Can't open $filename: $!\n";
  while (<TMP>)
  {
    chomp;
    if ($next == 0) {
      if (/Server version:\s+(\S+.*)/i)
      {
	$tot{$prog}{'server'} = $1;
      }
      elsif (/Arguments:\s+(.+)/i)
      {
	$tot{$prog}{'arguments'} = $1;
	# Remove some standard, not informative arguments
	$tot{$prog}{'arguments'} =~ s/--log|--use-old-results|--server=\S+|--cmp=\S+|--user=\S+|--pass=\S+|--machine=\S+//g;
	$tot{$prog}{'arguments'} =~ s/\s+/ /g;
      }
      elsif (/Comments:\s+(.+)/i) {
	$tot{$prog}{'comments'} = $1;
      } elsif (/^(\S+):\s*(estimated\s|)total\stime:\s+(\d+)\s+secs/i)
      {
	$tmp = $1; $tmp =~ s/://;
	$tot{$prog}{$tmp} = [ $3, (length($2) ? "+" : "")];
	$op1{$tmp} = $tmp;
      } elsif (/Totals per operation:/i) {
	$next = 1;
	next;
      }
    }
    elsif ($next == 1)
    {
      if (/^(\S+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*([+|?])*/)
      {
	$tot1{$prog}{$1} = [$2,$6,$7];
	$op{$1} = $1;
#print "TEST - $_ \n * $prog - $1 - $2 - $6 - $7 ****\n";
# $prog - filename
# $1 - operation
# $2 - time in secs
# $6 - number of loops
# $7 - nothing / + / ? / * => estimated time ...
      # get the highest value ....
      $highest = ($2/$6) if (($highest < ($2/$6)) && ($1 !~/TOTALS/i));
      $gifcount++;
      $giftotal += ($2/$6);
      }
    }
  }
}

if (!%op)
{
  print "Didn't find any files matching: '$files'\n";
  print "Use the --cmp=server,server option to compare benchmarks\n";
  exit 1;
}


# everything is loaded ...
# now we have to create a fancy output :-)

# I prefer to redirect scripts instead to force it to file ; Monty
#
# open(RES, ">$resultfile") || die "Can't write to $resultfile: $!\n";
# select(RES)
#

#print <<EOF;
#<cut for this moment>
#
#EOF

if ($opt_relative)
{
#  print "Column 1 is in seconds. All other columns are presented relative\n";
#  print "to this. 1.00 is the same, bigger numbers indicates slower\n\n";
}

#print "The result logs which where found and the options:\n";

if ($automatic_files)
{
  if ($key_order[$i] =~ /^$opt_server/)
  {
    if ($key_order[$i] =~ /^$opt_server/)
    {
      unshift(@key_order,$key_order[$i]);
      splice(@key_order,$i+1,1);
    }
  }
}
# extra for mysql and mysql_pgcc
#$number1 = shift(@key_order);
#$number2 = shift(@key_order);
#unshift(@key_order,$number1);
#unshift(@key_order,$number2);

# Print header

$column_count=0;
foreach $key (@key_order)
{
  $column_count++;
#  printf "%2d %-40.40s: %s %s\n", $column_count, $key,
#  $tot{$key}{'server'}, $tot{$key}{'arguments'};
#  print "Comments: $tot{$key}{'comments'}\n"
#    if ($tot{$key}{'comments'} =~ /\w+/);
}

#print "\n";

$namewidth=$opt_skip_count ? 20 :25;
$colwidth= $opt_relative ? 9 : 6;

print_sep("=");
#printf "%-$namewidth.${namewidth}s|", "Operation";
$count = 1;
foreach $key (@key_order)
{
#  printf "%${colwidth}d|", $count;
  $count++;
}
#print "\n";
#print_sep("-");
#print_string("Results per test:");
#print_sep("-");

foreach $key (sort {$a cmp $b} keys %op1)
{
#  printf "%-$namewidth.${namewidth}s|", $key;
  $first=undef();
  foreach $server (@key_order)
  {
    print_value($first,$tot{$server}{$key}->[0],$tot{$server}{$key}->[1]);
    $first=$tot{$server}{$key}->[0] if (!defined($first));
  }
#  print "\n";
}

print_sep("-");
print_string("The results per operation:");
print_sep("-");
$luukcounter = 1;
foreach $key (sort {$a cmp $b} keys %op)
{
  next if ($key =~ /TOTALS/i);
  $tmp=$key;
  $tmp.= " (" . $tot1{$key_order[0]}{$key}->[1] . ")" if (!$skip_count);
#  printf "%-$namewidth.${namewidth}s|", $tmp;
  $first=undef();
  foreach $server (@key_order)
  {
    print_value($first,$tot1{$server}{$key}->[0],$tot1{$server}{$key}->[2]);
    $first=$tot1{$server}{$key}->[0] if (!defined($first));
  }
#  print "\n";
  $luukcounter++;
}

#print_sep("-");
$key="TOTALS";
#printf "%-$namewidth.${namewidth}s|", $key;
$first=undef();
foreach $server (@key_order)
{
#  print_value($first,$tot1{$server}{$key}->[0],$tot1{$server}{$key}->[2]);
  $first=$tot1{$server}{$key}->[0] if (!defined($first));
}
#print "\n";
#print_sep("=");
&make_gif;

exit 0;

#
# some format functions;
#

sub print_sep
{
  my ($sep)=@_;
#  print $sep x ($namewidth + (($colwidth+1) * $column_count)+1),"\n";
}


sub print_value
{
  my ($first,$value,$flags)=@_;
  my ($tmp);

  if (defined($value))
  {
    if (!defined($first) || !$opt_relative)
    {
      $tmp=sprintf("%d",$value);
    }
    else
    {
      $first=1 if (!$first);	# Assume that it took one second instead of 0
      $tmp= sprintf("%.2f",$value/$first);
    }
    if (defined($flags))
    {
      $tmp="+".$tmp if ($flags =~ /\+/);
      $tmp="?".$tmp if ($flags =~ /\?/);
    }
  }
  else
  {
    $tmp="";
  }
  $tmp= " " x ($colwidth-length($tmp)) . $tmp if (length($tmp) < $colwidth);
#  print $tmp . "|";
}


sub print_string
{
  my ($str)=@_;
  my ($width);
  $width=$namewidth + ($colwidth+1)*$column_count;

  $str=substr($str,1,$width) if (length($str) > $width);
#  print($str," " x ($width - length($str)),"|\n");
}

sub usage
{
  exit(0);
}



###########################################
###########################################
###########################################
# making here a gif of the results ... (lets try it :-))
# luuk .... 1997
###########################################
## take care that $highest / $giftotal / $gifcount / $luukcounter 
## are getting there value above ... so don't forget them while 
## copying the code to some other program ....

sub make_gif {
  &gd; # some base things ....
  &legend; # make the nice legend
  &lines; # yep sometimes you have to print some lines
  &gif("gif/benchmark2-".$opt_cmp); # and finally we can print all to a gif file ...
}
##### mmm we are finished now ... 


# first we have to calculate some limits and some other stuff
sub calculate {
# here is the list which I have to know to make everything .....
# the small border width ... 					$sm_border = 
# the border default						$border = 
# the step default ... if it must be calculated then no value	$step =
# the highest number						$highest = 
# the max length of the text of the x borders			$max_len_lb=
# the max length of a legend entry				$max_len_le=
# number of entries in the legend				$num_legen =
# the length of the color blocks for the legend			$legend_block=
# the width of the gif ...if it must be calculated - no value   $width =
# the height of the gif .. if it must be calculated - no value	$height =
# the width of the grey field ' 	'	'	'	$width_grey=
# the height of the grey field '	'	'	'	$height_grey=
# number of dashed lines					$lines=
# if bars must overlap how much they must be overlapped		$overlap=
# titlebar title of graph in two colors big			$titlebar=
# titlebar1 sub title of graph in small font in black		$titlebar1=
# xlabel							$xlabel=
# ylabel							$ylabel=
# the name of the gif ...					$name=
# then the following things must be knows .....
# xlabel below or on the left side ?
# legend yes/no?
# where must the legend be placed?
# must the xlabel be printed horizontal or vertical?
# must the ylabel be printed horizontal or vertical?
# must the graph be a line or a bar graph?
# is a xlabel several different entries or some sub entries of one?
#    so xlabel 1 => test1=10, test2=15, test3=7 etc
#    or xlabel 1 => test1a=12, test1b=10, test1c=7 etc
# must the bars overlap (only with the second example I think)
# must the number be printed above or next to the bar?
# when must the number be printed .... only when it extends the graph ...???
# the space between the bars .... are that the same width of the bars ...
#    or is it a separate space ... defined ???
# must the date printed below or some where else ....

#calculate all space for text and other things ....
  $sm_border = 8; # the grey border around ...
  $border = 40; #default ...
  $left_border = 2.75 * $border; #default ...
  $right_border = $border; #default ...
  $up_border = $border; #default ...
  $down_border = $border; # default ...
  $step = ($height - $up_border - $down_border)/ ($luukcounter + (($#key_order + 1) * $luukcounter));
  # can set $step to get nice graphs ... and change the format ...
  $step = 8; # set hard the step value
  
  $gifavg = ($giftotal/$gifcount);
  $highest = 2 * $gifavg;
  $highest = 1; # set hard the highest value ...
  $xhigh = int($highest + .5 * $highest);
  
  # here to get the max lenght of the test entries ...
  # so we can calculate the with of the left border
  foreach $oper (sort keys (%op)) {
    $max_len_lb = length($oper) if (length($oper) > $max_len_lb);
#    print "oper = $oper - $max_len_lb\n";
  }
  $max_len_lb = $max_len_lb * gdSmallFont->width;
  $left_border = (3*$sm_border) + $max_len_lb;
  $down_border = (4*$sm_border) + (gdSmallFont->width*(length($xhigh)+3)) + (gdSmallFont->height *2); 
  $right_border = (3*$sm_border) + 3 + (gdSmallFont->width*(length($highest)+5));

  # calculate the space for the legend .....
  foreach $key (@key_order) {
    $tmp = $key;
    $tmp =~ s/-cmp-$opt_cmp//i;
    $giflegend = sprintf "%-24.24s: %-40.40s",$tmp,$tot{$key}{'server'};
    $max_len_le = length($giflegend) if (length($giflegend) > $max_len_le);
  }
  $max_len_le = $max_len_le * gdSmallFont->width;
  $legend_block = 10; # the length of the block in the legend
  $max_high_le = (($#key_order + 1)*(gdSmallFont->height+2)) + (2*$legend_block);
  $down_border += $max_high_le;
  $up_border = (5 * $sm_border) + gdSmallFont->height + gdLargeFont->height;
  
  print "Here some things we already know ....\n";
#  print "luukcounter = $luukcounter (number of tests)\n";
#  print "gifcount = $gifcount (number of total entries)\n";
#  print "giftotal = $giftotal (total secs)\n";
#  print "gifavg = $gifavg\n";
#  print "highest = $highest\n";
#  print "xhigh = $xhigh\n";
#  print "step = $step -- $#key_order\n";
#  print "max_len_lb = $max_len_lb\n";
#  printf "Small- width %d - height %s\n",gdSmallFont->width,gdSmallFont->height;
#  printf "Tiny- width %d - height %s\n",gdTinyFont->width,gdTinyFont->height;
}

sub gd {
  &calculate;
  $width = 600; # the width ....
  $height = 500; # the height ... 
  $width_greyfield = 430;
  # when $step is set ... count the height ....????
  $width = $width_greyfield + $left_border + $right_border;
  $height = ($step * ($luukcounter + ($luukcounter * ($#key_order + 1)))) + $down_border + $up_border;
  $b_width = $width - ($left_border + $right_border); # width within the grey field
  $overlap = 0; # how far each colum can fall over each other ...nice :-)

  # make the gif image ....
  $im = new GD::Image($width,$height);

  # allocate the colors to use ...
  $white 		= $im->colorAllocate(255,255,255);
  $black 		= $im->colorAllocate(0,0,0);
  $paper_white 		= $im->colorAllocate(220, 220, 220);
  $grey1 		= $im->colorAllocate(240, 240, 240);
  $grey4 		= $im->colorAllocate(229, 229, 229);
  $grey2 		= $im->colorAllocate(102, 102, 102);
  $grey3 		= $im->colorAllocate(153, 153, 153);
  
  $red 			= $im->colorAllocate(205,0,0); # msql
  $lred 		= $im->colorAllocate(255,0,0);
  $blue 		= $im->colorAllocate(0,0,205); # mysql
  $lblue 		= $im->colorAllocate(0,0,255); # mysql_pgcc
  $green 		= $im->colorAllocate(0, 205, 0); # postgres
  $lgreen 		= $im->colorAllocate(0, 255, 0); # pg_fast
  $orange 		= $im->colorAllocate(205,133, 0); # solid
  $lorange 		= $im->colorAllocate(255, 165, 0); # Adabas
  $yellow 		= $im->colorAllocate(205,205,0);  # empress
  $lyellow 		= $im->colorAllocate(255,255,0);
  $magenta 		= $im->colorAllocate(255,0,255); # oracle
  $lmagenta 		= $im->colorAllocate(255,200,255);
  $cyan 		= $im->colorAllocate(0,205,205); # sybase
  $lcyan 		= $im->colorAllocate(0,255,255);
  $sienna 		= $im->colorAllocate(139,71,38); # db2
  $lsienna 		= $im->colorAllocate(160,82,45);
  $coral 		= $im->colorAllocate(205,91,69); # Informix
  $lcoral 		= $im->colorAllocate(255,114,86);
  $peach		= $im->colorAllocate(205,175,149);
  $lpeach		= $im->colorAllocate(255,218,185);
  
  @colors = ($red, $blue, $green, $orange, $yellow, $magenta, $cyan, $sienna, $coral, $peach);
  @lcolors = ($lred, $lblue, $lgreen, $lorange, $lyellow, $lmagenta, $lcyan, $lsienna, $lcoral, $lpeach);

  # set a color per server so in every result it has the same color ....
  foreach $key (@key_order) {
    if ($tot{$key}{'server'} =~ /mysql/i) {
      if ($key =~ /mysql_pgcc/i || $key =~ /mysql_odbc/i || $key =~ /mysql_fast/i) {
        $tot{$key}{'color'} = $lblue;
      } else {
        $tot{$key}{'color'} = $blue;
      }
    } elsif ($tot{$key}{'server'} =~ /msql/i) {
      $tot{$key}{'color'} = $lred;
    } elsif ($tot{$key}{'server'} =~ /postgres/i) {
      if ($key =~ /pg_fast/i) {
        $tot{$key}{'color'} = $lgreen;
      } else {
        $tot{$key}{'color'} = $green;
      }
    } elsif ($tot{$key}{'server'} =~ /solid/i) {
      $tot{$key}{'color'} = $lorange;
    } elsif ($tot{$key}{'server'} =~ /empress/i) {
      $tot{$key}{'color'} = $lyellow;
    } elsif ($tot{$key}{'server'} =~ /oracle/i) {
      $tot{$key}{'color'} = $magenta;
    } elsif ($tot{$key}{'server'} =~ /sybase/i) {
      $tot{$key}{'color'} = $cyan;
    } elsif ($tot{$key}{'server'} =~ /db2/i) {
      $tot{$key}{'color'} = $sienna;
    } elsif ($tot{$key}{'server'} =~ /informix/i) {
      $tot{$key}{'color'} = $coral;
    } elsif ($tot{$key}{'server'} =~ /microsoft/i) {
      $tot{$key}{'color'} = $peach;
    } elsif ($tot{$key}{'server'} =~ /access/i) {
      $tot{$key}{'color'} = $lpeach;
    } elsif ($tot{$key}{'server'} =~ /adabas/i) {
      $tot{$key}{'color'} = $lorange;
    }
  }

  # make the nice little borders
  # left bar
  $poly0 = new GD::Polygon;
  $poly0->addPt(0,0);
  $poly0->addPt($sm_border,$sm_border);
  $poly0->addPt($sm_border,($height - $sm_border));
  $poly0->addPt(0,$height);
  $im->filledPolygon($poly0,$grey1);
  $im->polygon($poly0, $grey4);
  # upper bar
  $poly3 = new GD::Polygon;
  $poly3->addPt(0,0);
  $poly3->addPt($sm_border,$sm_border);
  $poly3->addPt(($width - $sm_border),$sm_border);
  $poly3->addPt($width,0);
  $im->polygon($poly3, $grey4);
  $tmptime = localtime(time);
  $im->string(gdSmallFont,($width - $sm_border - (gdSmallFont->width * length($tmptime))),($height - ($sm_border) - gdSmallFont->height), $tmptime, $grey3);
  
  # right bar
  $poly1 = new GD::Polygon;
  $poly1->addPt($width,0);
  $poly1->addPt(($width - $sm_border),$sm_border);
  $poly1->addPt(($width - $sm_border),($height - $sm_border));
  $poly1->addPt($width,$height);
  $im->filledPolygon($poly1, $grey3);
  $im->stringUp(gdSmallFont,($width - 10),($height - (2 * $sm_border)), "Made by Luuk de Boer - 1997 (c)", $blue);
  #below bar
  $poly2 = new GD::Polygon;
  $poly2->addPt(0,$height);
  $poly2->addPt($sm_border,($height - $sm_border));
  $poly2->addPt(($width - $sm_border),($height - $sm_border));
  $poly2->addPt($width,$height);
  $im->filledPolygon($poly2, $grey2);
  
  # do the black line around where in you will print ... (must be done at last
  # but is hard to develop with ... but the filled grey must be done first :-)
  $im->filledRectangle($left_border,$up_border,($width - ($right_border)),($height-$down_border),$grey4);


  # print the nice title ...
  $titlebar = "MySQL Benchmark results"; # head title ...
  $titlebar1 = "Compare $opt_cmp "; # sub title
  $header2 = "seconds/test"; # header value
  $center = ($width / 2) - ((gdLargeFont->width * length($titlebar)) / 2);
  $center1 = ($width / 2) - ((gdSmallFont->width * length($titlebar1)) / 2);
  $center2 = ($width_greyfield/2) - ((gdSmallFont->width*length($header2))/2);
  $bovenkant = $sm_border * 3;
  $bovenkant1 = $bovenkant + gdLargeFont->height + (.5*$sm_border);
  $bovenkant2 = $height - $down_border + (1*$sm_border) + (gdSmallFont->width*(length($xhigh)+3)); 
  $im->string(gdLargeFont,($center),($bovenkant + 1), $titlebar, $grey3);
  $im->string(gdLargeFont,($center),($bovenkant), $titlebar, $red);
  $im->string(gdSmallFont,($center1),($bovenkant1), $titlebar1, $black);
  $im->string(gdSmallFont,($left_border + $center2),($bovenkant2), $header2, $black);

  $xlength = $width - $left_border - $right_border;
  $lines = 10; # hard coded number of dashed lines
  $xverh = $xlength / $xhigh;
#  print " de verhouding ===> $xverh --- $xlength -- $xhigh \n";

  $xstep = ($xhigh / $lines) * $xverh;
  $teller = 0;
  # make the nice dashed lines and print the values ...
  for ($i = 0; $i <= $lines; $i++) {
    $st2 = ($left_border) + ($i * $xstep);
    $im->dashedLine($st2,($height-$down_border),$st2,($up_border), $grey3);
    if (($i != 0) && ($teller == 2)) {
      $st3 = sprintf("%.2f", $i*($xhigh/$lines));
      $im->stringUp(gdTinyFont,($st2 - (gdSmallFont->height/2)),($height - $down_border +(.5*$sm_border) + (gdSmallFont->width*(length($xhigh)+3))), $st3, $black);
      $teller = 0;
    }
    $teller++;
  }
  $im->rectangle($left_border,$up_border,($width - ($right_border)),($height-$down_border),$black);
}

sub legend {
  # make the legend ...
  $legxbegin = $left_border;

  $legybegin = $height - $down_border + (2*$sm_border) + (gdSmallFont->width * (length($xhigh) + 3)) + gdSmallFont->height;
  $legxend = $legxbegin + $max_len_le + (4*$legend_block);
  $legxend = $legxbegin + $width_greyfield;
  $legyend = $legybegin + $max_high_le;
  $im->filledRectangle($legxbegin,$legybegin,$legxend,$legyend,$grey4);
  $im->rectangle($legxbegin,$legybegin,$legxend,$legyend,$black);
  # calculate the space for the legend .....
  $c = 0; $i = 1;
  $legybegin += $legend_block;
  foreach $key (@key_order) {
    $xtmp = $legxbegin + $legend_block;
    $ytmp = $legybegin + ($c * (gdSmallFont->height +2));
    $xtmp1 = $xtmp + $legend_block;
    $ytmp1 = $ytmp + gdSmallFont->height;
    $im->filledRectangle($xtmp,$ytmp,$xtmp1,$ytmp1,$tot{$key}{'color'});
    $im->rectangle($xtmp,$ytmp,$xtmp1,$ytmp1,$black);
    $tmp = $key;
    $tmp =~ s/-cmp-$opt_cmp//i;
    $giflegend = sprintf "%-24.24s: %-40.40s",$tmp,$tot{$key}{'server'};
    $xtmp2 = $xtmp1 + $legend_block;
    $im->string(gdSmallFont,$xtmp2,$ytmp,"$giflegend",$black);
    $c++;
    $i++;
#    print "$c $i -> $giflegend\n";
  }
  
}

sub lines {

  $g = 0;
  $i = 0;
  $ybegin = $up_border + ((($#key_order + 2)/2)*$step);
  $xbegin = $left_border;
  foreach $key (sort {$a cmp $b} keys %op) {
    next if ($key =~ /TOTALS/i);
    $c = 0;
#    print "key - $key\n";
    foreach $server (@key_order) {
      $tot1{$server}{$key}->[1] = 1 if ($tot1{$server}{$key}->[1] == 0);
      $entry = $tot1{$server}{$key}->[0]/$tot1{$server}{$key}->[1];
      $ytmp = $ybegin + ($i * $step) ;
      $xtmp = $xbegin + ($entry * $xverh) ;
      $ytmp1 = $ytmp + $step;
#      print "$server -- $entry --x $xtmp -- y $ytmp - $c\n";
      $entry1 = sprintf("%.2f", $entry);
      if ($entry < $xhigh) {
        $im->filledRectangle($xbegin, $ytmp, $xtmp, $ytmp1, $tot{$server}{'color'});
        $im->rectangle($xbegin, $ytmp, $xtmp, $ytmp1, $black);
# print the seconds behind the bar (look below for another entry)
# this entry is for the bars that are not greater then the max width
# of the grey field ...
#        $im->string(gdTinyFont,(($xtmp+3),($ytmp),"$entry1",$black));
# if you want the seconds in the color of the bar just uncomment it (below)
#        $im->string(gdTinyFont,(($xtmp+3),($ytmp),"$entry1",$tot{$server}{'color'}));
      } else {
        $im->filledRectangle($xbegin, $ytmp, ($xbegin + ($xhigh*$xverh)), $ytmp1, $tot{$server}{'color'});
        $im->rectangle($xbegin, $ytmp, ($xbegin + ($xhigh*$xverh)), $ytmp1, $black);

# print the seconds behind the bar (look below for another entry)
# here is the seconds printed behind the bar is the bar is too big for 
# the graph ... (seconds is greater then xhigh ...)
        $im->string(gdTinyFont, ($xbegin + ($xhigh*$xverh)+3),($ytmp),"$entry1",$black);
# if you want the seconds in the color of the bar just uncomment it (below)
#        $im->string(gdTinyFont, ($xbegin + ($xhigh*$xverh)+3),($ytmp),"$entry1",$colors[$c]);
      }
      $c++;
      $i++;
    }
    # see if we can center the text between the bars ...
    $ytmp2 = $ytmp1 - (((($c)*$step) + gdSmallFont->height)/2);
    $im->string(gdSmallFont,($sm_border*2),$ytmp2,$key, $black);
    $i++;
  }
}


sub gif {
  my ($name) = @_;
  $name_gif = $name . ".gif";
  print "name --> $name_gif\n";
  open (GIF, "> $name_gif") || die "Can't open $name_gif: $!\n";
  print GIF $im->gif;
  close (GIF);
}

