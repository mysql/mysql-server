#!@PERL@
# ****************************
package MySQLaccess;
#use strict;
use POSIX qw(tmpnam);
use Fcntl;

BEGIN {
	# ****************************
	# static information...
	$VERSION     = "2.06, 20 Dec 2000";
	$0           =~ m%/([^/]+)$%o;
	$script      = $1;
        $script      = 'MySQLAccess' unless $script;
	$script_conf = "$script.conf";
	$script_log  = "~/$script.log";

	# ****************************
	# information on MySQL
	$MYSQL     = '@bindir@/mysql';    # path to mysql executable
	$SERVER    = '3.21';
	$MYSQL_OPT = ' --batch --unbuffered';
	$ACCESS_DB = 'mysql';		 # name of DB with grant-tables
	$ACCESS_H  = 'host';		 # 
	$ACCESS_U  = 'user';	         # 
	$ACCESS_D  = 'db';               #
	# Add/Edit privileges
	$ACCESS_H_TMP = 'host_tmp';      
	$ACCESS_U_TMP = 'user_tmp';      
	$ACCESS_D_TMP = 'db_tmp';        
	$ACCESS_H_BCK = 'host_backup';   
	$ACCESS_U_BCK = 'user_backup';   
	$ACCESS_D_BCK = 'db_backup';     
        $DIFF      = '/usr/bin/diff'; 
        $TMP_PATH  = '/tmp';             #path to writable tmp-directory
        $MYSQLDUMP = '@bindir@/mysqldump';
                                         #path to mysqldump executable

        $MYSQLADMIN= 'http://foobar.com/MySQLadmin';
                                         #URL of CGI for manipulating
                                         #the temporary grant-tables
}

END {
	unlink $MYSQL_CNF if defined $MYSQL_CNF and not $DEBUG;
}

$INFO = <<"_INFO";
--------------------------------------------------------------------------
   mysqlaccess (Version $VERSION)
   ~~~~~~~~~~~
   Copyright (C) 1997,1998 Yves.Carlier\@rug.ac.be
                           University of Ghent (RUG), Belgium
                           Administratieve Informatieverwerking (AIV)

   report the access-privileges for a USER from a HOST to a DB

   Many thanks go to <monty\@mysql.com> and <psmith\@BayNetworks.COM>
   for their suggestions, debugging and patches. 

   use `$script -?' to get more information on available options.

   From version 2.0x, $script can also be used through a WEB-browser
   if it is ran as a CGI-script.  (See the release-notes)

--------------------------------------------------------------------------
_INFO

$OPTIONS = <<_OPTIONS;

Usage: $script [host [user [db]]] OPTIONS

  -?, --help           display this helpscreen and exit
  -v, --version        print information on the program `$script'

  -u, --user=#         username for logging in to the db
  -p, --password=#     validate password for user
  -h, --host=#         name or IP-number of the host
  -d, --db=#           name of the database

  -U, --superuser=#    connect as superuser
  -P, --spassword=#    password for superuser
  -H, --rhost=#        remote MySQL-server to connect to
      --old_server     connect to old MySQL-server (before v3.21) which 
                       does not yet know how to handle full where clauses.

  -b, --brief          single-line tabular report
  -t, --table          report in table-format

  --relnotes           print release-notes
  --plan               print suggestions/ideas for future releases
  --howto              some examples of how to run `$script'
  --debug=N            enter debuglevel N (0..3)

  --copy               reload temporary grant-tables from original ones
  --preview            show differences in privileges after making
                       changes in (temporary) grant-tables
  --commit             copy grant-rules from temporary tables to grant-tables
                       (!don't forget to do an mysqladmin reload)
  --rollback           undo the last changes to the grant-tables.

  Note:
    + At least the user and the db must be given (even with wildcards)
    + If no host is given, `localhost' is assumed
    + Wilcards (*,?,%,_) are allowed for host, user and db, but be sure 
      to escape them from your shell!! (ie type \\* or '*')
_OPTIONS

$RELEASE = <<'_RELEASE';
 
Release Notes:
-------------
  0.1-beta1: internal
  - first trial.
 
  0.1-beta2: (1997-02-27)
  - complete rewrite of the granting-rules, based on the documentation
    found in de FAQ.
  - IP-number and name for a host are equiv.
 
  0.1-beta3: (1997-03-10)
  - more information
  - 'localhost' and the name/ip of the local machine are now equiv.

  0.1-beta4: (1997-03-11)
  - inform the user if he has not enough priv. to read the mysql db

  1.0-beta1: (1997-03-12)
  suggestions by Monty:
  - connect as superuser with superpassword.
  - mysqlaccess could also notice if all tables are empty. This means
    that all user have full access!
  - It would be nice if one could optionally start mysqlaccess without
    any options just the arguments 'user db' or 'host user db', where
    host is 'localhost' if one uses only two arguments.

  1.0-beta2: (1997-03-14)
  - bugfix: translation to reg.expr of \_ and \%.
  - bugfix: error in matching regular expression and string given
            by user which resulted in
            'test_123' being matched with 'test'

  1.0-beta3: (1997-03-14)
  - bugfix: the user-field should not be treated as a sql-regexpr,
            but as a plain string.
  - bugfix: the host-table should not be used if the host isn't empty in db
                                          or  if the host isn't emty in user
            (Monty)
 
  1.0-beta4: (1997-03-14)
  - bugfix: in an expression "$i = $j or $k", the '=' binds tighter than the or
            which results in problems...
            (by Monty)
  - running mysqlaccess with "perl -w" gives less warnings...   ;-)

  1.0-beta5: (1997-04-04)
  - bugfix: The table sorting was only being applied to the "user" table; all
            the tables need to be sorted.  Rewrote the sort algorithm, and
            the table walk algorithm (no temp file anymore), and various
            other cleanups.  I believe the access calculation is 100% correct.
            (by Paul D. Smith <psmith\@baynetworks.com>)
  - Allow the debug level to be set on the cmd line with --debug=N.
            (by Paul D. Smith <psmith\@baynetworks.com>)
  - More -w cleanups; should be totally -w-clean.
            (by Paul D. Smith <psmith\@baynetworks.com>)
 
  1.1-beta1: (1997-04-xx) 
  1.1-beta2: (1997-04-11)
  - new options:
             --all_users : report access-rights for all possible users
             --all_dbs   : report access-rights for all possible dbs
             --all_hosts : report access-rights for all possible hosts
             --brief     : as brief as possible, don't mention notes,warnings and rules
             --password  : validate password for user 
  - layout: long messages are wrapped on the report.
  - functionality:
            more descriptive notes and warnings
            wildcards (*,?) are allowed in the user,host and db options
            setting xxxx=* is equiv to using option --all_xxxx
            note: make sure you escape your wildcards, so they don't get
                  interpreted by the shell.  use \* or '*'
  - bugfix: Fieldnames which should be skipped on the output can now have
            a first capital letter.
  - bugfix: any option with a '.' (eg ip-number) was interpreted as
            a wildcard-expression.
  - bugfix: When no entry was found in the db-table, the default accessrights are
            N, instead of the faulty Y in a previous version.
 
  1.1-beta-3  : (1997-04-xx)
  1.1-beta-4  : (1997-04-xx)
  1.1-beta-5  : (1997-04-xx)
  1.1         : (1997-04-28)
  - new options:
            --rhost     : name of mysql-server to connect to
            --plan      : print suggestions/ideas for future releases
            --relnotes  : display release-notes
            --howto     : display examples on how to use mysqlaccess
            --brief     : single-line tabular output
  - functionality/bugfix:
    *      removed options --all_users,--all_dbs,--all_hosts, which 
           were redundant with the wildcard-expressions for the corresponding
           options. They made the processing of the commandline too painful 
           and confusing ;-)
           (suggested by psmith)
    *      redefined the option --brief, which now gives a single-line 
           tabular output
    *      Now we check if the right version of the mysql-client is used,
           since we might use an option not yet implemented in an
           older version (--unbuffered, since 3.0.18)
           Also the error-messages the mysql-client reports are 
           better interpreted ;-)  
    *      Wildcards can now be given following the SQL-expression 
           (%,_) and the Regular-expression (*,?) syntax.
  - speed: we now open a bidirectional pipe to the mysql-client, and keep 
           it open throughout the whole run. Queries are written to,
           and the answers read from the pipe.
           (suggested by monty)
  - bugfixes:
    *      the Rules were not properly reset over iterations 
    *      when in different tables the field-names were not identical, 
           eg. Select_priv and select_priv, they were considered as 
           definitions of 2 different access-rights.
    *      the IP-number of a host with a name containing wildcards should
           not be searched for in Name2IP and IP2Name.
    *      various other small things, pointed out by <monty> and <psmith>

  1.2         : (1997-05-13)
  - bugfix:
    * Fixed bug in acl with anonymous user:  Now if one gets accepted by the
      user table as a empty user name, the user name is set to '' when 
      checking against the 'db' and 'host' tables. (Bug fixed in MySQL3.20.19)

  1.2-1       : (1997-xx-xx)
  - bugfix:
    * hashes should  be initialized with () instead of {} <psmith>
    * "my" variable $name masks earlier declaration in same scope,
      using perl 5.004 <????>

  1.2-2       : (1997-06-10)
    
  2.0p1-3     : (1997-10-xx)
  - new
    * packages
    * log-file for debug-output : /tmp/mysqlaccess.log
    * default values are read from a configuration file $script.conf
      first this file is looked for in the current directory; if not
      found it is looked for in /etc/
      Note that when default-values are given, these can't get overriden
      by empty (blanc) values!
    * CGI-BIN version with HTML and forms interface.  Simply place the
      script in an ScriptAliased directory, make the configuration file
      available in the that directory or in /etc, and point your browser
      to the right URL. 
    * copy the grant-rules to temporary tables, where you are safe to
      play with them.
    * preview changes in privileges after changing grant-rules,
      before taking them into production
    * copy the new grant-rules from the temporary tables back to the
      grant-tables.
    * Undo all changes made in the grant-tables (1-level undo).
  -new options:
    * --table   : as opposite of the --brief option.
    * --copy    : (re)load temporary grant-tables from original ones.
    * --preview : preview changes in privileges after changing
                  some or more entries in the grant-tables.
    * --commit  : copy grant-rules from temporary tables to grant-tables
                  (!don't forget to do an mysqladmin reload)
    * --rollback: undo the last changes to the grant-tables.

  - bugfix:
    * if the table db is empty, mysqlaccess freezed 
      (by X Zhu <X.Zhu@Bradford.ac.uk>)

  2.0         : (1997-10-09)
  - fixed some "-w" warnings.
  - complain when certain programs and paths can't be found.

  2.01        : (1997-12-12)
  - bugfix:
    * rules for db-table where not calculated and reported correctly.
  2.02        : (1998-01-xx)
  - bugfix:
    * Privileges of the user-table were not AND-ed properly with the
      other privileges. (reported by monty)
  - new option:
    * --old_server: mysqlaccess will now use a full where clause when
                    retrieving information from the MySQL-server.  If
                    you are connecting to an old server (before v3.21)
                    use the option --old_server.
  2.03         : (1998-02-27)
  - bugfix:
    * in Host::MatchTemplate: incorrect match if host-field was left empty.

  2.04-alpha1  : (2000-02-11)
  Closes vulnerability due to former implementation requiring passwords
  to be passed on the command line.
  - functionality
    Option values for --password -p -spassword -P  may now be omitted from
    command line, in which case the values will be prompted for.
      (fix supplied by Steve Harvey <sgh@vex.net>)

   2.05: (2000-02-17)   Monty
   Moved the log file from /tmp to ~

   2.06:  Don't print '+++USING FULL WHERE CLAUSE+++'

_RELEASE

$TODO = <<_TODO;

 Plans:
 -----
  -a full where clause is use now.  How can we handle older servers?
  -add some more functionality for DNS.
  -select the warnings more carefuly.
  >>  I think that the warnings should either be enhanced to _really_
  >>  understand and report real problems accurately, or restricted to
  >>  only printing things that it knows with 100% certainty. <psmith)
  >>  Why do I have both '%' and 'any_other_host' in there?  Isn't that
  >>  the same thing?  I think it's because I have an actual host '%' in
  >>  one of my tables.  Probably the script should catch that and not
  >>  duplicate output. <psmith>

_TODO

# From the FAQ: the Grant-algorithm
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# The host table is mainly to maintain a list of "secure" servers.
# At TCX hosts contain a list of all machines on local network. These are granted
# all privileges.
# Technically the user grant is calculated by:
#
#    1.First sort all entries by host by putting host without wildcards first,
#      after this host with wildcards and entries with host = ".
#      Under each host sort user by the same criterias.
#    2.Get grant for user from the "db" table.
#    3.If hostname is "empty" for the found entry, AND the privileges with
#      the privileges for the host in "host" table.
#      (Remove all which is not "Y" in both)
#    4.OR (add) the privileges for the user from the "user" table.
#     (add all privileges which is "Y" in "user")
#
#    When matching, use the first found match.
#
# -----------------------------------------------------------------------------------

$HOWTO = <<_HOWTO;

Examples of how to call $script:
~~~~~~~~
1)Calling $script with 2 arguments:

  \$ $script root mysql
     ->report rights of user root logged on at the local host in db mysql

  Access-rights
  for USER 'root', from HOST 'localhost', to DB 'mysql'
          +-----------------+---+ +-----------------+---+
          | select_priv     | Y | | drop_priv       | Y |
          | insert_priv     | Y | | reload_priv     | Y |
          | update_priv     | Y | | shutdown_priv   | Y |
          | delete_priv     | Y | | process_priv    | Y |
          | create_priv     | Y | | file_priv       | Y |
          +-----------------+---+ +-----------------+---+
  BEWARE:  Everybody can access your DB as user 'root'
        :  WITHOUT supplying a password.  Be very careful about it!!

  The following rules are used:
   db    : 'No matching rule'
   host  : 'Not processed: host-field is not empty in db-table.'
   user  : 'localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y'

2)Calling $script with 3 arguments:

  \$ $script foo.bar nobody Foo 
     ->report rights of user root logged in at machine foobar to db Foo

  Access-rights
  for USER 'nobody', from HOST 'foo.bar', to DB 'Foo'
          +-----------------+---+ +-----------------+---+
          | select_priv     | Y | | drop_priv       | N |
          | insert_priv     | Y | | reload_priv     | N |
          | update_priv     | Y | | shutdown_priv   | N |
          | delete_priv     | Y | | process_priv    | N |
          | create_priv     | N | | file_priv       | N |
          +-----------------+---+ +-----------------+---+
  BEWARE:  Everybody can access your DB as user 'nobody'
        :  WITHOUT supplying a password.  Be very careful about it!!

  The following rules are used:
   db    : 'foo.bar','Foo','nobody','Y','Y','Y','N','N','N'
   host  : 'Not processed: host-field is not empty in db-table.'
   user  : 'foo.bar','nobody','','N','N','N','Y','N','N','N','N','N','N'

