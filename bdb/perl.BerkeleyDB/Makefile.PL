#! perl -w

# It should not be necessary to edit this file. The configuration for
# BerkeleyDB is controlled from the file config.in


BEGIN { die "BerkeleyDB needs Perl 5.004_04 or greater" if $] < 5.004_04 ; }

use strict ;
use ExtUtils::MakeMaker ;

my $LIB_DIR ;
my $INC_DIR ;
my $DB_NAME ;
my $LIBS ;

ParseCONFIG() ;

if (defined $DB_NAME) 
  { $LIBS = $DB_NAME }
else { 
    if ($^O eq 'MSWin32')
      { $LIBS = '-llibdb' }
    else
      { $LIBS = '-ldb' }
}

# OS2 is a special case, so check for it now.
my $OS2 = "" ;
$OS2 = "-DOS2" if $^O eq 'os2' ;

WriteMakefile(
	NAME 		=> 'BerkeleyDB',
	LIBS 		=> ["-L${LIB_DIR} $LIBS"],
        MAN3PODS        => ' ',         # Pods will be built by installman.
	INC		=> "-I$INC_DIR",
	VERSION_FROM	=> 'BerkeleyDB.pm',
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> "$OS2",
	#'macro'		=> { INSTALLDIRS => 'perl' },
        'dist'          => {COMPRESS=>'gzip', SUFFIX=>'gz'},    
	($] >= 5.005
	    ? (ABSTRACT_FROM	=> 'BerkeleyDB.pod',
	       AUTHOR  	=> 'Paul Marquess <Paul.Marquess@btinternet.com>')
	    : ()
	),
	);


sub MY::postamble {
	'
$(NAME).pod:	$(NAME).pod.P t/examples.t.T t/examples3.t.T mkpod
	perl ./mkpod

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
    my @Options = qw( INCLUDE LIB DBNAME ) ;
    my %ValidOption = map {$_, 1} @Options ;
    my %Parsed = %ValidOption ;
    my $CONFIG = 'config.in' ;

    print "Parsing $CONFIG...\n" ;

    # DBNAME is optional, so pretend it has been parsed.
    delete $Parsed{'DBNAME'} ;

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

    $INC_DIR =  $ENV{'BERKELEYDB_INCLUDE'} || $Info{'INCLUDE'} ;
    $LIB_DIR =  $ENV{'BERKELEYDB_LIB'} || $Info{'LIB'} ;
    $DB_NAME = $Info{'DBNAME'} if defined $Info{'DBNAME'} ;
    print "Looks Good.\n" ;

}

# end of file Makefile.PL
