package My::Suite::CTest;
use Cwd;

@ISA = qw(My::Suite);

sub list_cases {
  my ($self) = @_;
  keys %{$self->{ctests}}
}

sub start_test {
  my ($self, $tinfo)= @_;
  my $args=[ ];

  my $oldpwd=getcwd();
  chdir $::opt_vardir;
  my $proc=My::SafeProcess->new
           (
            name          => $tinfo->{shortname},
            path          => $self->{ctests}->{$tinfo->{shortname}},
            args          => \$args,
            append        => 1,
            output        => $::path_current_testlog,
            error         => $::path_current_testlog,
           );
  chdir $oldpwd;
  $proc;
}

{ 
  return "Not run for embedded server" if $::opt_embedded_server;
  return "Not configured to run ctest" unless -f "../CTestTestfile.cmake";
  my ($ctest_vs)= $opt_vs_config ? "--build-config $opt_vs_config" : "";
  my (@ctest_list)= `cd .. && ctest $opt_vs_config --show-only --verbose`;
  return "No ctest" if $?;

  my ($command, %tests);
  for (@ctest_list) {
    chomp;
    $command= $' if /^\d+: Test command: +/;
    $tests{$'}=$command if /^ +Test +#\d+: +/;
  }
  bless { ctests => { %tests } };
}