3)Using wildcards:

  \$ $script  \\* nobody Foo --brief
     ->report access-rights of user nobody from all machines to db Foo,
       and use a matrix-report.

  Sel  Ins  Upd  Del  Crea Drop Reld Shut Proc File Host,User,DB        
  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- --------------------
   Y    Y    Y    Y    N    N    N    N    N    N   localhost,nobody,Foo
   N    N    N    N    N    N    N    N    N    N   %,nobody,Foo  
   N    N    N    N    N    N    N    N    N    N   any_other_host,nobody,Foo

_HOWTO


# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ #
#                       START OF THE PROGRAM                            #
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ #

use Getopt::Long;
use Sys::Hostname;
use IPC::Open3;
#use CGI;			#moved to use of CGI by monty


# ****************************
# debugging flag
# can be set to 0,1,2,3
# a higher value gives more info
# ! this can also be set on the command-line
	$DEBUG   = 0;

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>8
#  Normaly nothing should be changed beneeth this line


# ****************************
# no caching on STDOUT
	$|=1;

	$MYSQL_CNF = POSIX::tmpnam();
	%MYSQL_CNF = (client    => { },
                      mysql     => { },
                      mysqldump => { },
	);



$NEW_USER = 'ANY_NEW_USER';
$NEW_DB   = 'ANY_NEW_DB'  ;


# %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% #
#  mysqlaccess:                                              #
#  ~~~~~~~~~~~                                               #
#  Lets get to it,                                           #
#  and start the program by processing the parameters        #
# %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% #

($CMD,$CGI) = GetMode();

# ****************************
# the copyright message should
# always be printed (once)
MySQLaccess::Report::Print_Header();

# *****************************
# Read configuration-file
  MySQLaccess::Debug::Print(1, "Reading configuration file...");
  if (-f "./$script_conf") {
     require "./$script_conf";
  }
  elsif (-f "/etc/$script_conf") {
     require "/etc/$script_conf";
  }

# ****************************
# Read in all parameters
if ($MySQLaccess::CMD) { #command-line version
	# ----------------------------
	# Get options from commandline
	$Getopt::Long::ignorecase=0; #case sensitive options
	if ( grep(/\-\?/,@ARGV) ) { MySQLaccess::Report::Print_Usage(); exit 0; }
	GetOptions("help"          => \$Param{'help'}
	          ,"host|h=s"      => \$Param{'host'}
	          ,"user|u=s"      => \$Param{'user'}
	          ,"password|p:s"  => \$Param{'password'}
	          ,"db|d=s"        => \$Param{'db'}
	          ,"superuser|U=s" => \$Param{'superuser'}
	          ,"spassword|P:s" => \$Param{'spassword'}
	          ,"rhost|H=s"     => \$Param{'rhost'}
                  ,"old_server"    => \$Param{'old_server'}
	          ,"debug=i"       => \$Param{'DEBUG'}
	          ,"brief|b"       => \$Param{'brief'}
	          ,"table|t"       => \$Param{'table'}
	          ,"relnotes"      => \$Param{'relnotes'}
	          ,"plan"          => \$Param{'plan'}
	          ,"howto"         => \$Param{'howto'}
	          ,"version|v"     => \$Param{'version'}
                  ,"preview"       => \$Param{'preview'}
                  ,"copy"          => \$Param{'copy'}
                  ,"commit"        => \$Param{'commit'}
                  ,'rollback'      => \$Param{'rollback'}
		  );

        # -----------------------------
        # set DEBUG
        $DEBUG = $Param{'DEBUG'} if ($Param{'DEBUG'}>=$DEBUG);

	# -----------------------------
	# check for things which aren't
	# declared as options:
	# 2 arguments: (user,db) -> ('localhost','user','db')
	if ($#ARGV == 1) {
	   MySQLaccess::Debug::Print(2,"$script called with 2 arguments:");
	   $Param{'host'} = $Param{'host'} || 'localhost'; 
	   $Param{'user'} = $ARGV[0] || $Param{'user'};
           $Param{'db'}   = $ARGV[1] || $Param{'db'}; 
	}
	# 3 arguments: (host,user,db)
	if ($#ARGV == 2) {
	   MySQLaccess::Debug::Print(2,"$script called with 3 arguments:");
	   $Param{'host'} = $ARGV[0] || $Param{'host'};
	   $Param{'user'} = $ARGV[1] || $Param{'user'};
	   $Param{'db'}   = $ARGV[2] || $Param{'db'};
	}

	# -------------------------------------
	# prompt for user password if requested
	if ( defined($Param{'password'}) && length($Param{'password'}) == 0 ) {
           $Param{'password'} = PromptPass(
	                        "Password for MySQL user $Param{'user'}: ");
	}
}
if ($MySQLaccess::CGI) { #CGI-version
	use CGI;
 	$Q = new CGI;
	$Param{'help'} = $Q->param('help') ;
	$Param{'host'} = $Q->param('host') || $Q->param('h') || $Param{'host'};
	$Param{'user'} = $Q->param('user') || $Q->param('u') || $Param{'user'};
	$Param{'db'}   = $Q->param('db')   || $Q->param('d') || $Param{'db'};
	$Param{'password'}  = $Q->param('password')  || $Q->param('p') || $Param{'password'};
	$Param{'superuser'} = $Q->param('superuser') || $Q->param('U') || $Param{'superuser'};
	$Param{'spassword'} = $Q->param('spassword') || $Q->param('P') || $Param{'spassword'};
	$Param{'rhost'}     = $Q->param('rhost')     || $Q->param('H') || $Param{'rhost'};
	$Param{'old_server'}= $Q->param('old_server')|| $Param{'old_server'};
	$Param{'debug'}     = $Q->param('debug')     || $Param{'debug'};
	$Param{'brief'}     = $Q->param('brief')     || $Param{'brief'}; 
	$Param{'table'}     = $Q->param('table')     || $Param{'table'}; 
	$Param{'relnotes'}  = $Q->param('relnotes');
	$Param{'plan'}      = $Q->param('plan');
	$Param{'howto'}     = $Q->param('howto'); 
	$Param{'version'}   = $Q->param('version') ? $Q->param('version') : $Q->param('v');
	$Param{'edit'}      = $Q->param('edit'); 
	$Param{'preview'}   = $Q->param('preview'); 
	$Param{'copy'}      = $Q->param('copy'); 
	$Param{'commit'}    = $Q->param('commit'); 
	$Param{'rollback'}  = $Q->param('rollback'); 
        # -----------------------------
        # set DEBUG
        $DEBUG = $Q->param('debug') if ($Q->param('debug')>=$DEBUG);
}

# ----------------------
# brief and table-format 
# exclude each-other
# table-format is prefered
if (defined($Param{'table'})) { undef($Param{'brief'}); }
if (defined($Param{'preview'}) or
    defined($Param{'copy'}) or
    defined($Param{'commit'}) or
    defined($Param{'rollback'}) ) { $Param{'edit'}='on'; }


# ----------------------
# if no host is given
# assume we mean 'localhost'
if (!defined($Param{'host'}))      { $Param{'host'}='localhost'; }

# ----------------------
# perform some checks
# -> eliminate 'broken pipe' error
push(@MySQLaccess::Grant::Error,'not_found_mysql')     if !(-x $MYSQL);
push(@MySQLaccess::Grant::Error,'not_found_diff')      if !(-x $DIFF);
push(@MySQLaccess::Grant::Error,'not_found_mysqldump') if !(-x $MYSQLDUMP);
push(@MySQLaccess::Grant::Error,'not_found_tmp')       if !(-d $TMP_PATH);
push(@MySQLaccess::Grant::Error,'write_err_tmp')       if !(-w $TMP_PATH);
if (@MySQLaccess::Grant::Error) {
   MySQLaccess::Report::Print_Error_Messages() ;
   exit 0;
}

#-----------------------
# get info/help if necc.
$print_usage=1;
if ( defined($Param{'version'}) ) {
   MySQLaccess::Report::Print_Version();
   $print_usage=0;
   MySQLaccess::Report::Print_Footer();
   MySQLaccess::DB::CloseConnection();
   exit 0;
#   exit 0;
}
if ( defined($Param{'relnotes'}) ) {
   MySQLaccess::Report::Print_Relnotes();
   $print_usage=0;
   MySQLaccess::Report::Print_Footer();
   MySQLaccess::DB::CloseConnection();
   exit 0;
#   exit 0;
}
if ( defined($Param{'plan'}) ) {
   MySQLaccess::Report::Print_Plans();
   $print_usage=0;
   MySQLaccess::Report::Print_Footer();
   MySQLaccess::DB::CloseConnection();
   exit 0;
#   exit 0;
}
if ( defined($Param{'howto'}) ) {
   MySQLaccess::Report::Print_HowTo();
   $print_usage=0;
   MySQLaccess::Report::Print_Footer();
   MySQLaccess::DB::CloseConnection();
   exit 0;
#   exit 0;
}

# -----------------------------
# generate a help-screen in CMD-mode
# or a blanc form in CGI-mode 
if ( defined($Param{'help'}) 
     or !defined($Param{'user'}) 
     or !defined($Param{'host'})
     or !defined($Param{'db'}) 
   ) {
   push(@MySQLaccess::Grant::Error,'user_required') unless defined($Param{'user'});
   push(@MySQLaccess::Grant::Error,'db_required') unless defined($Param{'db'});
   push(@MySQLaccess::Grant::Error,'host_required') unless defined($Param{'host'});
   MySQLaccess::Report::Print_Usage() if $print_usage;
   exit 0;
}


# ----------------------------
# get hostname and local-ip
# for localhost
$localhost = MySQLaccess::Host::LocalHost();
$local_ip  = MySQLaccess::Host::Name2IP($localhost);
$MySQLaccess::Host::localhost = MySQLaccess::Host::LocalHost();
$MySQLaccess::Host::local_ip  = MySQLaccess::Host::Name2IP($localhost);
MySQLaccess::Debug::Print(3, "localhost name=$localhost, ip=$local_ip");

#-----------------------------------
# version of MySQL-server to connect
# to determine use of full where clause
$MySQLaccess::Host::SERVER = $Param{'old_server'} ? '3.20' : $SERVER;

#---------------------------------
# create the config file for mysql and mysqldump
# to avoid passing authentication info on the command line
#
MergeConfigFiles();
die "Unsafe config file found: $unsafeConfig\n"  if $unsafeConfig;
if (defined($Param{'superuser'})) {
   $MYSQL_CNF{'mysql'}{'user'} = $Param{'superuser'};
   $MYSQL_CNF{'mysqldump'}{'user'} = $Param{'superuser'};
}
if (defined($Param{'spassword'})) {
   if ( $CMD && length($Param{'spassword'}) == 0 ) {
      $Param{'spassword'} =
           PromptPass("Password for MySQL superuser $Param{'superuser'}: ");
   }
   if ( length($Param{'spassword'}) > 0 ) {
      $MYSQL_CNF{'mysql'}{'password'} = $Param{'spassword'};
      $MYSQL_CNF{'mysqldump'}{'password'} = $Param{'spassword'};
   }
}
WriteTempConfigFile();

#---------------------------------
# Inform user if he has not enough
# privileges to read the access-db
if ( $nerror=MySQLaccess::DB::OpenConnection() ) {
    MySQLaccess::Report::Print_Error_Access($nerror);
    exit 0;
}

# -----------------------
# Read MySQL ACL-files
if ($nerror=MySQLaccess::Grant::ReadTables()) {
    MySQLaccess::Report::Print_Error_Access($nerror);
    exit 0;
};
if ($Param{'edit'} and $nerror=MySQLaccess::Grant::ReadTables('tmp')) {
    MySQLaccess::Report::Print_Error_Access($nerror);
    exit 0;
}

#---------------------------------
# reload temporay grant-tables 
# with data from original ones
if ( defined($Param{'copy'}) ) {
   $nerror=MySQLaccess::DB::LoadTmpTables();
   if ($nerror) {
      MySQLaccess::Report::Print_Error_Access($nerror);
      exit 0;
   }
   my $msg = "The grant-rules are copied from the grant-tables to\n"
           . "the temporary tables.";
   MySQLaccess::Report::Print_Message([$msg]);
#   MySQLaccess::Report::Print_Footer();
#   MySQLaccess::DB::CloseConnection();
#   exit 0;
}


#---------------------------------
# preview result of changes in the 
# grant-tables
if ( defined($Param{'preview'}) ) {
   $aref=MySQLaccess::Grant::Diff_Privileges();
   MySQLaccess::Report::Print_Diff_ACL($aref);
#   MySQLaccess::Report::Print_Footer();
#   MySQLaccess::DB::CloseConnection();
#   exit 0;
}


#---------------------------------
# reload grant-tables 
# with data from temporary tables
if ( defined($Param{'commit'}) ) {
   if ($nerror = MySQLaccess::DB::CommitGrantTables()) {
      MySQLaccess::Report::Print_Error_Access($nerror);
      exit 0;
   }
   my $msg = "The grant-rules have been copied from the temporary tables\n"
           . "to the grant-tables.";
   my $msg1= "Don't forget to do an 'mysqladmin reload' before these\n"
           . "changes take effect.";
   my $msg2= "A backup-version of your original grant-rules are saved in the\n"
           . "backup-tables, so you can always perform a 1-level rollback.";
   MySQLaccess::Report::Print_Message([$msg,$msg1,$msg2]);
#   MySQLaccess::Report::Print_Footer();
#   MySQLaccess::DB::CloseConnection();
#   exit 0;
}

#---------------------------------
# restore previous grant-rules
# with data from backup tables
if ( defined($Param{'rollback'}) ) {
   if ($nerror = MySQLaccess::DB::RollbackGrantTables()) {
      MySQLaccess::Report::Print_Error_Access($nerror);
      exit 0;
   }
   my $msg = "The old grant-rules have been copied back from the backup tables\n"
           . "to the grant-tables.";
   my $msg1= "Don't forget to do an 'mysqladmin reload' before these\n"
           . "changes take effect.";
   MySQLaccess::Report::Print_Message([$msg,$msg1]);
#   MySQLaccess::Report::Print_Footer();
#   MySQLaccess::DB::CloseConnection();
#   exit 0;
}
#----------------------------------
# show edit-taskbar
if ( defined($Param{'edit'})) {
   if ($MySQLaccess::CGI ) {
   MySQLaccess::Report::Print_Edit();
   $print_usage=0;
   MySQLaccess::Report::Print_Footer();
   MySQLaccess::DB::CloseConnection();
   exit 0;
   }
   else {
   MySQLaccess::Report::Print_Edit();
   $print_usage=0;
   MySQLaccess::Report::Print_Footer();
   MySQLaccess::DB::CloseConnection();
   exit 0;
   }
}


# -----------------------------
# Build list of users,dbs,hosts
# to process...
@all_dbs   = @{MySQLaccess::DB::Get_All_dbs($Param{'db'})};
@all_users = @{MySQLaccess::DB::Get_All_users($Param{'user'})};
@all_hosts = @{MySQLaccess::DB::Get_All_hosts($Param{'host'})};
#if EDIT-mode
#@all_dbs_tmp   = @{MySQLaccess::DB::Get_All_dbs($Param{'db'},'tmp')};
#@all_users_tmp = @{MySQLaccess::DB::Get_All_users($Param{'user'},'tmp')};
#@all_hosts_tmp = @{MySQLaccess::DB::Get_All_hosts($Param{'host'},'tmp')};

