# use 5.010000;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Net::HandlerSocket',
    VERSION_FROM      => 'lib/Net/HandlerSocket.pm', # finds $VERSION
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'lib/Net/HandlerSocket.pm', # retrieve abstract from module
       AUTHOR         => 'higuchi dot akira at dena dot jp>') : ()),
    CC                => 'g++ -fPIC',
    LD                => 'g++ -fPIC',
    LIBS              => ['-lhsclient'], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I. -I/usr/include/handlersocket',
    OPTIMIZE          => '-g -O3 -Wall -Wno-unused',
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
);
