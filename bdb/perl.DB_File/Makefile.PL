#! perl -w
use strict ;
use ExtUtils::MakeMaker 5.16 ;
use Config ;

my $VER_INFO ;
my $LIB_DIR ;
my $INC_DIR ;
my $DB_NAME ;
my $LIBS ;
my $COMPAT185 = "" ;

my @files = ('DB_File.pm', glob "t/*.t") ;
# See if warnings is available
eval 'use warnings;';
if ($@) {
    # not there, so write a dummy warnings.pm
    oldWarnings(@files) ;
} else {
    # is there, 
    newWarnings(@files) ;
}

ParseCONFIG() ;

if (defined $DB_NAME)
  { $LIBS = $DB_NAME }
else {
    if ($^O eq 'MSWin32')
      { $LIBS = '-llibdb' }
    else
      { $LIBS = '-ldb' }
}

# Solaris is special.
#$LIBS .= " -lthread" if $^O eq 'solaris' ;

# OS2 is a special case, so check for it now.
my $OS2 = "" ;
$OS2 = "-DOS2" if $Config{'osname'} eq 'os2' ;

WriteMakefile(
	NAME 		=> 'DB_File',
	LIBS 		=> ["-L${LIB_DIR} $LIBS"],
        MAN3PODS        => ' ',         # Pods will be built by installman.
	INC		=> "-I$INC_DIR",
	VERSION_FROM	=> 'DB_File.pm',
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> "$OS2 $VER_INFO $COMPAT185",
	OBJECT		=> 'version$(OBJ_EXT) DB_File$(OBJ_EXT)',
	OPTIMIZE	=> '-g',
	'macro'		=> { INSTALLDIRS => 'perl' },
        'dist'          => {COMPRESS=>'gzip', SUFFIX=>'gz'},    
	);


sub MY::postamble {
	'
version$(OBJ_EXT):	version.c

$(NAME).xs:	typemap
	@$(TOUCH) $(NAME).xs

Makefile:	config.in

' ;
}


sub ParseCONFIG
{
    my ($k, $v) ;
    my @badkey = () ;
    my %Info = () ;
    my @Options = qw( INCLUDE LIB PREFIX HASH DBNAME COMPAT185 ) ;
    my %ValidOption = map {$_, 1} @Options ;
    my %Parsed = %ValidOption ;
    my $CONFIG = 'config.in' ;

    print "Parsing $CONFIG...\n" ;

    # DBNAME & COMPAT185 are optional, so pretend they  have 
    # been parsed.
    delete $Parsed{'DBNAME'} ;
    delete $Parsed{'COMPAT185'} ;
    $Info{COMPAT185} = "No" ;


    open(F, "$CONFIG") or die "Cannot open file $CONFIG: $!\n" ;
    while (<F>) {
	s/^\s*|\s*$//g ;
	next if /^\s*$/ or /^\s*#/ ;
	s/\s*#\s*$// ;

	($k, $v) = split(/\s+=\s+/, $_, 2) ;
	$k = uc $k ;
	if ($ValidOption{$k}) {
	    delete $Parsed{$k} ;
	    $Info{$k} = $v ;
	}
	else {
	    push(@badkey, $k) ;
	}
    }
    close F ;

    print "Unknown keys in $CONFIG ignored [@badkey]\n"
	if @badkey ;

    # check parsed values
    my @missing = () ;
    die "The following keys are missing from $CONFIG file: [@missing]\n" 
        if @missing = keys %Parsed ;

    $INC_DIR = $ENV{'DB_FILE_INCLUDE'} || $Info{'INCLUDE'} ;
    $LIB_DIR = $ENV{'DB_FILE_LIB'} || $Info{'LIB'} ;
    $DB_NAME = $Info{'DBNAME'} if defined $Info{'DBNAME'} ;
    $COMPAT185 = "-DCOMPAT185 -DDB_LIBRARY_COMPATIBILITY_API" 
        if (defined $ENV{'DB_FILE_COMPAT185'} && 
		$ENV{'DB_FILE_COMPAT185'} =~ /^\s*(on|true|1)\s*$/i) ||
		$Info{'COMPAT185'} =~ /^\s*(on|true|1)\s*$/i ; 
    my $PREFIX  = $Info{'PREFIX'} ;
    my $HASH    = $Info{'HASH'} ;

    $VER_INFO = "-DmDB_Prefix_t=${PREFIX} -DmDB_Hash_t=${HASH}" ;

    print <<EOM if 0 ;
    INCLUDE	[$INC_DIR]
    LIB		[$LIB_DIR]
    HASH	[$HASH]
    PREFIX	[$PREFIX]
    DBNAME      [$DB_NAME]

EOM

    print "Looks Good.\n" ;

}

sub oldWarnings
{
    local ($^I) = ".bak" ;
    local (@ARGV) = @_ ;

    while (<>)
    {
	if (/^__END__/)
	{
	    print ;
	    my $this = $ARGV ;
    	    while (<>)
	    {
		last if $ARGV ne $this ;
		print ;
	    }
	}

	s/^(\s*)(no\s+warnings)/${1}local (\$^W) = 0; #$2/ ;
	s/^(\s*)(use\s+warnings)/${1}local (\$^W) = 1; #$2/ ;
	print ;
    }
}

sub newWarnings
{
    local ($^I) = ".bak" ;
    local (@ARGV) = @_ ;

    while (<>)
    {
	if (/^__END__/)
	{
	    my $this = $ARGV ;
	    print ;
    	    while (<>)
	    {
		last if $ARGV ne $this ;
		print ;
	    }
	}

	s/^(\s*)local\s*\(\$\^W\)\s*=\s*\d+\s*;\s*#\s*((no|use)\s+warnings.*)/$1$2/ ;
	print ;
    }
}

# end of file Makefile.PL