# -----------------------------
# Report access-rights for each
# tuple (host,user,db)
#$headers=0;
my %Access = ();
foreach $host (@all_hosts) {
  foreach $user (@all_users) {
    foreach $db (@all_dbs) {
      MySQLaccess::Grant::Initialize();
      %Access = MySQLaccess::Grant::Get_Access_Rights($host,$user,$db);	
      MySQLaccess::Report::Print_Access_rights($host,$user,$db,\%Access);
    }
  }
}

# -----------------------------
# End script
MySQLaccess::Report::Print_Footer();
MySQLaccess::DB::CloseConnection();
exit 0;

#############################################################
#  FUNCTIONS  #
###############
sub GetMode {
   my $cmd=0;
   my $cgi=0;
   if (defined($ENV{'HTTP_HOST'})) { $cmd=0; $cgi=1; }
   else                            { $cmd=1; $cgi=0; } 
   return ($cmd,$cgi);
}

# ================================
# sub PromptPass
#  prompt tty for a password
# ================================
sub PromptPass {
    my ($prompt) = @_;
    my $password;
    $ENV{PATH} = "/bin:/usr/bin";
    $ENV{IFS} = " \t\n";
    $ENV{SHELL} = "/bin/sh";
    system "stty -echo";
    print $prompt;
    chomp($password = <STDIN>);
    print "\n";
    system "stty echo";
    $password;
}

# =================================
# sub CheckUnsafeFile
#  tell if a config file containing a password is unsafe
# =================================
sub CheckUnsafeFile {
    my ($fname) = @_;
    my ($dev, $ino, $mode, $nlink,
        $uid, $gid, $rdev, $size,
        $atime, $mtime, $ctime, $blksize, $blocks) = stat($fname);

    if ( $uid != $< ) {   # unsafe if owned by other than current user
        return 1;
    }
    if ( $mode & 066 ) {  # unsafe if accessible by other
        return 1;
    }
    $fname =~ s#/[^/]+$##;
    if ( (length $fname) > 0 ) {
        return CheckUnsafeDir($fname);
    }
    return 0;
}

# =================================
# sub CheckUnsafeDir
#  tell if a directory is unsafe
# =================================
sub CheckUnsafeDir {
    my ($fname) = @_;
    my ($dev, $ino, $mode, $nlink,
        $uid, $gid, $rdev, $size,
        $atime, $mtime, $ctime, $blksize, $blocks) = stat($fname);

    # not owned by me or root
    if ( ($uid != $<) && ($uid != 0) ) {
        return 1;
    }
    if ( $mode & 022 ) {  # unsafe if writable by other
        return 1  unless $mode & 01000;  # but sticky bit ok
    }
    $fname =~ s#/[^/]+$##;
    if ( (length $fname) > 0 ) {
        return CheckUnsafeDir($fname);
    }
    return 0;
}

# =================================
# sub MergeConfigFile
#  merge data from .cnf file
# =================================
sub MergeConfigFile {
    my ($fname) = @_;
    my ($group, $item, $value);
    if ( open CNF, $fname ) {
         while (<CNF>) {
             s/^\s+//;
             next if /^[#;]/;
             if ( /\[\s*(\w+)\s*]/ ) {
                 $group = $1;
                 $group =~ tr/A-Z/a-z/;
                 if ( !exists $MYSQL_CNF{$group} ) {
                     undef $group;
                 }
             } elsif ( defined $group ) {
                 ($item, $value) = /((?:\w|-)+)\s*=\s*(\S+)/;
                 # don't unquote backslashes as we just write it back out
                 if ( defined $item ) {
                     if ( $item =~ /^password$/ ) {
                         if ( CheckUnsafeFile($fname) ) {
                             $unsafeConfig = $fname;
                         }
                     }
                     if ( $group eq 'client' ) {
                         $MYSQL_CNF{'mysql'}{$item} = $value;
                         $MYSQL_CNF{'mysqldump'}{$item} = $value;
                     } else {
                         $MYSQL_CNF{$group}{$item} = $value;
                     }
                 }
             }
         }
         close(CNF);
    }
}

# =================================
# sub MergeConfigFiles
#  merge options from config files
#  NOTE: really should do two separate merges for each
#    client to exactly duplicate order of resulting argument lists
# =================================
sub MergeConfigFiles {
    my ($name,$pass,$uid,$gid,$quota,$comment,$gcos,$dir,$shell) = getpwuid $<;
    MergeConfigFile("/etc/my.cnf");
    MergeConfigFile("$dir/.my.cnf");
}

# =================================
# sub WriteTempConfigFile
#  write 
# =================================
sub WriteTempConfigFile {
   sysopen CNFFILE, $MYSQL_CNF, O_RDWR|O_CREAT|O_EXCL, 0700
      or die "sysopen $MYSQL_CNF: $!";
   
   # groups may be in any order, generic groups such as [client] assumed
   # here to be empty
   foreach $group (keys %MYSQL_CNF) {
      print CNFFILE "[$group]\n";
      foreach $item (keys %{$MYSQL_CNF{$group}}) {
         if ( defined $MYSQL_CNF{$group}{$item} ) {
            print CNFFILE "$item=$MYSQL_CNF{$group}{$item}\n";
         } else {
            print CNFFILE "$item\n";
         }
      }
      print CNFFILE "\n";
   }
   close(CNFFILE);
}

######################################################################
package MySQLaccess::DB;
###########
BEGIN {
    $DEBUG     = 2;
    $DEBUG     = $MySQLaccess::DEBUG unless ($DEBUG);
    # Error-messages from the MySQL client
    %ACCESS_ERR= ('Access_denied'       => 'Access denied' 
                 ,'Dbaccess_denied'     => 'Access to database denied'
                 ,'Unrecognized_option' => 'unrecognized option' 
                 ,'Unknown_table'       => "Can't find file:"
                 ,'unknown_error'       => '^ERROR:'
                 );
}
# ######################################
#  Connecting to the MYSQL DB
# ======================================
# sub OpenConnection
#  Open an connection to the mysql-db
#  questions to MYSQL_Q
#  answers from MYSQL_A
# ======================================
sub OpenConnection {
    my $pid;
    MySQLaccess::Debug::Print(2,"OpenConnection:");

    # check path to mysql-client executable
    if (! -f $MySQLaccess::MYSQL) {
       if ($MySQLaccess::CMD) { die "Could not find MySQL-client '$MySQLaccess::MYSQL'"; }
       if ($MySQLaccess::CGI) { 
          print "<center>\n<font color=Red>\n";
          print "ERROR: Could not find MySQL-client '$MySQLaccess::MYSQL'";
          print "</center>\n</font>\n";
          exit 0;
       }
    }

    # path to mysql executable
    my $connect = "$MySQLaccess::MYSQL --defaults-file=$MySQLaccess::MYSQL_CNF";
    $connect .= " $MySQLaccess::MYSQL_OPT";
    # superuser, spassword transmitted via defaults-file
    if (defined($MySQLaccess::Param{'rhost'}))     { $connect .= " --host=$MySQLaccess::Param{'rhost'}"; }
    # other options??

    # grant-database
    $connect .= " $MySQLaccess::ACCESS_DB";

    # open connection (not using /bin/sh -c)
    MySQLaccess::Debug::Print(2,"Connecting to: $connect");
    $pid=IPC::Open3::open3(\*MYSQL_Q,\*MYSQL_A,"",split /\s+/,$connect);
    MySQLaccess::Debug::Print(2,"PID of open pipe: $pid");
    
    # check connection 
    print MYSQL_Q "select 'ok';\n";
    $answer = <MYSQL_A>; #answer from mysql
    MySQLaccess::Debug::Print(2,"Answer: $answer\n");
    foreach $nerror (sort(keys(%ACCESS_ERR))) {
      MySQLaccess::Debug::Print(3,"check answer for error $ACCESS_ERR{$nerror}");
      if (grep(/$ACCESS_ERR{$nerror}/i,$answer)) { 
         MySQLaccess::Debug::Print(2,"Answer contain error [$nerror]");
         return $nerror; 
      }
    }

if (0) {
    # check server-version 
    print MYSQL_Q "select 'ok';\n";
    $answer = <MYSQL_A>; #answer from mysql
    MySQLaccess::Debug::Print(2,"Answer: $answer\n");
    foreach $nerror (sort(keys(%ACCESS_ERR))) {
      MySQLaccess::Debug::Print(3,"check answer for error $ACCESS_ERR{$nerror}");
      if (grep(/$ACCESS_ERR{$nerror}/i,$answer)) { 
         MySQLaccess::Debug::Print(2,"Answer contain error [$nerror]");
         return $nerror; 
      }
    }
}

    my $skip=<MYSQL_A>; 
    return 0; 
}

# ======================================
# sub CloseConnection
#  Close the connection to the mysql-db
# ======================================
sub CloseConnection {
    close MYSQL_Q;
    close MYSQL_A;
}

# ===========================================================
# sub CreateTable($table)
#  Create temporary/backup table
# ===========================================================
sub CreateTable {
    my $pid;
    my ($table,$force) = @_;
    my %tables = ( $MySQLaccess::ACCESS_U_TMP => $MySQLaccess::ACCESS_U,
                   $MySQLaccess::ACCESS_H_TMP => $MySQLaccess::ACCESS_H,
                   $MySQLaccess::ACCESS_D_TMP => $MySQLaccess::ACCESS_D,
                   $MySQLaccess::ACCESS_U_BCK => $MySQLaccess::ACCESS_U,
                   $MySQLaccess::ACCESS_H_BCK => $MySQLaccess::ACCESS_H,
                   $MySQLaccess::ACCESS_D_BCK => $MySQLaccess::ACCESS_D,
                   $MySQLaccess::ACCESS_U => $MySQLaccess::ACCESS_U_BCK,
                   $MySQLaccess::ACCESS_H => $MySQLaccess::ACCESS_H_BCK,
                   $MySQLaccess::ACCESS_D => $MySQLaccess::ACCESS_D_BCK,
                 ); 
    my $tbl;
    my $query="";
    my $delim;
    my $skip;
    my $create;
    my @known_tables=();

#    print STDERR "CreateTable($table)\n";
    MySQLaccess::Debug::Print(1,"CreateTable($table):");

    ## error-handling
    return 'Unknown_table' unless defined($tables{$table});

    ## build list of known/existing tables;
    ## if 'force' existing table is dropped first
    if (defined($force) and $force) {
       @known_tables = Show_Tables();
       if (grep(/^$table$/,@known_tables)) {
       $query = "DROP TABLE $table;";
       }
    }

    ## path to mysqldump executable
    my $connect = $MySQLaccess::MYSQLDUMP;
    $connect .= " --defaults-file=$MySQLaccess::MYSQL_CNF --no-data";
    # superuser, spassword transmitted via defaults-file
    if (defined($MySQLaccess::Param{'rhost'}))     { $connect .= " --host=$MySQLaccess::Param{'rhost'}"; }
    $connect .= " $MySQLaccess::ACCESS_DB";
    $connect .= " $tables{$table}";


    ## get creation-data for original table
    $create = '';
    my $mysqldump = $connect;
    $mysqldump =~ s/ \$TABLE / $tbl /;

    # open connection (not using /bin/sh -c)
    MySQLaccess::Debug::Print(2,"Connecting to: $connect");
    $pid=IPC::Open3::open3(\*DONTCARE,\*CREATE,"",split /\s+/,$mysqldump);
    MySQLaccess::Debug::Print(2,"PID of open pipe: $pid");
    #open(CREATE,"$mysqldump");
    @create = <CREATE>;
    $create = "@create";
    foreach $nerror (sort(keys(%ACCESS_ERR))) {
       MySQLaccess::Debug::Print(3,"check answer for error $ACCESS_ERR{$nerror}");
       if (grep(/$ACCESS_ERR{$nerror}/i,$create)) { 
          MySQLaccess::Debug::Print(2,"Answer contain error [$nerror]");
          return $nerror; 
       }
    }
    close(CREATE);
    close(DONTCARE);

    ## manipulate result for creation-data for temporary table
    $create =~ s/CREATE TABLE $tables{$table} \(/CREATE TABLE $table \(/;

    ## recreate temporary table
    $query .= "$create\n";
    $query .= "select 'ok';";

    ## execute query
    print MYSQL_Q "$query\n";
#    print STDERR $query;

    $answer = <MYSQL_A>; #answer from mysql
#    print STDERR "A>",$answer;
    MySQLaccess::Debug::Print(2,"Answer: $answer\n");
    foreach $nerror (sort(keys(%ACCESS_ERR))) {
#       print STDERR "->$nerror?";
       MySQLaccess::Debug::Print(3,"check answer for error $ACCESS_ERR{$nerror}");
       if (grep(/$ACCESS_ERR{$nerror}/i,$answer)) { 
#          print STDERR "Yes!";
          MySQLaccess::Debug::Print(2,"Answer contain error [$nerror]");
          return $nerror; 
       }
    }

    $delim = <MYSQL_A>; # read header
    if ($delim ne "ok\n") {
       while (($line=<MYSQL_A>) ne "ok\n")
       { MySQLaccess::Debug::Print(3," A> $line"); }
        $skip = <MYSQL_A>; # skip result 'ok'
    }
#    print STDERR "CreateTable done\n";
    return 0;
}


# ===========================================================
# sub CopyTable()
#  Copy the structure and the data of a table to another table
# ===========================================================
sub CopyTable {
    my ($from,$to,$force) = @_;
    my @known_tables  = Show_Tables();    
    my $query = "";
    my $nerror= 0;
    my $skip;

#    print STDERR "CopyTable($from,$to)\n";
    MySQLaccess::Debug::Print(1,"MySQLaccess::DB::CopyTable($from,$to)");

    ## error-handling
    if (!grep(/^$from$/,@known_tables)) { return 'Unknown_table'; }

    ## copy structure 
    ## if forced
    if (defined($force) and $force) {
       return $nerror if ($nerror=CreateTable($to,$force)); 
#       print STDERR "Structure copied\n";
    }

    ## copy data
    $query .= "DELETE FROM $to;";
    $query .= "INSERT INTO $to SELECT * FROM $from;";
    $query .= "SELECT 'ok';\n";
    MySQLaccess::Debug::Print(2,"Query: $query");
       
    ## execute query
    print MYSQL_Q "$query\n";
#    print STDERR $query;

    ## check for errors...
    my $answer = <MYSQL_A>; #answer from mysql
#    print STDERR $answer;
    MySQLaccess::Debug::Print(2,"Answer: $answer\n");
    foreach $nerror (sort(keys(%ACCESS_ERR))) {
       MySQLaccess::Debug::Print(3,"check answer for error $ACCESS_ERR{$nerror}");
       if (grep(/$ACCESS_ERR{$nerror}/i,$answer)) { 
          MySQLaccess::Debug::Print(2,"Answer contain error [$nerror]");
          return $nerror; 
       }
    }

    my $delim = <MYSQL_A>; # read header
#    print STDERR $delim;
    if ($delim ne "ok\n") {
       while (($line=<MYSQL_A>) ne "ok\n")
       { MySQLaccess::Debug::Print(3," A> $line"); }
       $skip = <MYSQL_A>; # skip result 'ok'
    }

    return 0;
}

# ===========================================================
# sub LoadTmpTables()
#  (Re)load temporary tables with entries of ACL-tables
# ===========================================================
sub LoadTmpTables {
    my %tables = ( $MySQLaccess::ACCESS_U => $MySQLaccess::ACCESS_U_TMP,
                   $MySQLaccess::ACCESS_H => $MySQLaccess::ACCESS_H_TMP,
                   $MySQLaccess::ACCESS_D => $MySQLaccess::ACCESS_D_TMP,
                 ); 
    my $tbl;
    my $nerror;
    
#    print STDERR "LoadTmpTables:\n";
    MySQLaccess::Debug::Print(1,"LoadTmpTables():");
    foreach $tbl (keys(%tables)) {
#       print STDERR "$tbl -> $tables{$tbl}\n";
       MySQLaccess::Debug::Print(2,"Loading table $tbl -> $tables{$tbl}.");
       return $nerror if ($nerror=CopyTable($tbl,$tables{$tbl},'force'));
    }
    return 0;
}

# ===========================================================
# sub BackupGrantTables()
#  Make a backup of the original grant-tables
# ===========================================================
sub BackupGrantTables {
    my %tables = ( $MySQLaccess::ACCESS_U => $MySQLaccess::ACCESS_U_BCK,
                   $MySQLaccess::ACCESS_H => $MySQLaccess::ACCESS_H_BCK,
                   $MySQLaccess::ACCESS_D => $MySQLaccess::ACCESS_D_BCK,
                 ); 
    my $tbl;
    my $nerror;
    
#    print STDERR "BackupGrantTables:\n";
    MySQLaccess::Debug::Print(1,"BackupGrantTables():");
    foreach $tbl (keys(%tables)) {
#       print STDERR "$tbl -> $tables{$tbl}\n";
       MySQLaccess::Debug::Print(2,"Backup table $tbl -> $tables{$tbl}.");
       return $nerror if ($nerror=CopyTable($tbl,$tables{$tbl},'force'));
    }
    return 0;
}

# ===========================================================
# sub RollbackGrantTables()
#  Rollback the backup of the grant-tables
# ===========================================================
sub RollbackGrantTables {
    my %tables = ( $MySQLaccess::ACCESS_U_BCK => $MySQLaccess::ACCESS_U,
                   $MySQLaccess::ACCESS_H_BCK => $MySQLaccess::ACCESS_H,
                   $MySQLaccess::ACCESS_D_BCK => $MySQLaccess::ACCESS_D,
                 ); 
    my $tbl;
    my $nerror;
    
#    print STDERR "RollbackGrantTables:\n";
    MySQLaccess::Debug::Print(1,"RollbackGrantTables():");
    foreach $tbl (keys(%tables)) {
#       print STDERR "$tbl -> $tables{$tbl}\n";
       MySQLaccess::Debug::Print(2,"Rollback table $tbl -> $tables{$tbl}.");
       return $nerror if ($nerror=CopyTable($tbl,$tables{$tbl},'force'));
    }
    return 0;
}


# ===========================================================
# sub CommitGrantTables()
#  Copy grant-rules from temporary tables to the ACL-tables
# ===========================================================
sub CommitGrantTables {
    my %tables = ( $MySQLaccess::ACCESS_U => $MySQLaccess::ACCESS_U_TMP,
                   $MySQLaccess::ACCESS_H => $MySQLaccess::ACCESS_H_TMP,
                   $MySQLaccess::ACCESS_D => $MySQLaccess::ACCESS_D_TMP,
                 ); 
    my $tbl;
    my $query;
    my $delim;
    my $skip;
    my $create;

    print STDERR "CommitGrantTables()\n";
    MySQLaccess::Debug::Print(1,"CommitGrantTables():");
    
    ## Make backup of original grant-tables
    MySQLaccess::Debug::Print(2,"Making backup of original grant-tables...");
    BackupGrantTables();

    ## Copy data from temporay tables to grant-tables
    foreach $tbl (keys(%tables)) {
       print STDERR "$tbl -> $tables{$tbl}\n";
       MySQLaccess::Debug::Print(2,"Loading data $tables{$tbl} -> $tbl.");
       return $nerror if ($nerror=CopyTable($tables{$tbl},$tbl));
    }
    return 0;
}


# ===========================================================
# sub Show_Fields($table): 
#  return (a reference to) a hash which holds the names
#  of all relevant grant-fields, with their index in the record,
#  and (a reference to) an array which holds the fieldnames.
# ===========================================================
sub Show_Fields {
    my ($table) = @_;
    my %skip = ('host' => [0,1]
               ,'user' => [0,1,2]
               ,'db'   => [0,1,2]
               );
    my %Struct = ();
    my @Struct = ();
    my $query = "show fields from $table;select 'ok';\n";
    my $i=0;
    my $line;

#print STDERR $query;
    MySQLaccess::Debug::Print(1,"Show_Fields($table):");
    MySQLaccess::Debug::Print(2,"SQL: $query");

    print MYSQL_Q "$query";
    my $skip = <MYSQL_A>;  #skip header
    while (($line=<MYSQL_A>) ne "ok\n")
    {
#print STDERR ">",$line;
	chop($line);
	MySQLaccess::Debug::Print(2," $table>: $line");
	my ($field,$type,$null,$key,$default,$extra) = split(' ',$line);
        $field = ucfirst($field); 
	MySQLaccess::Debug::Print(3, " <split: $field - $type - $null - $key - $default - $extra");
	if (! grep(/$i/,@{$skip{$table}}) ){
	   $Struct{$field} = $i; #hash
	   push(@Struct,$field); #array
	   MySQLaccess::Debug::Print(3," ==> added column[$i]: $field ($Struct{$field})");
	} 
        else {
           MySQLaccess::Debug::Print(3," ==> skipped column[$i], value=[$field]");
        }
	$i++;
    }

    $skip=<MYSQL_A>;  # Get ok row (found already ok header)

    MySQLaccess::Debug::Print(2, "Array:");
    foreach $field (@Struct) { MySQLaccess::Debug::Print(2,"+ $field"); }
    MySQLaccess::Debug::Print(2,"Hash:");
    foreach $field (keys(%Struct)) { MySQLaccess::Debug::Print(2,"+ $field -> $Struct{$field}"); }

    return  (\%Struct,\@Struct); 
}

# ===========================================================
# sub Show_Tables(): 
#  return (a reference to) an array which holds all 
#  known tables.
# ===========================================================
sub Show_Tables {
    my @Tables = ();
    my $query = "show tables;select 'ok';\n";
    my $i=0;
    my $line;

    MySQLaccess::Debug::Print(1,"Show_Tables():");
    MySQLaccess::Debug::Print(2,"SQL: $query");

    print MYSQL_Q "$query";
    my $skip = <MYSQL_A>;  #skip header
    while (($line=<MYSQL_A>) ne "ok\n")
    {
	chop($line);
	push(@Tables,$line); #array
	MySQLaccess::Debug::Print(3," ==> added table: $line");
    }

    $skip=<MYSQL_A>;  # Get ok row (found already ok header)

    MySQLaccess::Debug::Print(2, "Array:");
    foreach $tbl (@Tables) { MySQLaccess::Debug::Print(2,"+ $tbl"); }

    return @Tables; 
}

# ======================================
# sub Validate_Password($passwd,$host,$user,$encpw)
#  Validate the given password 
#  for user '$user' 
#  connecting from host '$host'
# ======================================
sub Validate_Password {
    my ($password,$host,$user,$encpw) = @_;
    my $valid=0;

    MySQLaccess::Debug::Print(1,"Validate_Password($password,$host,$user,$encpw)");
    my $sql = "select host,user,password from user having "
             ."host='$host' and user='$user' and password='$encpw' "
             ."and password=PASSWORD('$password');\n";
    $sql .= "select 'ok';\n";
    MySQLaccess::Debug::Print(2,"SQL = $sql");
    print MYSQL_Q "$sql";
    
    # if password is valid, at least 1 row returns before we read 'ok'
    while ( ($line=<MYSQL_A>) ne "ok\n") {
      MySQLaccess::Debug::Print(2," A> $line");
      $valid = defined($line); 
    }
    my $skip = <MYSQL_A>; # read 'ok'

    return $valid;
}


# ==========================================================
# sub Sort_fields: (rewritten by psmith)
#  Build the query for an ordered list of entries
# ==========================================================
sub Sort_fields {
  my ($start, $end, $sofar, $this, @rest) = (@_);
  my @where = ("((FIELD not like '\\%') AND (FIELD <> ''))",
               "((FIELD like '%\\%%') OR (FIELD like '%\\_%'))",
               "(FIELD = '')");
  my $res = '';

  $this or return ("$start $sofar $end");

  $sofar .= ' AND ' if $sofar;

  foreach $w (@where) {
    my $f = $w;
    $f =~ s/FIELD/$this/g;

    $res .= Sort_fields($start, $end, "$sofar$f", @rest);
  }

  return ($res);
}

# ===========================================================
# sub Sort_table: (rewritten by psmith)
#  return all entries in the given table,
#  in an ordered fashion
# ===========================================================
sub Sort_table {
    my ($tbl, @order) = @_;
    my @res=();

    # as long as there's no full where clause (Distrib 3.20)...
    # use having :-(
    # NOTE: this clause WILL NOT work on 3.21, because of the
    # order of 'ORDER BY' and 'HAVING'
    my $start = "SELECT *,UCASE(host) as ucase_host FROM $tbl ";
    $start   .= 'ORDER BY ' . join(',', @order) ." HAVING ";
    my $end   = ";\n";

    # server version 3.21 has a full where clause :-)
    if ($MySQLaccess::Host::SERVER >= '3.21') {
    # print "+++USING FULL WHERE CLAUSE+++\n";
       $start = "SELECT *,UCASE(host) as ucase_host FROM $tbl WHERE ";
       $end = ' ORDER BY ' . join(',', @order) . ";\n";
    }

    MySQLaccess::Debug::Print(1,"Sort_table():");
    MySQLaccess::Debug::Print(2,"Sorting table $tbl by `@order'");

    my $tmp;
    foreach $tmp (@order)
    {
      $tmp="UCASE(host)" if ($tmp eq "ucase_host");
    }
    my $query  = Sort_fields($start, $end, '', @order);
    $query    .= "select 'ok';\n";
    MySQLaccess::Debug::Print(2,"Query: $query");

    print MYSQL_Q "$query\n";

    my $delim = <MYSQL_A>; # read header
    MySQLaccess::Debug::Print(3," A> $delim");
    if ($delim ne "ok\n") {
       if ($delim =~ /^ERROR/) {
       push(@MySQLaccess::Grant::Error,'use_old_server');
       MySQLaccess::Report::Print_Error_Messages() ;
       exit 1;
       }
       while (($line=<MYSQL_A>) ne "ok\n")
       {
           MySQLaccess::Debug::Print(3," A> $line");
           push(@res,$line);
       }
    }
    my $skip = <MYSQL_A>; # skip result 'ok'

    # remove columnheaders from output
    @res = grep(!/^\Q$delim\E$/, @res);
    # remove trailing \n from each returned record
    chomp(@res); 
    # each record has 1 field to much : ucase_host
    @res = grep { /(.*)\t.*$/; $_ = $1; } @res;

    MySQLaccess::Debug::Print(2,"Result of sorted table $tbl:");
    foreach $line (@res) { MySQLaccess::Debug::Print(2," >>$line"); }
    return @res;
}

# ===========================================================
# sub Get_All_db(template): 
#  return all db the grant-tables are working on,
#  which conform to the template
# ===========================================================
sub Get_All_dbs {
   my ($template,$tmp) = @_;
   my @db=();
   my $aref;

   # working with  temporary tables or production tables
   if (defined($tmp) and $tmp) {
      $aref = \@MySQLaccess::Grant::sorted_db_tmp_table ;
   }
   else {
      $aref = \@MySQLaccess::Grant::sorted_db_table;
   }

   MySQLaccess::Debug::Print(1," template=[$template]");

   # get all db for which access-rights can be calculated,
   # which conform to the template.
   # !! these db's don't have to exist yet, so it's not
   #    enough to look which db already exist on the system
   $reg_expr = $template;
   if ($template =~ /[\*\?]/) {
      $reg_expr =~ tr/*?/%_/;
      #$reg_expr = MySQLaccess::Wildcards::Wild2Reg($template);
   }
   $reg_expr = MySQLaccess::Wildcards::SQL2Reg("$reg_expr");

   if ( ! ($template =~ /[\*\?%_]/) ) {
      push(@db,$template);
      return \@db;
   }

   MySQLaccess::Debug::Print(2,"#Reading db-table...");
   foreach $record (@{$aref}) { #MySQLaccess::Grant::sorted_db_table) {
    my @record=split(/\t/,$record);
    my $db = $record[1];
    MySQLaccess::Debug::Print(2,"> $db ");
    if ( (!grep(/$db/i,@db)) and ($db =~/$reg_expr/i) ) {
       push(@db,$db);
       MySQLaccess::Debug::Print(2,"added");
    } 
    else {
       MySQLaccess::Debug::Print(2,"skipped");
    }
   }
   # if no rule is found for a certain db in the db-table,
   # the rights of the user are used, so we should inform
   # the user for
   if (!grep(/^%$/,@db)) { push(@db,"$MySQLaccess::NEW_DB"); }
   return \@db;
}

# ===========================================================
# sub Get_All_users(template): 
#  return all users the grant-tables are working on,
#  which conform to the template
# ===========================================================
sub Get_All_users {
   ($template,$tmp) = @_; # nog verder uitwerken!!!
   my @user=();
   my $aref;

   # working with  temporary tables or production tables
   if (defined($tmp) and $tmp) {
      $aref = \@MySQLaccess::Grant::sorted_user_tmp_table ;
   }
   else {
      $aref = \@MySQLaccess::Grant::sorted_user_table;
   }

   MySQLaccess::Debug::Print(1,"Debug Get_All_users:");
   # get all db for which access-rights can be calculated.
   # !! these db's don't have to exist yet, so it's not
   #    enough to look which db already exist on the system
   $reg_expr = $template;
   if ($template =~ /[\*\?]/) {
      $reg_expr =~ tr/*?/%_/;
      #$reg_expr = MySQLaccess::Wildcards::Wild2Reg($template);
   }
   $reg_expr = MySQLaccess::Wildcards::SQL2Reg("$reg_expr");

   if ( ! ($template =~ /[\*\?%_]/) ) {
      push(@user,$template);
      return \@user;
   }

   MySQLaccess::Debug::Print(2,"#Reading user-table...");
   foreach $record (@{$aref}) { #MySQLaccess::Grant::sorted_user_table) {
    my @record=split(/\t/,$record);
    my $user = $record[1];
    MySQLaccess::Debug::Print(2,"> $user ");
    if ( (!grep(/$user/,@user)) and ($user=~/$reg_expr/)) {
       push(@user,$user);
       MySQLaccess::Debug::Print(2, "added");
    } 
    else {
       MySQLaccess::Debug::Print(2, "skipped");
    }
   }
   # Any user means also:
   # - the 'empty' user, ie without supplying a username
   # - any user still to be defined/created
   #push(@user,'');               #without_suplying_a_username
   push(@user,"$MySQLaccess::NEW_USER");
   #push(@Warnings,'minimum_priv');
   return \@user;
}

# ===========================================================
# sub Get_All_hosts(template): 
#  return all hosts the grant-tables are working on,
#  which conform to the template
# ===========================================================
sub Get_All_hosts {
   my ($template,$tmp) = @_;
   my @host=();
   my $aref;
   my $aref1;

   # working with  temporary tables or production tables
   if (defined($tmp) and $tmp) {
      $aref = \@MySQLaccess::Grant::sorted_host_tmp_table ;
      $aref1= \@MySQLaccess::Grant::sorted_db_tmp_table ;
   }
   else {
      $aref = \@MySQLaccess::Grant::sorted_host_table;
      $aref1= \@MySQLaccess::Grant::sorted_db_table ;
   }

   MySQLaccess::Debug::Print(1, "Debug Get_All_hosts:");
   # get all db for which access-rights can be calculated.
   # !! these db's don't have to exist yet, so it's not
   #    enough to look which db already exist on the system
   $reg_expr = $template;
   if ($template =~ /[\*\?]/) {
      $reg_expr =~ tr/*?/%_/;
      #$reg_expr = MySQLaccess::Wildcards::Wild2Reg($template);
   }
   $reg_expr = MySQLaccess::Wildcards::SQL2Reg("$reg_expr");

   if ( ! ($template =~ /[\*\?%_]/) ) {
      push(@host,$template);
      return \@host;
   }

   MySQLaccess::Debug::Print(1, "#Reading db-table...");
   foreach $record (@{$aref1}) { #MySQLaccess::Grant::sorted_db_table) {
    my @record=split(/\t/,$record);
    my $host = $record[0];
    MySQLaccess::Debug::Print(2, "> $host ");
    if (! grep(/$host/i,@host)) {
       push(@host,$host);
       MySQLaccess::Debug::Print(2, "added");
    } 
    else {
       MySQLaccess::Debug::Print(2, "skipped");
    }
   }
   MySQLaccess::Debug::Print(1, "#Reading host-table...");
   foreach $record (@{$aref}) {
    my @record=split(/\t/,$record);
    my $host = $record[0];
    MySQLaccess::Debug::Print(2, "> $host ");
    if ( (!grep(/$host/,@host)) and ($host=~/$reg_expr/)) {
       push(@host,$host);
       MySQLaccess::Debug::Print(2, "added");
    } 
    else {
       MySQLaccess::Debug::Print(2, "skipped");
    }
   }
   # DOUBT:
   #print "#Reading user-table...\n" if ($DEBUG>1);
   #foreach $record (@MySQLaccess::Grant::sorted_user_table) {
   # my @record=split(/\t/,$record);
   # my $host = $record[0];
   # print "> $host " if ($DEBUG>2);
   # if ( (!grep(/$host/,@host)) and ($host=~/$reg_expr/)) {
   #    push(@host,$host);
   #    print "added\n" if ($DEBUG>2);
   # } 
   # else {
   #    print "skipped\n" if ($DEBUG>2);
   # }
   #}
   # Any host also means:
   # - any host still to be defined/created
   #push(@host,"any_other_host");

   @host = sort(@host);
   return \@host;
}


##########################################################################
package MySQLaccess::Grant;
##############
BEGIN {
    $DEBUG     = 0;
    $DEBUG     = $MySQLaccess::DEBUG unless ($DEBUG);
}



# ===========================================================
# sub Diff_Privileges()
#  Calculate diff between temporary and original grant-tables
# ===========================================================
sub Diff_Privileges {
   my @before=();
   my @after =();
   my @diffs =();

   # -----------------------------
   # Build list of users,dbs,hosts
   # to process...
   my @all_dbs   = @{MySQLaccess::DB::Get_All_dbs('*')};
   my @all_users = @{MySQLaccess::DB::Get_All_users('*')};
   my @all_hosts = @{MySQLaccess::DB::Get_All_hosts('*')};
   #if EDIT-mode
   my @all_dbs_tmp   = @{MySQLaccess::DB::Get_All_dbs('*','tmp')};
   my @all_users_tmp = @{MySQLaccess::DB::Get_All_users('*','tmp')};
   my @all_hosts_tmp = @{MySQLaccess::DB::Get_All_hosts('*','tmp')};


   my %Access;
   # ------------------------------------
   # Build list of priv. for grant-tables
   foreach $host (@all_hosts) {
     foreach $user (@all_users) {
       foreach $db (@all_dbs) {
         MySQLaccess::Grant::Initialize();
         %Access = MySQLaccess::Grant::Get_Access_Rights($host,$user,$db);	
         push(@before,MySQLaccess::Report::Raw_Report($host,$user,$db,\%Access));
       }
     }
   }

   # ----------------------------------
   # Build list of priv. for tmp-tables
   foreach $host (@all_hosts_tmp) {
     foreach $user (@all_users_tmp) {
       foreach $db (@all_dbs_tmp) {
         MySQLaccess::Grant::Initialize('tmp');
         %Access = MySQLaccess::Grant::Get_Access_Rights($host,$user,$db,'tmp');	
         push(@after,MySQLaccess::Report::Raw_Report($host,$user,$db,\%Access));
       }
     }
   }

   # ----------------------------------
   # Write results to temp-file to make
   # DIFF
   @before = sort(@before);
   @after  = sort(@after);

   $before = "$MySQLaccess::TMP_PATH/$MySQLaccess::script.before.$$";
   $after  = "$MySQLaccess::TMP_PATH/$MySQLaccess::script.after.$$";
   #$after = "/tmp/t0";
   open(BEFORE,"> $before") ||
    push(@MySQLaccess::Report::Errors,"Can't open temporary file $before for writing");
   open(AFTER,"> $after") ||
    push(@MySQLaccess::Report::Errors,"Can't open temporary file $after for writing");
   print BEFORE join("\n",@before);
   print AFTER  join("\n",@after);
   close(BEFORE);
   close(AFTER);

   # ----------------------------------
   # compute difference
   my $cmd="$MySQLaccess::DIFF $before $after |";
   open(DIFF,"$cmd");
   @diffs = <DIFF>;
   @diffs = grep(/[<>]/,@diffs);
   chomp(@diffs);
   close(DIFF);

   # ----------------------------------
   # cleanup temp. files
   unlink(BEFORE);
   unlink(AFTER);

   return \@diffs;
}

# ===========================================================
# sub Initialize()
#
# ===========================================================
sub Initialize {
    %MySQLaccess::Grant::Access       = %{Default_Access_Rights()};
    @MySQLaccess::Grant::Errors       = ();
    @MySQLaccess::Grant::Warnings     = ();
    @MySQLaccess::Grant::Notes        = ();
    # -----
    # rules
    $MySQLaccess::Grant::Rules{'user'} = 'no_rule_found';
    $MySQLaccess::Grant::Rules{'db'}   = 'no_rule_found';
    $MySQLaccess::Grant::Rules{'host'} = 'no_equiv_host';
    $MySQLaccess::Grant::full_access   = 1;

    $MySQLaccess::Grant::process_host_table = 0;
    return 1;
}

# ===========================================================
# sub ReadTables()
#  
# ===========================================================
sub ReadTables {
    my ($tmp) = @_;
    my ($HOST,$DB,$USER);
    my @tables;

    # build list of available tables
    @tables = MySQLaccess::DB::Show_Tables();

    # reading production grant-tables or temporary tables?
    $tmp = (defined($tmp) and $tmp) ? 1 : 0;
    if ($tmp) { #reading temporary tables
       $HOST=$MySQLaccess::ACCESS_H_TMP;
       $DB  =$MySQLaccess::ACCESS_D_TMP;
       $USER=$MySQLaccess::ACCESS_U_TMP;

       # ----------------------------
       # do tables exist?
       if (!grep(/$HOST/,@tables)) { MySQLaccess::DB::CreateTable($HOST); }
       if (!grep(/$USER/,@tables)) { MySQLaccess::DB::CreateTable($USER); }
       if (!grep(/$DB/,@tables))   { MySQLaccess::DB::CreateTable($DB); }

       MySQLaccess::Debug::Print(1,"Finding fields in tmp-ACL files:");
       # -----------------------------
       # Get record-layout 
       my ($h1,$h2) = MySQLaccess::DB::Show_Fields($HOST);
       my ($d1,$d2) = MySQLaccess::DB::Show_Fields($DB);
       my ($u1,$u2) = MySQLaccess::DB::Show_Fields($USER);
       %MySQLaccess::Grant::H_tmp = %{$h1}; @MySQLaccess::Grant::H_tmp = @{$h2};
       %MySQLaccess::Grant::D_tmp = %{$d1}; @MySQLaccess::Grant::D_tmp = @{$d2};
       %MySQLaccess::Grant::U_tmp = %{$u1}; @MySQLaccess::Grant::U_tmp = @{$u2};

#       @MySQLaccess::Grant::Privileges_tmp=@{Make_Privlist()};
#
       MySQLaccess::Debug::Print(1, "Reading sorted temp-tables:");
       @MySQLaccess::Grant::sorted_db_tmp_table  = MySQLaccess::DB::Sort_table($DB, 'ucase_host', 'user', 'db');
       @MySQLaccess::Grant::sorted_host_tmp_table= MySQLaccess::DB::Sort_table($HOST, 'ucase_host', 'db');
       @MySQLaccess::Grant::sorted_user_tmp_table= defined($MySQLaccess::Param{'password'}) ?
                           MySQLaccess::DB::Sort_table($USER, 'ucase_host', 'user', 'password'):
                           MySQLaccess::DB::Sort_table($USER, 'ucase_host', 'user');
    }
    else {      #reading production grant-tables
       $HOST=$MySQLaccess::ACCESS_H;
       $DB  =$MySQLaccess::ACCESS_D;
       $USER=$MySQLaccess::ACCESS_U;

       MySQLaccess::Debug::Print(1,"Finding fields in ACL files:");
       # -----------------------------
       # Get record-layout 
       my ($h1,$h2) = MySQLaccess::DB::Show_Fields($HOST);
       my ($d1,$d2) = MySQLaccess::DB::Show_Fields($DB);
       my ($u1,$u2) = MySQLaccess::DB::Show_Fields($USER);
       %MySQLaccess::Grant::H = %{$h1}; @MySQLaccess::Grant::H = @{$h2};
       %MySQLaccess::Grant::D = %{$d1}; @MySQLaccess::Grant::D = @{$d2};
       %MySQLaccess::Grant::U = %{$u1}; @MySQLaccess::Grant::U = @{$u2};

       @MySQLaccess::Grant::Privileges=@{Make_Privlist()};

       MySQLaccess::Debug::Print(1, "Reading sorted tables:");
       @MySQLaccess::Grant::sorted_db_table  = MySQLaccess::DB::Sort_table($DB, 'ucase_host', 'user', 'db');
       @MySQLaccess::Grant::sorted_host_table= MySQLaccess::DB::Sort_table($HOST, 'ucase_host', 'db');
       @MySQLaccess::Grant::sorted_user_table= defined($MySQLaccess::Param{'password'}) ?
                           MySQLaccess::DB::Sort_table($USER, 'ucase_host', 'user', 'password'):
                           MySQLaccess::DB::Sort_table($USER, 'ucase_host', 'user');
    }

    return 0;
}

# ===========================================================
# sub Get_Access_Rights(host,user,db)
#  report the access_rights for the tuple ($host,$user,$db).
# ===========================================================
sub Get_Access_Rights {
  local ($host,$user,$db,$tmp) = @_;

   my $aref_user;
   my $aref_host;
   my $aref_db;
   # working with  temporary tables or production tables
   if (defined($tmp) and $tmp) {
      $aref_user = \@MySQLaccess::Grant::sorted_user_tmp_table;
      $aref_host = \@MySQLaccess::Grant::sorted_host_tmp_table;
      $aref_db   = \@MySQLaccess::Grant::sorted_db_tmp_table;   
   }
   else {
      $aref_user = \@MySQLaccess::Grant::sorted_user_table;
      $aref_host = \@MySQLaccess::Grant::sorted_host_table;
      $aref_db   = \@MySQLaccess::Grant::sorted_db_table; 
   }


  my ($refrecord,$refgrant);
  my ($_host_,$_user_,$encpw_);
  my %_Access_;

  MySQLaccess::Debug::Print(1, "for ($host,$user,$db):");  

  # ******************************************************************************
  # Create default access-rights
  #   default access-rights are no access at all!!


  # ******************************************************************************
  # get hostname for IP-address
  # get IP-address for hostname
  local $host_name = MySQLaccess::Host::IP2Name($host);
  local $host_ip   = MySQLaccess::Host::Name2IP($host);

  MySQLaccess::Debug::Print(3,"host=$host, hostname=$host_name, host-ip =$host_ip");
  MySQLaccess::Debug::Print(3,"user=$user");
  MySQLaccess::Debug::Print(3,"db  =$db");

  # ***********************************************************************
  # retrieve information on USER
  #  check all records in mysql::user for matches with the tuple (host,user)
  # ***********************************************************************
  #    4.OR (add) the privileges for the user from the "user" table.
  #     (add all privileges which is "Y" in "user")
  ($refrecord,$refgrant)    = Get_grant_from_user($host,$user,$aref_user);
  ($_host_,$_user_,$encpw_) = @{$refrecord};
  %_access_                 = %{$refgrant};

  foreach $field (keys(%U)) { ##only priv. set in user-table
    $MySQLaccess::Grant::Access{$field} = ($MySQLaccess::Grant::Access{$field} or $_access_{$field});
  }

  if ($_user_ eq $MySQLaccess::NEW_USER) { 
     push(@Warnings,'minimum_priv');
  }
  if ($_user_ ne $user) {
     $user=$_user_;
     push(@Warnings,'anonymous_access');
  }

  # *******************************************************
  #  Validate password if this has been asked to do
  # *******************************************************
  if (defined($password)) {
     $valid = Validate_Password($password,$_host_,$_user_,$_encpw_,$aref_user);
     if (!$valid) { push(@Errors,'invalid_password'); }
     else         { push(@Notes,'valid_password'); }
  }

  # ******************************************************************************
  # retrieve information on DB
  #  check all records in mysql::db for matches with the triple (host,db,user)
  #  first match is used.
  # ******************************************************************************
  #    2.Get grant for user from the "db" table.

  ($refrecord,$refgrant)=Get_grant_from_db($host,$db,$user,$aref_db); #set process_host_table
  ($_host_,$_user_,$encpw_) = @{$refrecord};
  %_access_                 = %{$refgrant};

  foreach $field (keys(%D)) { ##only priv. set in db-table
    $MySQLaccess::Grant::Access{$field} = ($MySQLaccess::Grant::Access{$field} or $_access_{$field});
  }

  # ***********************************************************************
  # retrieve information on HOST
  #  check all records in mysql::host for matches with the tuple (host,db)
  #
  #  ' The host table is mainly to maintain a list of "secure" servers. '
  # ***********************************************************************
  #    3.If hostname is "empty" for the found entry, AND the privileges with
  #      the privileges for the host in "host" table.
  #      (Remove all which is not "Y" in both)

  if ($MySQLaccess::Grant::process_host_table) {
     ($refrecord,$refgrant)=Get_grant_from_host($host,$db,$aref_host);
     ($_host_,$_user_,$encpw_) = @{$refrecord};
     %_access_                 = %{$refgrant};

     foreach $field (keys(%H)) {  ##only priv. set in host-table 
       $MySQLaccess::Grant::Access{$field} = ($MySQLaccess::Grant::Access{$field} and $_access_{$field});
     } 
  }

  MySQLaccess::Debug::Print(1,"done for ($host,$user,$db)");
  return %MySQLaccess::Grant::Access;
}

# ####################################
# FINDING THE RIGHT GRANT-RULE
# ==========================================================
# sub Get_grant_from_user:
# ==========================================================
sub Get_grant_from_user {
  my ($host,$user,$aref) = @_;

  MySQLaccess::Debug::Print(1, "");
  MySQLaccess::Debug::Print(1, "(host=$host,user=$user)");

  my %Access_user = %{Default_Access_Rights()}; 

  my $rule_found=0;
  my @record = ();
  my $record;

  foreach $record (@{$aref}) {
    $MySQLaccess::Grant::full_access=0;
    MySQLaccess::Debug::Print(3, "Record= $record");
    @record=split(/\t/,$record);

    # check host and db
    # with possible wildcards in field
    # replace mysql-wildcards by reg-wildcards
    my $host_tpl = MySQLaccess::Wildcards::SQL2Reg($record[0]);
    my $user_tpl = $record[1]; #user field isn't pattern-matched!!
    my $passwd   = $record[2];

    MySQLaccess::Debug::Print(3, "=>host_tpl : read=$record[0] -> converted=$host_tpl");
    MySQLaccess::Debug::Print(3, "=>user_tpl : read=$record[1] -> $user_tpl");
    MySQLaccess::Debug::Print(3, "=>password : read=$record[2] -> $passwd");


    if ( MySQLaccess::Host::MatchTemplate($host,$host_tpl) and
         MySQLaccess::Wildcards::MatchTemplate($user_tpl,$user)
       ) 
    {
  MySQLaccess::Debug::Print(2, "FOUND!!");
        if ($passwd eq '') { push(@Warnings,'insecure_user');  }
        else               { push(@Notes,'password_required'); }

        foreach $field (keys(%U)) {
          $Access_user{$field} = $MySQLaccess::Report::Answer{$record[$U{$field}]};
        }
        #print "\n" if $DEBUG;
        $MySQLaccess::Grant::Rules{'user'} = $record;
        $rule_found=1;
        last;
    }
  }

  # -------------------------------
  #  setting privileges to user-priv
  MySQLaccess::Debug::Print(2, "Rights after parsing user-table..:");
  if (! $rule_found ) {
     @record=();
     MySQLaccess::Debug::Print(2, "NO record found in the user-table!!");
  }
  else {
     MySQLaccess::Debug::Print(2, "Selected record=@record");
     MySQLaccess::Debug::Print(2, "<=?=> $record");
  }
 
  MySQLaccess::Debug::Print(1, "returning @record");

  return (\@record,\%Access_user); #matching record in user-table
}

# ==========================================================
# sub Get_grant_from_db:
# ==========================================================
sub Get_grant_from_db {
  my ($host,$db,$user,$aref) = @_;

  MySQLaccess::Debug::Print(1, "(host=$host,user=$user,db=$db)");

  my %Access_db    = %{Default_Access_Rights()};
  my $rule_found=0;

  foreach $record (@{$aref}) {
    $full_access=0;
    MySQLaccess::Debug::Print(2, "Read db: $record");
    @record=split(/\t/,$record);

    # check host and db
    # with possible wildcards in field
    # replace mysql-wildcards by reg-wildcards
    my $host_tpl = MySQLaccess::Wildcards::SQL2Reg($record[0]);
    my $db_tpl   = MySQLaccess::Wildcards::SQL2Reg($record[1]);
    my $user_tpl = $record[2]; #user field isn't pattern matched!!
    MySQLaccess::Debug::Print(3, "=>host_tpl : read=$record[0] -> converted=$host_tpl");
    MySQLaccess::Debug::Print(3, "=>db_tpl   : read=$record[1] -> $db_tpl");
    MySQLaccess::Debug::Print(3, "=>user_tpl : read=$record[2] -> $user_tpl");

    if ( ( MySQLaccess::Host::Is_localhost($host_tpl)
           or  MySQLaccess::Wildcards::MatchTemplate($host_tpl,$host_name)
           or  MySQLaccess::Wildcards::MatchTemplate($host_tpl,$host_ip) )
         and ( MySQLaccess::Wildcards::MatchTemplate($db_tpl,$db) )
         and ( MySQLaccess::Wildcards::MatchTemplate($user_tpl,$user) ) ) {
 
      $MySQLaccess::Grant::process_host_table = ($record[0] eq '');

      if ($user_tpl eq '') { push(@Warnings,'public_database'); }

      foreach $field (keys(%D)) {
        $Access_db{$field} = $MySQLaccess::Report::Answer{$record[$D{$field}]};
      }
      $rule_found=1;
      $MySQLaccess::Grant::Rules{'db'} = $record;
      last;
    }
  }

  # -------------------------------
  #  setting privileges to db-priv
  MySQLaccess::Debug::Print(2, "Rights after parsing db-table..:");
  if (! $rule_found ) {
    MySQLaccess::Debug::Print(2, "NO rule found in db-table => no access granted!!");
  }

  return (\@record,\%Access_db);
}

# ==========================================================
# sub Get_grant_from_host:
# ==========================================================
sub Get_grant_from_host {
  my ($host,$db,$aref) = @_;

  MySQLaccess::Debug::Print(1, "Get_grant_from_host()");

  my %Access_host = %{Default_Access_Rights()};

  # the host-table doesn't have to be processed if the host-field
  # in the db-table isn't empty
  if (!$MySQLaccess::Grant::process_host_table) {
    MySQLaccess::Debug::Print(2, ">> Host-table doesn't have to be processed!!");
    $MySQLaccess::Grant::Rules{'host'} = 'no_equiv_host';
    return ([],\%Access_host);
  }

  my $rule_found=0;
  my @record = ();

  foreach $record (@{$aref}) {
    $full_access=0;
    MySQLaccess::Debug::Print(2, "host: $record");
    @record=split(/\t/,$record);

    # check host and db
    # with possible wildcards in field
    # replace mysql-wildcards by reg-wildcards
    my $host_tpl = MySQLaccess::Wildcards::SQL2Reg($record[0]);
    my $db_tpl   = MySQLaccess::Wildcards::SQL2Reg($record[1]);
    MySQLaccess::Debug::Print(3, "=>host_tpl : $record[0] -> $host_tpl");
    MySQLaccess::Debug::Print(3, "=>db_tpl   : $record[1] -> $db_tpl");

    if ( ( MySQLaccess::Host::Is_localhost($host_tpl)
           or MySQLaccess::Wildcards::MatchTemplate($host_tpl,$host_name)
           or MySQLaccess::Wildcards::MatchTemplate($host_tpl,$host_ip) )
         and ( MySQLaccess::Wildcards::MatchTemplate($db_tpl,$db) ) ) {

      $MySQLaccess::Grant::Rules{'host'} = $record;
      $rule_found=1;
      foreach $field (keys(%H)) {
        $Access_host{$field} = $MySQLaccess::Report::Answer{$record[$H{$field}]};
      }
      last;
    }
  }

  # -------------------------------
  #  setting privileges to host-priv
  MySQLaccess::Debug::Print(2, "Rights after parsing host-table..:");
  if (! $rule_found ) {
     @record=();
     MySQLaccess::Debug::Print(2, "NO restrictions found in the host-table!!");
  }

  # --------------------------------
  # debugging access-rights in db 

  return (\@record,\%Access_host); #matching record in host-table
}



# ===========================================================
# sub Default_Access_Rights():
#  return (a reference to) a hash which holds all default
#  priviliges currently defined in the grant-tables.
# ===========================================================
sub Default_Access_Rights {
    my %right = ();

    MySQLaccess::Debug::Print(2, "Debug Default_Access_Rights():");
    # add entry for all fields in the HOST-table
    foreach $field (keys(%MySQLaccess::Grant::H)) {
	$right{$field}='0' unless (defined($right{$field}));
    }
    # add entry for all fields in the DB-table
    foreach $field (keys(%MySQLaccess::Grant::D)) {
	$right{$field}='0' unless (defined($right{$field}));
    }
    # add entry for all fields in the USER-table
    foreach $field (keys(%MySQLaccess::Grant::U)) {
	$right{$field}='0' unless (defined($right{$field}));
    }
    # --------------
    # debugging info
    foreach $field (keys(%right)) { MySQLaccess::Debug::Print(3, sprintf("> %15s : %1s",$field,$right{$field})); }

    return \%right;
}

# ======================================
# sub Make_Privlist
#  Make an ordered list of the privileges
#  that should be reported
# ======================================
sub Make_Privlist {
    # layout:
    #'select_priv',     'create_priv',
    #'insert_priv',     'drop_priv',
    #'update_priv',     'reload_priv',
    #'delete_priv',     'process_priv',
    #'file_priv',       'shutdown_priv');
    my $right;
    my @privlist=();
    foreach $right (@U) {
	if (! grep(/$right/,@privlist)) { push(@privlist,$right); }
    };
    foreach $right (@D) {
	if (! grep(/$right/,@privlist)) { push(@privlist,$right); }
    };
    foreach $right (@H) {
	if (! grep(/$right/,@privlist)) { push(@privlist,$right); }
    };
#       print "Privileges:\n";
#       foreach $field (@privlist) { print " > $field\n"; }
    return \@privlist;
}



########################################################################
package MySQLaccess::Report;
use Exporter ();
@EXPORT = qw(&Print_Header());
BEGIN {
    $FORM = $ENV{'SCRIPT_NAME'};
    $DEBUG     = 0;
    $DEBUG     = $MySQLaccess::DEBUG unless ($DEBUG);

    # translation-table for poss. answers
    %Answer =  ('Y' =>  1 , 'N' =>  0
               , 1  => 'Y',  0  => 'N'
               ,'?' => '?', ''  => '?'
               );
    $headers   = 0;
    $separator = 0;

# ****************************
# Notes and warnings
%MESSAGES = ( 
  'insecure_user' 
   => "Everybody can access your DB as user `\$user' from host `\$host'\n"
     ."WITHOUT supplying a password.\n"
     ."Be very careful about it!!"
 ,'password_required' 
   => "A password is required for user `\$user' :-("
 ,'invalid_password'
   => "The password '\$password' for user `\$user' is invalid :-P"
 , 'valid_password'
   => "You supplied the right password for user `\$user' :-)"
 ,'public_database' 
   => "Any user with the appropriate permissions has access to your DB!\n"
     ."Check your users!"
 ,'full_access' 
   => "All grant-tables are empty, which gives full access to ALL users !!"
 ,'no_rule_found'
   => "No matching rule"
 ,'no_equiv_host' 
   => "Not processed: host-field is not empty in db-table."
 ,'least_priv'
   => "If the final priveliges of the user are more then you gave the user,\n"
     ."check the priveliges in the db-table `\$db'."
 ,'minimum_priv'
   => "The privileges for any new user are AT LEAST\n"
     ."the ones shown in the table above,\n"
     ."since these are the privileges of the db `\$db'.\n"
 ,'not_found_mysql'
   => "The MySQL client program <$MySQLaccess::MYSQL> could not be found.\n"
     ."+ Check your path, or\n"
     ."+ edit the source of this script to point \$MYSQL to the mysql client.\n"
 ,'not_found_mysqldump'
   => "The MySQL dump program <$MySQLaccess::MYSQLDUMP> could not be found.\n"
     ."+ Check your path, or\n"
     ."+ edit the source of this script to point \$MYSQLDUMP to the mysqldump program.\n"
 ,'not_found_diff'
   => "The diff program <$MySQLaccess::DIFF> could not be found.\n"
     ."+ Check your path, or\n"
     ."+ edit the source of this script to point \$DIFF to the diff program.\n"
 ,'not_found_tmp'
   => "The temporary directory <$MySQLaccess::TMP_PATH> could not be found.\n"
     ."+ create this directory (writeable!), or\n"
     ."+ edit the source of this script to point \$TMP_PATH to the right directory.\n"
 ,'write_err_tmp'
   => "The temporary directory <$MySQLaccess::TMP_PATH> is not writable.\n"
     ."+ make this directory writeable!, or\n"
     ."+ edit the source of this script to point \$TMP_PATH to another directory.\n"
 ,'Unrecognized_option'
   => "Sorry,\n"
     ."You are using an old version of the mysql-program,\n"
     ."which does not yet implement a neccessary option.\n"
     ."\n"
     ."You need at least Version 6.2 of the mysql-client,\n"
     ."which was build in MySQL v3.0.18, to use this version\n"
     ."of `$MySQLaccess::script'."
 ,'Access_denied'
   => "Sorry,\n"
     ."An error occured when trying to connect to the database\n"
     ."with the grant-tables:\n"
     ."* Maybe YOU do not have READ-access to this database?\n"
     ."* If you used the -U option, you may have supplied an invalid username?\n"
     ."  for the superuser?\n"
     ."* If you used the -U option, it may be possible you have to supply\n"
     ."  a superuser-password to, with the -P option?\n"
     ."* If you used the -P option, you may have supplied an invalid password?\n"
 ,'Dbaccess_denied'
   => "Sorry,\n"
     ."An error occured when trying to connect to the database\n"
     ."with the grant-tables. (dbaccess denied)\n"
 ,'Unknown_tmp_table'
   => "Sorry,\n"
     ."An error occured when trying to work with the temporary tables in the database\n"
     ."with the grant-tables. (One of the temporary tables does not exist)\n"
 ,'Unknown_table'
   => "Sorry,\n"
     ."An error occured when trying to work with some tables in the database\n"
     ."with the grant-tables. (table does not exist)\n"
 ,'use_old_server'
   => "Sorry,\n"
     ."An error occured when executing an SQL statement.\n"
     ."You might consider altering the use of the parameter `--old_server' when \n"
     ."calling `$MySQLaccess::script'."
 ,'unknown_error'
   => "Sorry,\n"
     ."An error occured when trying to connect to the database\n"
     ."with the grant-tables. (unknown error)\n"
 ,'anonymous_access'
   => "Accessing the db as an anonymous user.\n"
     ."Your username has no relevance\n"
 ,'user_required'
   => "You have to supply a userid."
 ,'db_required'
   => "You have to supply the name of a database."
 ,'host_required'
   => "You have to supply the name of a host."
 );


} 
# =====================================
# sub Print_Header:
#  print header info
# =====================================
sub Print_Header {
    if ($MySQLaccess::CMD) { #command-line mode
    print "$MySQLaccess::script Version $MySQLaccess::VERSION\n"
         ."By RUG-AIV, by Yves Carlier (Yves.Carlier\@rug.ac.be)\n"
         ."Changes by Steve Harvey (sgh\@vex.net)\n"
         ."This software comes with ABSOLUTELY NO WARRANTY.\n";
    }
    if ($MySQLaccess::CGI) { #CGI-BIN mode
    print "content-type: text/html\n\n" 
       . "<HTML>\n"
         ."<HEAD>\n"
         ."<TITLE>MySQLaccess</TITLE>\n"
         ."</HEAD>\n"
         ."<BODY>\n"
         ."<H1>$MySQLaccess::script Version $MySQLaccess::VERSION</H1>\n" 
         ."<CENTER>\n<ADDRESS>\n"
         ."By RUG-AIV, by Yves Carlier (<a href=mailto:Yves.Carlier\@rug.ac.be>Yves.Carlier\@rug.ac.be</a>)<BR>\n"
         ."Changes by Steve Harvey (<a href=mailto:sgh\@vex.net>sgh\@vex.net</a>)<BR>\n"
         ."This software comes with ABSOLUTELY NO WARRANTY.<BR>\n"
         ."</ADDRESS>\n</CENTER>\n"
         ."<HR>\n";
    Print_Taskbar();
    print "<HR>\n";
    }
    return 1;
}

# =====================================
# sub Print_Footer:
#  print footer info
# =====================================
sub Print_Footer {
    if ($MySQLaccess::CMD) { #command-line mode
    print "\n"
         ."BUGs can be reported by email to Yves.Carlier\@rug.ac.be\n";
    }
    if ($MySQLaccess::CGI) { #CGI-BIN mode
    if ($MySQLaccess::Param{'brief'}) {
    print "</table>\n";  #close table in brief-output
    }
    print "<HR>\n"
         ."<ADDRESS>\n"
         ."BUGs can be reported by email to <a href=mailto:Yves.Carlier\@rug.ac.be>Yves.Carlier\@rug.ac.be</a><BR>\n"
#         ."Don't forget to mention the version $VERSION!<BR>\n"
         ."</ADDRESS>\n"
         ."</BODY>\n"
         ."</HTML>\n";
    }
    return 1;
}

# =====================================
# sub Print_Taskbar:
#  print taskbar on STDOUT
# =====================================
sub Print_Taskbar {
    print "<CENTER>\n"
         ."[<a href=$FORM?relnotes=on>Release&nbsp;Notes</a>] \n"
         ."[<a href=$FORM?version=on>Version</a>] \n"
         ."[<a href=$FORM?plan=on>Future&nbsp;Plans</a>] \n"
         ."[<a href=$FORM?howto=on>Examples</a>] \n"
         ."[<a href=$FORM?help=on>New check</a>] \n"
         ."[<a href=$FORM?edit=on>Change/edit ACL</a>] \n"
         ."</CENTER>\n";
    return 1;
}

# =====================================
# sub Print_Form:
#  print CGI-form
# =====================================
sub Print_Form {
print <<EOForm;
<center>
<!-- Quering -->
<FORM method=POST action=$FORM>

<table border width="100%" >
<tr>
  <th>MySQL server</th>
  <th>User information</th>
  <th>Reports</th>
  </tr>

<tr>
  <td valign=top>
  <table>
  <tr>
    <td halign=right><b>Host</b><br><font size=-2>(Host on which MySQL-server resides.)</font></td>
    <td valign=top><INPUT name=rhost type=text size=15 maxlength=15 value="$MySQLaccess::Param{'rhost'}"></td>
    </tr>
  <tr>
    <td halign=right><b>Superuser</b><br><font size=-2>(User which has <font color="Red">read-access</font> to grant-tables.)</font></td>
    <td valign=top><INPUT name=superuser type=text size=15 maxlength=15 value="$MySQLaccess::Param{'superuser'}"></td>
    </tr>
  <tr>
    <td halign=right><b>Password</b><br><font size=-2>(of Superuser.)</font></td>
    <td valign=top><INPUT name=spassword type=password size=15 maxlength=15 value="$MySQLaccess::Param{'spassword'}"></td>
    </tr>
  </table>
  </td>

  <td valign=top>
  <table>
  <tr>
    <td halign=right><b><font color=Red>User</font></b><br><font size=-2>(Userid used to connect to MySQL-database.)</font></td>
    <td halign=top><INPUT name=user type=text size=15 maxlength=15 value="$MySQLaccess::Param{'user'}"></td>
    </tr>
  <tr>
    <td halign=right><b>Password</b><br><font size=-2>(Password user has to give to get access to MySQL-database.)</font></td>
    <td valign=top><INPUT name=password type=password size=15 maxlength=15 value="$MySQLaccess::Param{'password'}"></td>
    </tr>
  <tr>
    <td halign=right><b><font color=Red>Database</font></b><br><font size=-2>(Name of MySQL-database user tries to connect to.</font><br><font size=-2>Wildcards <font color="Green">(*,?,%,_)</font> are allowed.)</font></td>
    <td valign=top><INPUT name=db type=text size=15 maxlength=15 value="$MySQLaccess::Param{'db'}"></td>
    </tr>
  <tr>
    <td halign=right><b>Host</b><br><font size=-2>(Host from where the user is trying to connect to MySQL-database.</font><br><font size=-2>Wildcards <font color="Green">(*,?,%,_)</font> are allowed.)</font></td>
    <td valign=top><INPUT name=host type=text size=15 maxlength=15 value="$MySQLaccess::Param{'host'}"></td>
    </tr>
  </table>
  </td>

  <td valign=center>
  <table cellspacing=5 cellpadding=2 cols=1 height="100%">
  <tr align=center>
    <td halign=right><INPUT type=submit name=brief value="Brief"><br>
                     <INPUT type=submit name=table value="Tabular"></td>
    </tr>
  <tr align=center>
    <td></td>
    </tr>
  <tr align=center>
    <td halign=right><INPUT type=reset value="Clear"></td>
    </tr>
  </table>
  </td>
  </tr>

</table>
</form>


</BODY>
</HTML>
EOForm
    return 1;
}

# =====================================
# sub Print_Usage:
#  print some information on STDOUT
# =====================================
sub Print_Usage {
    Print_Error_Messages();
    if ($MySQLaccess::CMD) { #command-line mode
        Print_Options();
    }
    if ($MySQLaccess::CGI) { #CGI-BIN mode
        Print_Form();
    }    
    return 1;
}

# ======================================
# sub Print_Version:
# ======================================
sub Print_Version {
    if ($MySQLaccess::CMD) {
       print $MySQLaccess::INFO;
    }
    if ($MySQLaccess::CGI) { 
       print "<PRE>\n"; 
       print $MySQLaccess::INFO;
       print "</PRE>\n"; 
    }
    return 1;
}

# ======================================
# sub Print_Relnotes:
# ======================================
sub Print_Relnotes {
    if ($MySQLaccess::CMD) {
       print $MySQLaccess::RELEASE;
    }
    if ($MySQLaccess::CGI) { 
       print "<PRE>\n";
       print $MySQLaccess::RELEASE;
       print "</PRE>\n"; 
    }
    return 1;
}

# ======================================
# sub Print_Plans:
# ======================================
sub Print_Plans {
    if ($MySQLaccess::CMD) {
       print $MySQLaccess::TODO;
    }
    if ($MySQLaccess::CGI) { 
       print "<PRE>\n";
       print $MySQLaccess::TODO;
       print "</PRE>\n"; 
    }
    return 1;
}

# ======================================
# sub Print_HowTo:
# ======================================
sub Print_HowTo {
    if ($MySQLaccess::CMD) {
       print $MySQLaccess::HOWTO;
    }
    if ($MySQLaccess::CGI) { 
       print "<PRE>\n"; 
       print $MySQLaccess::HOWTO;
       print "</PRE>\n"; 
    }
    return 1;
}

# ======================================
# sub Print_Options:
# ======================================
sub Print_Options {
    if ($MySQLaccess::CGI) { print "<PRE>\n"; }
    print $MySQLaccess::OPTIONS;
    if ($MySQLaccess::CGI) { print "</PRE>\n"; }
    return 1;
}

# ======================================
# sub Print_Error_Access:
# ======================================
sub Print_Error_Access {
    my ($error) = @_;
    print "\n";
    if ($MySQLaccess::CGI) { print "<font color=Red>\n<PRE>\n"; }
    print $MESSAGES{$error};
    if ($MySQLaccess::CGI) { print "</PRE>\n</font>\n"; }
    print "\n";
    return 1;
}

# ======================================
# sub Print_Error_Messages:
# ======================================
sub Print_Error_Messages {
#    my ($error) = @_;
    print "\n";
    if ($MySQLaccess::CGI) { print "<font color=Red>\n<center>\n"; }
    foreach $error (@MySQLaccess::Grant::Error) {
       print $MESSAGES{$error};
       print $MySQLaccess::CGI ? "<br>\n" : "\n";
    }
    if ($MySQLaccess::CGI) { print "</center>\n</font>\n"; }
    print "\n";
    return 1;
}

# ======================================
# sub Print_Message:
# ======================================
sub Print_Message {
    my ($aref) = @_;
    my @messages = @{$aref};
    print "\n";
    if ($MySQLaccess::CGI) { print "<font color=DarkGreen>\n<center>\n"; }
    foreach $msg (@messages) {
       print $msg;
       print $MySQLaccess::CGI ? "<br>\n" : "\n";
    }
    if ($MySQLaccess::CGI) { print "</center>\n</font>\n"; }
    print "\n";
    return 1;
}

# ======================================
# sub Print_Edit:
# ======================================
sub Print_Edit {
    print "\n";
    if (!$MySQLaccess::CGI) { 
       print "Note: Editing the temporary tables is NOT supported in CMD-line mode!\n";
       return 0;
    }
    print "<CENTER>\n"
         ."<form action=$FORM method=GET>\n"
         ."<table width=90% border>\n"
         ."<tr>\n"
         ." <td><input type=checkbox name=copy value=on> Copy grant-rules to temporary tables<br></td>\n"
         ." <td rowspan=5 align=center valign=center><input type=submit value=Go></td>\n"
         ."</tr>\n"
         ."<tr>\n"
         ." <td> Edit temporary tables with external application:<br>"
         ." <a href=\"$MySQLaccess::MYSQLADMIN\">$MySQLaccess::MYSQLADMIN</a></td>\n"
         ."</tr>\n"
         ."<tr>\n"
         ." <td><input type=checkbox name=preview value=on> Preview changes made in temporary tables</td>\n"
         ."</tr>\n"
         ."<tr>\n"
         ." <td><input type=checkbox name=commit value=on> Make changes permanent</td>\n"
         ."</tr>\n"
         ."<tr>\n"
         ." <td><input type=checkbox name=rollback value=on> Restore previous grand-rules</td>\n"
         ."</tr>\n"
         ."<tr>\n"
         ." <td colspan=2 align=center><font size=-2 color=Red>You need write,delete and drop-privileges to perform the above actions</font></td>\n"
         ."</tr>\n"
         ."</table>\n"
         ."</form>\n"
         ."</CENTER>\n";

    return 1;
}


# ======================================
# sub Print_Access_rights:
#  print the access-rights on STDOUT
# ======================================
sub Print_Access_rights {
    my ($host,$user,$db,$refhash) = @_;

    if (defined($MySQLaccess::Param{'brief'})) { 
#       if ($MySQLaccess::CGI) { print "<PRE>\n"; }
       Matrix_Report($host,$user,$db,$refhash);  
#       if ($MySQLaccess::CGI) { print "</PRE>\n"; }
    }
    else { 
       Tabular_Report($host,$user,$db,$refhash); 
       $MySQLaccess::Report::separator = $MySQLaccess::CGI ? "<hr>" : "-"x80;
    }
    return 1;
}

# ======================================
# sub Print_Diff_ACL:
#  print the diff. in the grants before and after
# ======================================
sub Print_Diff_ACL {
    my ($aref) = @_;
    my @diffs = @{$aref};
    my %block = ( '<' => 'Before',
                  '>' => 'After',
                );
    my %color = ( '<' => 'Green',
                  '>' => 'Red',
                ); 
    my $curblock = '';

    # -----------------------------
    # create column-headers
    foreach $field (@MySQLaccess::Grant::Privileges) {
      push(@headers,substr($field,0,4));
    }

    if ($MySQLaccess::CMD) {
    print "\n";
    print "Differences in access-rights BEFORE and AFTER changes in grant-tables\n";
#    print "---------------------------------------------------------------------\n";
      my $line1="";
      my $line2="";
      $line1 .= sprintf("| %-30s|",'Host,User,DB');
      $line2 .= sprintf("+-%-30s+",'-' x 30);
      foreach $header (@headers) {
        $line1 .= sprintf("%-4s|",$header);
        $line2 .= sprintf("%s+",'----');
      }
      print "$line2\n";
      print "$line1\n";
      print "$line2\n";

      $format = "format STDOUT = \n"
              . "^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< " . " @|||" x 10 ."\n"
              . '$host_user_db,@priv' . "\n"
              . ".\n";
#print $format;
      eval $format;
    }
    if ($MySQLaccess::CGI) {
    print "<table border width=100%>\n";
    print "<tr>\n";
    print "<th colspan=11>";
    print "Differences in access-rights <font color=$color{'<'}>BEFORE</font> "
         ."and <font color=$color{'>'}>AFTER</font> changes to grant-tables</font>\n";
    print "</th>";
    print "</tr>\n";
    print "<tr>\n";    
    $line1 .= sprintf("<th>%-20s</th>",'Host, User, DB');
    foreach $header (@headers) {
      $line1 .= sprintf("<th>%-4s</th>",$header);
    }
    print "$line1</tr>\n";
    }

    foreach $line (@diffs) {
        $type = substr($line,0,1);
        $line = substr($line,1);
        ($host,$user,$db,@priv) = split(/,/,$line);
        if ($MySQLaccess::CMD) {
           if ($type ne $curblock) {
              $curblock = $type;
              print $block{$curblock},":\n";
           }
           #print "$line\n";
           write;
        }
        if ($MySQLaccess::CGI) {
           if ($type ne $curblock) {
              $curblock = $type;
              print "<tr><td><b>$block{$curblock}<b></td></tr>\n";
           }
           $line1="<td><font color=$color{$type}>$host, $user, $db</font></td>";
           foreach $field (@priv) {
              $line1 .= sprintf("<td align=center><font color=$color{$type}>%-4s</font></td>",$field);
           }
           print "<tr>$line1</tr>\n";
        }
    }
    print      "\n";
    if ($MySQLaccess::CMD) {
    print "---------------------------------------------------------------------\n";
    }
    if ($MySQLaccess::CGI) {
    print      "</table><br>";
    }


    return 1;
}

# ======================================
# sub Tabular_Report
#  Tabular report,
#  suitable for 1 triple (host,db,user)
# ======================================
sub Tabular_Report {
    my ($host,$user,$db,$a) = @_;
    my $column=2;

    # -----------------------------
    # separator
    if ($MySQLaccess::Report::separator) { print "$MySQLaccess::Report::separator\n"; }
    
    # -----------------------------
    # print table of access-rights
    my $rows = int(@MySQLaccess::Grant::Privileges/2);  #round up
    my @table=();
    $j=0;
    for $i (0 .. $rows-1) {
      $table[$j]=$MySQLaccess::Grant::Privileges[$i];
      $j = $j+2;
    }
    $j=1;
    for $i ($rows .. $#MySQLaccess::Grant::Privileges) {
      $table[$j]=$MySQLaccess::Grant::Privileges[$i];
      $j = $j+2;
    }
    if ($MySQLaccess::CMD) {
    print "\n";
    print "Access-rights\n";
    print "for USER '$user', from HOST '$host', to DB '$db'\n";
    }
    if ($MySQLaccess::CGI) {
    print "<table border width=100%>\n";
    print "<tr>\n";
    }
    if ($MySQLaccess::CGI) {
    print "<th colspan=5>";
    print "<font color=Red>Access-rights</font>\n";
    print "for USER '<font color=Green>$user</font>', from HOST '<font color=Green>$host</font>', to DB '<font color=Green>$db</font>'\n";
    print "</th>";
    print "</tr>\n";
    print "<tr>\n";
    }
    if ($MySQLaccess::CMD) {
    print      "\t+-----------------+---+\t+-----------------+---+";
    }
    foreach $field (@table) {
        if ($MySQLaccess::CMD) {
          if ($column==2) { print "\n\t"; $column=1;}
          else            { print "\t";   $column=2;}
          printf "| %-15s | %s |",$field,$Answer{$a->{$field}}; 
        }
        if ($MySQLaccess::CGI) {
          if ($column==2) { print "</tr>\n<tr>\n"; $column=1;}
          else            { print "<td width=10%></td>";   $column=2;}
          printf " <td width=35%><b>%-15s</b></td><td width=10%>%s</td>\n",$field,$Answer{$a->{$field}}; 
        }
    }
    print      "\n";
    if ($MySQLaccess::CMD) {
    print      "\t+-----------------+---+\t+-----------------+---+\n";
    }
    if ($MySQLaccess::CGI) {
    print      "</tr>\n</table><br>";
    }

    # ---------------
    # print notes:
    foreach $note (@MySQLaccess::Grant::Notes) {
      my $message = $MESSAGES{$note};
      $message =~ s/\$user/$user/g; 
      $message =~ s/\$db/$db/g;
      $message =~ s/\$host/$host/g;
      $message =~ s/\$password/$password/g;
      $PREFIX='NOTE';
      if ($MySQLaccess::CMD) {
      my @lines = split(/\n/,$message);
      foreach $line (@lines) { 
        print "$PREFIX:\t $line\n"; 
        $PREFIX='    ';
      }
      }
      if ($MySQLaccess::CGI) {
      print "<b>$PREFIX:</b> $message<br>\n";
      }
    } 

    # ---------------
    # print warnings:
    foreach $warning (@MySQLaccess::Grant::Warnings) {
      my $message = $MESSAGES{$warning};
      $message =~ s/\$user/$user/g;
      $message =~ s/\$db/$db/g;
      $message =~ s/\$host/$host/g;
      $message =~ s/\$password/$password/g;
      $PREFIX='BEWARE';
      if ($MySQLaccess::CMD) {
      my @lines = split(/\n/,$message);
      foreach $line (@lines) { 
        print "$PREFIX:\t $line\n"; 
        $PREFIX='      ';
      }
      }
      if ($MySQLaccess::CGI) {
      print "<b>$PREFIX:</b> $message<br>\n";
      }
    }

    # ---------------
    # print errors:
    foreach $error (@MySQLaccess::Grant::Errors) {
      my $message = $MESSAGES{$error};
      $message =~ s/\$user/$user/g;
      $message =~ s/\$db/$db/g;
      $message =~ s/\$host/$host/g;
      $message =~ s/\$password/$password/g;
      $PREFIX='ERROR';
      if ($MySQLaccess::CMD) {
      my @lines = split(/\n/,$message);
      foreach $line (@lines) { 
        print "$PREFIX:\t $line\n"; 
        $PREFIX='    ';
      }
      }
      if ($MySQLaccess::CGI) {
      print "<b>$PREFIX:</b> $message<br>\n";
      }
    }

    # ---------------
    # inform if there are no rules ==> full access for everyone.
    if ($MySQLaccess::Grant::full_access) { print "$MESSAGES{'full_access'}\n"; }

    # ---------------
    # print the rules used
    print "\n";
    if ($MySQLaccess::CMD) {
    print "The following rules are used:\n";
    foreach $field (sort(keys(%MySQLaccess::Grant::Rules))) {
      my $rule = (defined($MESSAGES{$MySQLaccess::Grant::Rules{$field}}) ? $MESSAGES{$MySQLaccess::Grant::Rules{$field}} : $MySQLaccess::Grant::Rules{$field});
      $rule =~ s/\t/','/g;
      printf " %-5s : '%s'\n",$field,$rule;
    }
    }
    if ($MySQLaccess::CGI) {
    print "<br>\n";
    print "<table border width=100%>\n";
    print "<tr><th colspan=2>The following rules are used:</th></tr>\n";
    foreach $field (sort(keys(%MySQLaccess::Grant::Rules))) {
      my $rule = (defined($MESSAGES{$MySQLaccess::Grant::Rules{$field}}) ? $MESSAGES{$MySQLaccess::Grant::Rules{$field}} : $MySQLaccess::Grant::Rules{$field});
      $rule =~ s/\t/','/g;
      printf "<tr><th>%-5s</th><td>'%s'</td></tr>\n",$field,$rule;
    }
    print "</table>\n";
    }
 
    return 1;
}

# ======================================
# sub Matrix_Report:
#  single-line output foreach triple,
#  no notes,warnings,...
# ======================================
sub Matrix_Report {
    my ($host,$user,$db,$a) = @_;
    my @headers = ();
    
    if (! $headers) {
       # -----------------------------
       # create column-headers
       foreach $field (@MySQLaccess::Grant::Privileges) {
         push(@headers,substr($field,0,4));
       }
    
       # -----------------------------
       # print column-headers
       print "\n";
       if ($MySQLaccess::CMD) {
         my $line1="";
         my $line2="";
         foreach $header (@headers) {
           $line1 .= sprintf("%-4s ",$header);
           $line2 .= sprintf("%s ",'----');
         }
         $line1 .= sprintf("| %-20s",'Host,User,DB');
         $line2 .= sprintf("+ %-20s",'-' x 20);
         print "$line1\n";
         print "$line2\n";
       }
       if ($MySQLaccess::CGI) {
         print "<table width=100% border>\n";
         my $line1="<tr>";
         foreach $header (@headers) {
           $line1 .= sprintf("<th>%-4s</th>",$header);
         }
         $line1 .= sprintf("<th>%-20s</th>",'Host, User, DB');
         print "$line1</tr>\n";
       }

       # ----------------------------
       # column-headers should only be 
       # printed once.
       $MySQLaccess::Report::headers=1;
    }

    # ------------------------
    # print access-information
    if ($MySQLaccess::CMD) {
      foreach $field (@MySQLaccess::Grant::Privileges) {
  	  printf " %-2s  ",$Answer{$a->{$field}}; 
      }
      printf "| %-20s",join(',',$host,$user,$db);
      print "\n";
    }
    if ($MySQLaccess::CGI) {
      print "<tr>";
      foreach $field (@MySQLaccess::Grant::Privileges) {
  	  printf "<td align=center>%-2s</td>",$Answer{$a->{$field}}; 
      }
      printf "<td><b>%-20s</b></td>",join(', ',$host,$user,$db);
      print "</tr>\n";
    }

    return 1;
}


# ======================================
# sub Raw_Report:
#  single-line output foreach triple,
#  no notes,warnings,...
# ======================================
sub Raw_Report {
    my ($host,$user,$db,$a) = @_;
    my @headers = ();
    my $string = "";
    
    # ------------------------
    # print access-information
    $string = "$host,$user,$db,";
    foreach $field (@MySQLaccess::Grant::Privileges) {
	  $string .= $Answer{$a->{$field}} . ","; 
    }
    return $string;
}


#######################################################################
package MySQLaccess::Wildcards;
BEGIN {
    $DEBUG     = 0;
    $DEBUG     = $MySQLaccess::DEBUG unless ($DEBUG);
}
# ############################################
# SQL, WILDCARDS and REGULAR EXPRESSIONS 
# ============================================
# translage SQL-expressions to Reg-expressions
# ============================================
sub SQL2Reg {
    my ($expr) = @_;
    my $expr_o = $expr;
    $expr  =~ s/\./\\./g;
    $expr  =~ s/\\%/\002/g;
    $expr  =~ s/%/.*/g;
    $expr  =~ s/\002/%/g;
    $expr  =~ s/\\_/\002/g;
    $expr  =~ s/_/.+/g;
    $expr  =~ s/\002/_/g;
    MySQLaccess::Debug::Print(2,"$expr_o --> $expr");
    return $expr;
}

# translage WILDcards to Reg-expressions
# ============================================
sub Wild2Reg {
    my ($expr) = @_;
    my $expr_o = $expr;
    $expr  =~ s/\./\\./g;
    $expr  =~ s/\\\*/\002/g;
    $expr  =~ s/\*/.*/g;
    $expr  =~ s/\002/*/g;
    $expr  =~ s/\\\?/\002/g;
    $expr  =~ s/\?/.+/g;
    $expr  =~ s/\002/?/g;
    MySQLaccess::Debug::Print(2,"$expr_o --> $expr");
    return $expr;
}

# =============================================
# match a given string with a template
# =============================================
sub MatchTemplate {
    my ($tpl,$string) = @_;
    my $match=0;
    if ($string=~ /^$tpl$/ or $tpl eq '') { $match=1; }
    else                                  { $match=0;}
    MySQLaccess::Debug::Print(2,"($tpl,$string) --> $match");
    return $match;
}

#######################################################################
package MySQLaccess::Host;
BEGIN {
    $localhost = undef;
    $DEBUG     = 2;
    $DEBUG     = $MySQLaccess::DEBUG unless ($DEBUG);
}
# ======================================
# sub IP2Name
#  return the Name with the corr. IP-nmbr
#  (no aliases yet!!)
# ======================================
sub IP2Name {
    my ($ip) = @_;
    my $ip_o = $ip;
    if ($ip !~ /([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)/o) {
       MySQLaccess::Debug::Print(3,"'$ip' is not an ip-number, returning IP=$ip");
       return $ip;
    }
    MySQLaccess::Debug::Print(4,"IP=$ip split up => $1.$2.$3.$4");
    $ip = pack "C4",$1,$2,$3,$4;
    MySQLaccess::Debug::Print(4,"IP packed -> >>$ip<<\n");
    my ($name,$aliases,$addrtype,$length,@addrs) = gethostbyaddr($ip, AF_INET);
    MySQLaccess::Debug::Print(3,"IP=$ip_o => hostname=$name");
    MySQLaccess::Debug::Print(4,"aliases=$aliases");
    MySQLaccess::Debug::Print(4,"addrtype=$addrtype - length=$length");
    return ($name || $ip);
    #return ($name || undef);
}

# ======================================
# sub Name2IP
#  return the IP-number of the host
# ======================================
sub Name2IP {
    my ($name) = @_;
    if ($name =~ /[%_]/) { 
       MySQLaccess::Debug::Print(3,"'$name' contains SQL-wildcards, returning name=$name");
       return $name; 
    }
    my ($_name,$aliases,$addrtype,$length,@addrs) = gethostbyname($name);
    my ($a,$b,$c,$d) = unpack('C4',$addrs[0]);
    my $ip = "$a.$b.$c.$d";
    MySQLaccess::Debug::Print(3,"hostname=$name => IP=$ip");
    MySQLaccess::Debug::Print(4,"aliases=$aliases");
    MySQLaccess::Debug::Print(4,"addrtype=$addrtype - length=$length");
    #if ($ip ne "") { return "$ip"; }
    #else           { return undef; }
    return ($ip || $name);
}

# ========================================
# sub LocalHost
#  some special action has to be taken for
#  the localhost
# ========================================
sub LocalHost {
    if (!defined($MySQLaccess::Host::localhost)) {
       $MySQLaccess::Host::localhost = Sys::Hostname::hostname();
       MySQLaccess::Debug::Print(3,"Setting package variable \$localhost=$MySQLaccess::Host::localhost");
    }
    my $host = $localhost;
    MySQLaccess::Debug::Print(3,"localhost = $host");
    return $host;
}

# ========================================
# check if the given hostname (or ip)
# corresponds with the localhost
# ========================================
sub Is_localhost {
    my ($host_tpl) = @_;
    my $isit = 0;
    if (($MySQLaccess::host_name eq $localhost) or ($MySQLaccess::host_ip eq $local_ip)) {
	MySQLaccess::Debug::Print(2,"Checking for localhost");
      MySQLaccess::Debug::Print(3,"because ($MySQLaccess::host_name EQ $localhost) AND ($MySQLaccess::host_ip EQ $local_ip)");
      $isit = ( 'localhost' =~ /$host_tpl/ ) ? 1 : 0;
      MySQLaccess::Debug::Print(3," 'localhost' =?= $host_tpl  -> $isit");
      return $isit;
    }
    else {
      MySQLaccess::Debug::Print(4,"Not checking for localhost");
      MySQLaccess::Debug::Print(4,"because ($MySQLaccess::host_name != $localhost) AND ($MySQLaccess::host_ip != $local_ip)");
      return 0;
    }
}


# =========================================
# check if host (IP or name) can be matched
# on the template.
# =========================================
sub MatchTemplate {
    my ($host,$tpl) = @_;
    my $match = 0;
   
    MySQLaccess::Debug::Print(1, "($host) =?= ($tpl)");

    my $host_name = IP2Name($host);
    my $host_ip   = Name2IP($host);

    MySQLaccess::Debug::Print(2, "name=$host_name ; ip=$host_ip");
    $match = (MySQLaccess::Wildcards::MatchTemplate($tpl,$host_name) or
             MySQLaccess::Wildcards::MatchTemplate($tpl,$host_ip));

    MySQLaccess::Debug::Print(2, "($host_name,$host_ip) =?= ($tpl): $ncount");

    return $match;
}

########################################################################
package MySQLaccess::Debug;
BEGIN {
   my $dbg_file = "$MySQLaccess::script_log";
   open(DEBUG,"> $dbg_file") or warn "Could not open outputfile $dbg_file for debugging-info\n";
   select DEBUG;
   $| = 1;
   select STDOUT;
}
# =========================================
# Print debugging information on STDERR
# =========================================
sub Print {
    my ($level,$mesg) = @_;
    my ($pack,$file,$line,$subname,$hasargs,$wantarray) = caller(1);
    my ($PACK)  = split('::',$subname); 
    my $DEBUG = ${$PACK."::DEBUG"} ? ${$PACK."::DEBUG"} : $MySQLaccess::DEBUG ;
    my ($sec,$min,$hour) = localtime();
    print DEBUG "[$hour:$min:$sec $subname] $mesg\n" if ($DEBUG>=$level);
}

