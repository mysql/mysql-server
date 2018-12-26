#!/usr/bin/perl


if ( (scalar(@ARGV) != 2 ) || ($ARGV[0] =~ /[^0-9]/i ) )

{

	if( $ARGV[0] =~ /[^0-9]/i )

	{

		print "\n\nFirst parameter <Rowcount> should be an integer value. \n"	

	}

	&printusage;

}


else

{	 


# Case "InnoDB"


	if ( $ARGV[1] =~ /innodb/i )

	{

		

# First Table "tb1.txt"


		$file = 'innodb_tb1.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

#Data type declarations


		$s_int = 0;	

		$u_int = 0;

		srand();


	#Write data to file

	

		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$char = &get_next_char($count);

			$char_0 = &get_next_char_0($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;


			print ($char_0);		#char(0)##

			print ("\t",$char_0);		#char binary(0)##

			print ("\t",$char_0);		#char ascii(0)##

			print ("\t",$tinytext);		#tiny text unicode##

			print ("\t",$tinytext);		#text

			print ("\t",$longtext);		#medium text

			print ("\t",$longtext);		#long text

			print ("\t",$tinytext);		#tiny blob

			print ("\t",$tinytext);		#blob

			print ("\t",$longtext);		#medium blob

			print ("\t",$longtext);		#long blob

			print ("\t",$char);			#binary

			print ("\t",&random(127));		#tiny int

			print ("\t",&random(255));		#tiny int unsigned

			print ("\t",&random(127));		#tiny int zerofill

			print ("\t",&random(255));		#tiny int unsigned zerofill

			print ("\t",&random(32767));		#smallint

			print ("\t",&random(65535));		#smallint unsigned

			print ("\t",&random(32767));		#smallint zerofill

			print ("\t",&random(65535));		#smallint unsigned zerofill

			print ("\t",$s_int);			#mediumint

			print ("\t",$u_int);			#mediumint unsigned

			print ("\t",$u_int);			#mediumint zerofill

			print ("\t",$u_int);			#mediumint unsigned zerofill

			print ("\t",$s_int);			#int

			print ("\t",$u_int);			#int unsigned

			print ("\t",$u_int);			#int zerofill

			print ("\t",$u_int);			#int unsigned zerofill

			print ("\t",$s_int);			#bigint

			print ("\t",$u_int);			#bigint unsigned

			print ("\t",$u_int);			#bigint zerofill

			print ("\t",$u_int);			#bigint unsigned zerofill

			print ("\t",$s_int);			#decimal

			print ("\t",$u_int);			#decimal unsigned

			print ("\t",$u_int);			#decimal zerofill

			print ("\t",$u_int);			#decimal unsigned zerofill not null

			print ("\t",$s_int);			#decimal(0) not null

			print ("\t",$s_int);			#decimal(254) not null

			print ("\t",$u_int);			#decimal (0) unsigned not null

			print ("\t",$u_int);			#decimal (254) unsigned not null

			print ("\t",$u_int);			#decimal(0) zerofill not null

			print ("\t",$u_int);			#decimal(254) zerofill not null

			print ("\t",$u_int);			#decimal (0) unsigned zerofill not null

			print ("\t",$u_int);			#decimal (254) unsigned zerofill not null

			print ("\t",$s_int);			#decimal (0,0) not null

			print ("\t",$decimal);			#decimal(253,253) not null

			print ("\t",$u_int);			#decimal (0,0) unsigned not null

			print ("\t",$decimal);			#decimal (253,253) unsigned not null

			print ("\t",$u_int);			#decimal(0,0) zerofill not null

			print ("\t",$decimal);			#decimal(253,253) zerofill not null

			print ("\t",$u_int);			#decimal (0,0) unsigned zerofill not null

			print ("\t",$decimal);			#decimal (253,253) unsigned zerofill not null

			print ("\t",$s_int);			#numeric not null

			print ("\t",$u_int);			#numeric unsigned not null

			print ("\t",$u_int);			#numeric zerofill not null

			print ("\t",$u_int);			#numeric unsigned zerofill not null

			print ("\t",$s_int);			#numeric(0) not null

			print ("\t",$s_int);			#numeric(254) not null

			print ("\n");

		}


		select ($oldhandle);

		close (FILE_INPUT);


# Second Table "tb2.txt"


		$file = 'innodb_tb2.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);


		$s_int = 0;	

		$u_int = 0;

		$enum = 1;

		$set = 1;

		$int=0;

		$current_time = "838:59:59";

		$s_time = "00:00:00";

		$s_date = "1970-01-01";

		$current_date = "1000-01-00";

		$current_year = 1901;

		$u_current_float = 1.175494351e-38;

		$s_current_float = -1.175494351e-38;

		srand();


	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$char = &get_next_char($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$decimal = $count. "\." .$count;

			$int = &get_next_int($int);

			$enum = &get_next_enum($enum);

			$set = &get_next_set($set);

			$current_date = &get_next_date($current_date);

			$current_time = &get_next_time($current_time);

			$current_year = &get_next_year($current_year);

			$u_current_float = &get_next_float($u_current_float);

			$s_current_float = &get_next_float($s_current_float);

			$s_time = &get_next_s_time($s_time);

			$s_date = &get_next_s_date($s_date);

			$datetime = &get_next_datetime($s_date,$s_time);

			$timestamp = &get_next_timestamp($s_date,$s_time);


			print ($u_int);			#numeric (0) unsigned

			print ("\t",$u_int);			#numeric (254) unsigned

			print ("\t",$u_int);			#numeric (0) zerofill

			print ("\t",$u_int);			#numeric (254) zerofill

			print ("\t",$u_int);			#numeric (0) unsigned zerofill

			print ("\t",$u_int);			#numeric (254) unsigned zerofill

			print ("\t",$s_int);			#numeric (0,0)

			print ("\t",$decimal);			#numeric (253,253)

			print ("\t",$u_int);			#numeric (0,0) unsigned

			print ("\t",$decimal);			#numeric (253,253) unsigned

			print ("\t",$u_int);			#numeric (0,0) zerofill

			print ("\t",$decimal);			#numeric (253,253) zerofill

			print ("\t",$u_int);			#numeric (0,0) unsigned zerofill

			print ("\t",$decimal);			#numeric (253,253) unsigned zerofill

			print ("\t",$s_current_float);		#real

			print ("\t",$u_current_float);		#real unsigned

			print ("\t",$u_current_float);		#real zerofill

			print ("\t",$u_current_float);		#real unsigned zerofill

			print ("\t",$s_current_float);		#double

			print ("\t",$u_current_float);		#double unsigned

			print ("\t",$u_current_float);		#double zerofill

			print ("\t",$u_current_float);		#double unsigned zerofill

			print ("\t",$s_current_float);		#float not null

			print ("\t",$u_current_float);		#float unsigned not null

			print ("\t",$u_current_float);		#float zerofill not null

			print ("\t",$u_current_float);		#float unsigned zerofill not null

			print ("\t",$s_current_float);		#float(0) not null

			print ("\t",$s_current_float);		#float(23) not null

			print ("\t",$u_current_float);		#float(0) unsigned not null

			print ("\t",$u_current_float);		#float(23) unsigned not null

			print ("\t",$u_current_float);		#float(0) zerofill not null

			print ("\t",$u_current_float);		#float(23) zerofill not null

			print ("\t",$u_current_float);		#float(0) unsigned zerofill not null

			print ("\t",$u_current_float);		#float(23) unsigned zerofill not null

			print ("\t",$s_current_float);		#float(24) not null

			print ("\t",$s_current_float);		#float(53) not null

			print ("\t",$u_current_float);		#float(24) unsigned not null

			print ("\t",$u_current_float);		#float(53) unsigned not null

			print ("\t",$u_current_float);		#float(24) zerofill not null

			print ("\t",$u_current_float);		#float(53) zerofill not null

			print ("\t",$u_current_float);		#float(24) unsigned zerofill not null		

			print ("\t",$u_current_float);		#float(53) unsigned zerofill not null

			print ("\t",$current_date);		#date not null

			print ("\t",$current_time);		#time not null

			print ("\t",$datetime);			#datetime not null

			print ("\t",$timestamp);		#timestamp not null

			print ("\t",$current_year);		#year not null

			print ("\t",$current_year);		#year(3) not null

			print ("\t",$current_year);		#year(4) not null

			print ("\t",$enum);			#enum("1enum","2enum") not null

			print ("\t",$set);			#set("1set","2set") not null 

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);


# Third Table "tb3.txt"


		$file = 'innodb_tb3.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

		$u_int = 0;

		srand();


	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{


			$u_int++;

			$char = &get_next_char($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;


			print ($char);			#char not null

			print ("\t",$char);			#char binary not null

			print ("\t",$char);			#char ascii not null

			print ("\t",$tinytext);			#tinytext not null

			print ("\t",$tinytext);			#text not null

			print ("\t",$longtext);			#mediumtext not null

			print ("\t",$longtext);			#longtext not null unicode##

			print ("\t",$tinytext);			#tinyblob not null

			print ("\t",$tinytext);			#blob not null

			print ("\t",$longtext);			#mediumblob not null

			print ("\t",$longtext);			#longblob not null

			print ("\t",$char);			#binary not null

			print ("\t",&random(127));		#tinyint not null

			print ("\t",&random(255));		#tinyint unsigned not null

			print ("\t",&random(127));		#tinyint zerofill not null

			print ("\t",&random(255));		#tinyint unsigned zerofill not null

			print ("\t",&random(32767));		#smallint not null

			print ("\t",&random(65535));		#smallint unsigned not null

			print ("\t",&random(32767));		#smallint zerofill not null

			print ("\t",&random(65535));		#smallint unsigned zerofill not null

			print ("\t",$s_int);			#mediumint not null

			print ("\t",$u_int);			#mediumint unsigned not null

			print ("\t",$u_int);			#mediumint zerofill not null

			print ("\t",$u_int);			#mediumint unsigned zerofill not null

			print ("\t",$s_int);			#int not null 

			print ("\t",$u_int);			#int unsigned not null

			print ("\t",$u_int);			#int zerofill not null

			print ("\t",$u_int);			#int unsigned zerofill not null

			print ("\t",$s_int);			#bigint not null

			print ("\t",$u_int);			#bigint unsigned not null

 			print ("\t",$u_int);			#bigint zerofill not null

			print ("\t",$u_int);			#bigint unsigned zerofill not null

			print ("\t",$s_int);			#decimal not null

			print ("\t",$u_int);			#decimal unsigned not null

			print ("\t",$u_int);			#decimal zerofill not null

			print ("\t",$u_int);			#decimal unsigned zerofill

			print ("\t",$s_int);			#decimal(0)

			print ("\t",$s_int);			#decimal(254)

			print ("\t",$u_int);			#decimal (0) unsigned 

			print ("\t",$u_int);			#decimal (254) unsigned

			print ("\t",$u_int);			#decimal(0) zerofill 

			print ("\t",$u_int);			#decimal(254) zerofill

			print ("\t",$u_int);			#decimal (0) unsigned zerofill

			print ("\t",$u_int);			#decimal (254) unsigned zerofill

			print ("\t",$s_int);			#decimal (0,0)

			print ("\t",$decimal);			#decimal(253,253)

			print ("\t",$u_int);			#decimal (0,0) unsigned 

			print ("\t",$decimal);			#decimal (253,253) unsigned 

			print ("\t",$u_int);			#decimal(0,0) zerofill 

			print ("\t",$decimal);			#decimal(253,253) zerofill

			print ("\t",$u_int);			#decimal (0,0) unsigned zerofill

			print ("\t",$decimal);			#decimal (253,253) unsigned zerofill

			print ("\t",$s_int);			#numeric 

			print ("\t",$u_int);			#numeric unsigned 

			print ("\t",$u_int);			#numeric zerofill 

			print ("\t",$u_int);			#numeric unsigned zerofill 

			print ("\t",$s_int);			#numeric(0) 

			print ("\t",$s_int);			#numeric(254) 

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);


# Fourth Table "tb4.txt"


		$file = 'innodb_tb4.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

		$enum = 1;

		$ret_bit = 1;

		$set = 1;

		$s_int = 0;	

		$u_int = 0;

		$int=0;

		$s_time = "00:00:00";

		$s_date = "1970-01-01";

		$current_time = "838:59:59";

		$current_date = "1000-01-00";

		$current_year = 1901;

		$u_current_float = 1.175494351e-38;

		$s_current_float = -1.175494351e-38;

		srand();


	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

#			$bit = &get_next_bit(0);

			$char = &get_next_char($count);

			$char_0 = &get_next_char_0($count);

			$char_55 = &get_next_char_55($count);

			$char_90 = &get_next_char_90($count);

			$char_100 = &get_next_char_100($count);

			$char_255 = &get_next_char_255($count);

			$varchar_500 = &get_next_varchar_500($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;

			$int = &get_next_int($int);

			$enum = &get_next_enum($enum);

			$set = &get_next_set($set);

			$current_date = &get_next_date($current_date);

			$current_time = &get_next_time($current_time);

			$current_year = &get_next_year($current_year);

			$u_current_float = &get_next_float($u_current_float);

			$s_current_float = &get_next_float($s_current_float);

			$s_time = &get_next_s_time($s_time);

			$s_date = &get_next_s_date($s_date);

			$datetime = &get_next_datetime($s_date,$s_time);

			$timestamp = &get_next_timestamp($s_date,$s_time);


			print ($u_int);			#numeric (0) unsigned not null

			print ("\t",$u_int);			#numeric (254) unsigned not null

			print ("\t",$u_int);			#numeric (0) zerofill not null

			print ("\t",$u_int);			#numeric (254) zerofill not null

			print ("\t",$u_int);			#numeric (0) unsigned zerofill not null

			print ("\t",$u_int);			#numeric (254) unsigned zerofill not null

			print ("\t",$s_int);			#numeric (0,0) not null

			print ("\t",$decimal);			#numeric (253,253) not null

			print ("\t",$u_int);			#numeric (0,0) unsigned not null

			print ("\t",$decimal);			#numeric (253,253) unsigned not null

			print ("\t",$u_int);			#numeric (0,0) zerofill not null

			print ("\t",$decimal);			#numeric (253,253) zerofill not null

			print ("\t",$u_int);			#numeric (0,0) unsigned zerofill not null

			print ("\t",$decimal);			#numeric (253,253) unsigned zerofill not null

			print ("\t",$s_current_float);		#real not null

			print ("\t",$u_current_float);		#real unsigned not null

			print ("\t",$u_current_float);		#real zerofill not null

			print ("\t",$u_current_float);		#real unsigned zerofill not null

			print ("\t",$s_current_float);		#double not null

			print ("\t",$u_current_float);		#double unsigned not null

			print ("\t",$u_current_float);		#double zerofill not null

			print ("\t",$u_current_float);		#double unsigned zerofill not null

			print ("\t",$s_current_float);		#float

			print ("\t",$u_current_float);		#float unsi	gned 

			print ("\t",$u_current_float);		#float zerofill 

			print ("\t",$u_current_float);		#float unsigned zerofill 

			print ("\t",$s_current_float);		#float(0) 

			print ("\t",$s_current_float);		#float(23) 

			print ("\t",$u_current_float);		#float(0) unsigned 

			print ("\t",$u_current_float);		#float(23) unsigned 

			print ("\t",$u_current_float);		#float(0) zerofill 

			print ("\t",$u_current_float);		#float(23) zerofill

			print ("\t",$u_current_float);		#float(0) unsigned zerofill 

			print ("\t",$u_current_float);		#float(23) unsigned zerofill

			print ("\t",$s_current_float);		#float(24) 

			print ("\t",$s_current_float);		#float(53) 

			print ("\t",$u_current_float);		#float(24) unsigned 

			print ("\t",$u_current_float);		#float(53) unsigned 

			print ("\t",$u_current_float);		#float(24) zerofill 

			print ("\t",$u_current_float);		#float(53) zerofill 

			print ("\t",$u_current_float);		#float(24) unsigned zerofill 							

			print ("\t",$u_current_float);		#float(53) unsigned zerofill 

			print ("\t",$current_date);		#date 

			print ("\t",$current_time);		#time 

			print ("\t",$datetime);			#datetime 

			print ("\t",$timestamp);		#timestamp 

			print ("\t",$current_year);		#year 

			print ("\t",$current_year);		#year(3) 

			print ("\t",$current_year);		#year(4) 

			print ("\t",$enum);			#enum("1enum","2enum") 

			print ("\t",$set);			#set("1set","2set")

			print ("\t",$char_0);			#char(0) unicode##

 			print ("\t",$char_90);			#char(90)##
	
			print ("\t",$char_255);			#char(255) ascii##

			print ("\t",$char_0); 			#varchar(0)##

			print ("\t",$varchar_500);		#varchar(20000) binary##

			print ("\t",$varchar_500);		#varchar(2000) unicode##

			print ("\t",$char_100);		#char(100) unicode##

#			print ("\t",$bit);			#Bit(0)##
	
			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);

	}



#Next Database "MyIsam"


	elsif ( $ARGV[1] =~ /myisam/i )

	{


# Fifth Table "tb5.txt"


		$file = 'myisam_tb1.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

		

	#Data type declarations


		$s_int = 0;	

		$u_int = 0;

		srand();



	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$char = &get_next_char($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;


			print ($char);			#char

			print ("\t",$char);			#char binary

			print ("\t",$char);			#char ascii

			print ("\t",$tinytext);			#tiny text unicode##

			print ("\t",$tinytext);			#text

			print ("\t",$longtext);			#medium text

			print ("\t",$longtext);			#long text

			print ("\t",$tinytext);			#tiny blob

			print ("\t",$tinytext);			#blob

			print ("\t",$longtext);			#medium blob

			print ("\t",$longtext);			#long blob

			print ("\t",$char);			#binary

			print ("\t",&random(127));		#tiny int

			print ("\t",&random(255));		#tiny int unsigned

			print ("\t",&random(127));		#tiny int zerofill

			print ("\t",&random(255));		#tiny int unsigned zerofill

			print ("\t",&random(32767));		#smallint

			print ("\t",&random(65535));		#smallint unsigned

			print ("\t",&random(32767));		#smallint zerofill

			print ("\t",&random(65535));		#smallint unsigned zerofill

			print ("\t",$s_int);			#mediumint

			print ("\t",$u_int);			#mediumint unsigned

			print ("\t",$u_int);			#mediumint zerofill

			print ("\t",$u_int);			#mediumint unsigned zerofill

			print ("\t",$s_int);			#int

			print ("\t",$u_int);			#int unsigned

			print ("\t",$u_int);			#int zerofill

			print ("\t",$u_int);			#int unsigned zerofill

			print ("\t",$s_int);			#bigint

			print ("\t",$u_int);			#bigint unsigned

			print ("\t",$u_int);			#bigint zerofill

			print ("\t",$u_int);			#bigint unsigned zerofill

			print ("\t",$s_int);			#decimal not null

			print ("\t",$u_int);			#decimal unsigned not null

			print ("\t",$u_int);			#decimal zerofill not null

			print ("\t",$u_int);			#decimal unsigned zerofill not null

			print ("\t",$s_int);			#decimal(0) not null

			print ("\t",$s_int);			#decimal(254) not null

			print ("\t",$u_int);			#decimal (0) unsigned not null

			print ("\t",$u_int);			#decimal (254) unsigned not null

			print ("\t",$u_int);			#decimal(0) zerofill not null

			print ("\t",$u_int);			#decimal(254) zerofill not null

			print ("\t",$u_int);			#decimal (0) unsigned zerofill not null

			print ("\t",$u_int);			#decimal (254) unsigned zerofill not null

			print ("\t",$s_int);			#decimal (0,0) not null

			print ("\t",$decimal);			#decimal(253,253) not null

			print ("\t",$u_int);			#decimal (0,0) unsigned not null

			print ("\t",$decimal);			#decimal (253,253) unsigned not null

			print ("\t",$u_int);			#decimal(0,0) zerofill not null

			print ("\t",$decimal);			#decimal(253,253) zerofill not null

			print ("\t",$u_int);			#decimal (0,0) unsigned zerofill not null

			print ("\t",$decimal);			#decimal (253,253) unsigned zerofill not null

			print ("\t",$s_int);			#numeric not null

			print ("\t",$u_int);			#numeric unsigned not null

			print ("\t",$u_int);			#numeric zerofill not null

			print ("\t",$u_int);			#numeric unsigned zerofill not null

			print ("\t",$s_int);			#numeric(0) not null

			print ("\t",$s_int);			#numeric(254) not null

			print ("\n");

		}


		select ($oldhandle);

		close (FILE_INPUT);


# Sixth Table "tb6.txt"


		$file = 'myisam_tb2.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);


		$enum = 1;

		$set = 1;

		$s_int = 0;	

		$u_int = 0;

		$int=0;

		$s_time = "00:00:00";

		$s_date = "1970-01-01";		

		$current_time = "838:59:59";

		$current_date = "1000-01-00";

		$current_year = 1901;

		$u_current_float = 1.175494351e-38;

		$s_current_float = -1.175494351e-38;

		srand();


	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$char = &get_next_char($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$decimal = $count. "\." .$count;

			$int = &get_next_int($int);

			$enum = &get_next_enum($enum);

			$set = &get_next_set($set);

			$current_date = &get_next_date($current_date);

			$current_time = &get_next_time($current_time);

			$current_year = &get_next_year($current_year);

			$u_current_float = &get_next_float($u_current_float);

			$s_current_float = &get_next_float($s_current_float);

			$s_time = &get_next_s_time($s_time);

			$s_date = &get_next_s_date($s_date);

			$datetime = &get_next_datetime($s_date,$s_time);

			$timestamp = &get_next_timestamp($s_date,$s_time);


			$geometry = &get_next_geometry($count);

			$point = &get_next_point($count);

			$linestring = &get_next_linestring($count);

			$polygon = &get_next_polygon($count);

			$geometry_collection = &get_next_geometry_collection($count);

			$multipoint = &get_next_multipoint($count);

			$multilinestring = &get_next_multilinestring($count);

			$multipolygon = &get_next_multipolygon($count);


			print ($u_int);			#numeric (0) unsigned

			print ("\t",$u_int);			#numeric (254) unsigned

			print ("\t",$u_int);			#numeric (0) zerofill

			print ("\t",$u_int);			#numeric (254) zerofill

			print ("\t",$u_int);			#numeric (0) unsigned zerofill

			print ("\t",$u_int);			#numeric (254) unsigned zerofill

			print ("\t",$s_int);			#numeric (0,0)

			print ("\t",$decimal);			#numeric (253,253)

			print ("\t",$u_int);			#numeric (0,0) unsigned

			print ("\t",$decimal);			#numeric (253,253) unsigned

			print ("\t",$u_int);			#numeric (0,0) zerofill

			print ("\t",$decimal);			#numeric (253,253) zerofill

			print ("\t",$u_int);			#numeric (0,0) unsigned zerofill

			print ("\t",$decimal);			#numeric (253,253) unsigned zerofill

			print ("\t",$s_current_float);		#real

			print ("\t",$u_current_float);		#real unsigned

			print ("\t",$u_current_float);		#real zerofill

			print ("\t",$u_current_float);		#real unsigned zerofill

			print ("\t",$s_current_float);		#double

			print ("\t",$u_current_float);		#double unsigned

			print ("\t",$u_current_float);		#double zerofill

			print ("\t",$u_current_float);		#double unsigned zerofill

			print ("\t",$s_current_float);		#float not null

			print ("\t",$u_current_float);		#float unsigned not null

			print ("\t",$u_current_float);		#float zerofill not null

			print ("\t",$u_current_float);		#float unsigned zerofill not null

			print ("\t",$s_current_float);		#float(0) not null

			print ("\t",$s_current_float);		#float(23) not null

			print ("\t",$u_current_float);		#float(0) unsigned not null

			print ("\t",$u_current_float);		#float(23) unsigned not null

			print ("\t",$u_current_float);		#float(0) zerofill not null

			print ("\t",$u_current_float);		#float(23) zerofill not null

			print ("\t",$u_current_float);		#float(0) unsigned zerofill not null

			print ("\t",$u_current_float);		#float(23) unsigned zerofill not null

			print ("\t",$s_current_float);		#float(24) not null

			print ("\t",$s_current_float);		#float(53) not null

			print ("\t",$u_current_float);		#float(24) unsigned not null

			print ("\t",$u_current_float);		#float(53) unsigned not null

			print ("\t",$u_current_float);		#float(24) zerofill not null

			print ("\t",$u_current_float);		#float(53) zerofill not null

			print ("\t",$u_current_float);		#float(24) unsigned zerofill not null		

			print ("\t",$u_current_float);		#float(53) unsigned zerofill not null

			print ("\t",$current_date);		#date not null

			print ("\t",$current_time);		#time not null

			print ("\t",$datetime);			#datetime not null

			print ("\t",$timestamp);		#timestamp not null

			print ("\t",$current_year);		#year not null

			print ("\t",$current_year);		#year(3) not null

			print ("\t",$current_year);		#year(4) not null

			print ("\t",$enum);			#enum("1enum","2enum") not null

			print ("\t",$set);			#set("1set","2set") not null

			print ("\t",$geometry);		#geometry not null

			print ("\t",$point);			#point not null

			print ("\t",$linestring);			#linestring not null

			print ("\t",$polygon);			#polygon not null

			print ("\t",$geometry_collection);	#geometrycollection not null

			print ("\t",$multipoint);		#multipoint not null

			print ("\t",$multilinestring);		#multilinestring not null

			print ("\t",$multipolygon);		#multipolygon not null


#geometry not null, point not null, linestring not null, polygon not null, geometrycollection not null, multipoint not null, multilinestring not null, multipolygon not null 

 

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);


# Seventh Table "tb7.txt"


		$file = 'myisam_tb3.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

		$u_int = 0;

		srand();


	#Write data to file

		for ($count=0; $count < $ARGV[0]; $count++)

		{


			$u_int++;

			$char = &get_next_char($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;


			print ($char);			#char not null

			print ("\t",$char);			#char binary not null

			print ("\t",$char);			#char ascii not null

			print ("\t",$tinytext);			#tinytext not null

			print ("\t",$tinytext);			#text not null

			print ("\t",$longtext);			#mediumtext not null

			print ("\t",$longtext);			#longtext not null unicode##

			print ("\t",$tinytext);			#tinyblob not null

			print ("\t",$tinytext);			#blob not null

			print ("\t",$longtext);			#mediumblob not null

			print ("\t",$longtext);			#longblob not null

			print ("\t",$char);			#binary not null

			print ("\t",&random(127));		#tinyint not null

			print ("\t",&random(255));		#tinyint unsigned not null

			print ("\t",&random(127));		#tinyint zerofill not null

			print ("\t",&random(255));		#tinyint unsigned zerofill not null

			print ("\t",&random(32767));		#smallint not null

			print ("\t",&random(65535));		#smallint unsigned not null

			print ("\t",&random(32767));		#smallint zerofill not null

			print ("\t",&random(65535));		#smallint unsigned zerofill not null

			print ("\t",$s_int);			#mediumint not null

			print ("\t",$u_int);			#mediumint unsigned not null

			print ("\t",$u_int);			#mediumint zerofill not null

			print ("\t",$u_int);			#mediumint unsigned zerofill not null

			print ("\t",$s_int);			#int not null 

			print ("\t",$u_int);			#int unsigned not null

			print ("\t",$u_int);			#int zerofill not null

			print ("\t",$u_int);			#int unsigned zerofill not null

			print ("\t",$s_int);			#bigint not null

			print ("\t",$u_int);			#bigint unsigned not null

 			print ("\t",$u_int);			#bigint zerofill not null

			print ("\t",$u_int);			#bigint unsigned zerofill not null

			print ("\t",$s_int);			#decimal 

			print ("\t",$u_int);			#decimal unsigned 

			print ("\t",$u_int);			#decimal zerofill 

			print ("\t",$u_int);			#decimal unsigned zerofill

			print ("\t",$s_int);			#decimal(0)

			print ("\t",$s_int);			#decimal(254)

			print ("\t",$u_int);			#decimal (0) unsigned 

			print ("\t",$u_int);			#decimal (254) unsigned

			print ("\t",$u_int);			#decimal(0) zerofill 

			print ("\t",$u_int);			#decimal(254) zerofill

			print ("\t",$u_int);			#decimal (0) unsigned zerofill

			print ("\t",$u_int);			#decimal (254) unsigned zerofill

			print ("\t",$s_int);			#decimal (0,0)

			print ("\t",$decimal);			#decimal(253,253)

			print ("\t",$u_int);			#decimal (0,0) unsigned 

			print ("\t",$decimal);			#decimal (253,253) unsigned 

			print ("\t",$u_int);			#decimal(0,0) zerofill 

			print ("\t",$decimal);			#decimal(253,253) zerofill

			print ("\t",$u_int);			#decimal (0,0) unsigned zerofill

			print ("\t",$decimal);			#decimal (253,253) unsigned zerofill

			print ("\t",$s_int);			#numeric 

			print ("\t",$u_int);			#numeric unsigned 

			print ("\t",$u_int);			#numeric zerofill 

			print ("\t",$u_int);			#numeric unsigned zerofill 

			print ("\t",$s_int);			#numeric(0) 

			print ("\t",$s_int);			#numeric(254) 

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);


# Eighth Table "tb8.txt"


		$file = 'myisam_tb4.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);


		$enum = 1;

		$set = 1;

		$ret_bit = 1;

		$s_int = 0;	

		$u_int = 0;

		$s_time = "00:00:00";

		$s_date = "1970-01-01";

		$int=0;

		$current_time = "838:59:59";

		$current_date = "1000-01-00";

		$current_year = 1901;

		$u_current_float = 1.175494351e-38;

		$s_current_float = -1.175494351e-38;

		srand();


	#Write data to file

		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$bit = &get_next_bit(30);

			$char = &get_next_char($count);

			$char_0 = &get_next_char_0($count);

			$char_55 = &get_next_char_55($count);

			$char_90 = &get_next_char_90($count);

			$char_100 = &get_next_char_100($count);

			$char_255 = &get_next_char_255($count);

			$varchar_500 = &get_next_varchar_500($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;

			$enum = &get_next_enum($enum);

			$set = &get_next_set($set);

			$current_date = &get_next_date($current_date);

			$current_time = &get_next_time($current_time);

			$current_year = &get_next_year($current_year);

			$u_current_float = &get_next_float($u_current_float);

			$s_current_float = &get_next_float($s_current_float);

			$s_time = &get_next_s_time($s_time);

			$s_date = &get_next_s_date($s_date);

			$datetime = &get_next_datetime($s_date,$s_time);

			$timestamp = &get_next_timestamp($s_date,$s_time);

			$geometry = &get_next_geometry($count);

			$point = &get_next_point($count);

			$linestring = &get_next_linestring($count);

			$polygon = &get_next_polygon($count);

			$geometry_collection = &get_next_geometry_collection($count);

			$multipoint = &get_next_multipoint($count);

			$multilinestring = &get_next_multilinestring($count);

			$multipolygon = &get_next_multipolygon($count);


			print ($u_int);			#numeric (0) unsigned not null

			print ("\t",$u_int);			#numeric (254) unsigned not null

			print ("\t",$u_int);			#numeric (0) zerofill not null

			print ("\t",$u_int);			#numeric (254) zerofill not null

			print ("\t",$u_int);			#numeric (0) unsigned zerofill not null

			print ("\t",$u_int);			#numeric (254) unsigned zerofill not null

			print ("\t",$s_int);			#numeric (0,0) not null

			print ("\t",$decimal);			#numeric (253,253) not null

			print ("\t",$u_int);			#numeric (0,0) unsigned not null

			print ("\t",$decimal);			#numeric (253,253) unsigned not null

			print ("\t",$u_int);			#numeric (0,0) zerofill not null

			print ("\t",$decimal);			#numeric (253,253) zerofill not null

			print ("\t",$u_int);			#numeric (0,0) unsigned zerofill not null

			print ("\t",$decimal);			#numeric (253,253) unsigned zerofill not null

			print ("\t",$s_current_float);		#real not null

			print ("\t",$u_current_float);		#real unsigned not null

			print ("\t",$u_current_float);		#real zerofill not null

			print ("\t",$u_current_float);		#real unsigned zerofill not null

			print ("\t",$s_current_float);		#double not null

			print ("\t",$u_current_float);		#double unsigned not null

			print ("\t",$u_current_float);		#double zerofill not null

			print ("\t",$u_current_float);		#double unsigned zerofill not null

			print ("\t",$s_current_float);		#float

			print ("\t",$u_current_float);		#float unsigned 

			print ("\t",$u_current_float);		#float zerofill 

			print ("\t",$u_current_float);		#float unsigned zerofill 

			print ("\t",$s_current_float);		#float(0) 

			print ("\t",$s_current_float);		#float(23) 

			print ("\t",$u_current_float);		#float(0) unsigned 

			print ("\t",$u_current_float);		#float(23) unsigned 

			print ("\t",$u_current_float);		#float(0) zerofill 

			print ("\t",$u_current_float);		#float(23) zerofill

			print ("\t",$u_current_float);		#float(0) unsigned zerofill 

			print ("\t",$u_current_float);		#float(23) unsigned zerofill

			print ("\t",$s_current_float);		#float(24) 

			print ("\t",$s_current_float);		#float(53) 

			print ("\t",$u_current_float);		#float(24) unsigned 

			print ("\t",$u_current_float);		#float(53) unsigned 

			print ("\t",$u_current_float);		#float(24) zerofill 

			print ("\t",$u_current_float);		#float(53) zerofill 

			print ("\t",$u_current_float);		#float(24) unsigned zerofill 			

			print ("\t",$u_current_float);		#float(53) unsigned zerofill 

			print ("\t",$current_date);		#date 

			print ("\t",$current_time);		#time 

			print ("\t",$datetime);			#datetime 

			print ("\t",$timestamp);		#timestamp 

			print ("\t",$current_year);		#year 

			print ("\t",$current_year);		#year(3) 

			print ("\t",$current_year);		#year(4) 

			print ("\t",$enum);			#enum("1enum","2enum") 

			print ("\t",$set);			#set("1set","2set")

			print ("\t",$geometry);		#geometry

			print ("\t",$point);			#point 

			print ("\t",$linestring);			#linestring 

			print ("\t",$polygon);			#polygon 

			print ("\t",$geometry_collection);	#geometrycollection 

			print ("\t",$multipoint);		#multipoint 

			print ("\t",$multilinestring);		#multilinestring 

			print ("\t",$multipolygon);		#multipolygon 

			print ("\t",$char_255);		#char(255) unicode##

			print ("\t",$char_55);			#char(60) ascii##

			print ("\t",$char_255);		#char(255) binary##

			print ("\t",$char_0);			#varchar(0) binary##

			print ("\t",$varchar_500);		#varbinary(20000)##

			print ("\t",$char_100);		#varchar(120) unicode##

			print ("\t",$char_100);		#char(100) unicode##

			print ("\t",$bit);			#bit(30)##
			
#geometry, point, linestring, polygon, geometrycollection, multipoint, multilinestring, multipolygon 


			print ("\n");

		}		

		select ($oldhandle);

		close (FILE_INPUT);

	}



	elsif ( $ARGV[1] =~ /memory/i )

	{


# Ninth Table "tb9.txt"


		$file = 'memory_tb1.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

		

	#Data type declarations


		$s_int = 0;	

		$u_int = 0;

		srand();



	#Write data to file

		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$char = &get_next_char($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;


			print ($char);			#char

			print ("\t",$char);			#char binary

			print ("\t",$char);			#char ascii

			print ("\t",$char);			#binary

			print ("\t",&random(127));		#tiny int

			print ("\t",&random(255));		#tiny int unsigned

			print ("\t",&random(127));		#tiny int zerofill

			print ("\t",&random(255));		#tiny int unsigned zerofill

			print ("\t",&random(32767));		#smallint

			print ("\t",&random(65535));		#smallint unsigned

			print ("\t",&random(32767));		#smallint zerofill

			print ("\t",&random(65535));		#smallint unsigned zerofill

			print ("\t",$s_int);			#mediumint

			print ("\t",$u_int);			#mediumint unsigned

			print ("\t",$u_int);			#mediumint zerofill

			print ("\t",$u_int);			#mediumint unsigned zerofill

			print ("\t",$s_int);			#int

			print ("\t",$u_int);			#int unsigned

			print ("\t",$u_int);			#int zerofill

			print ("\t",$u_int);			#int unsigned zerofill

			print ("\t",$s_int);			#bigint

			print ("\t",$u_int);			#bigint unsigned

			print ("\t",$u_int);			#bigint zerofill

			print ("\t",$u_int);			#bigint unsigned zerofill

			print ("\t",$s_int);			#decimal

			print ("\t",$u_int);			#decimal unsigned

			print ("\t",$u_int);			#decimal zerofill

			print ("\t",$u_int);			#decimal unsigned zerofill not null

			print ("\t",$s_int);			#decimal(0) not null

			print ("\t",$s_int);			#decimal(254) not null

			print ("\t",$u_int);			#decimal (0) unsigned not null

			print ("\t",$u_int);			#decimal (254) unsigned not null

			print ("\t",$u_int);			#decimal(0) zerofill not null

			print ("\t",$u_int);			#decimal(254) zerofill not null

			print ("\t",$u_int);			#decimal (0) unsigned zerofill not null

			print ("\t",$u_int);			#decimal (254) unsigned zerofill not null

			print ("\t",$s_int);			#decimal (0,0) not null

			print ("\t",$decimal);			#decimal(253,253) not null

			print ("\t",$u_int);			#decimal (0,0) unsigned not null

			print ("\t",$decimal);			#decimal (253,253) unsigned not null

			print ("\t",$u_int);			#decimal(0,0) zerofill not null

			print ("\t",$decimal);			#decimal(253,253) zerofill not null

			print ("\t",$u_int);			#decimal (0,0) unsigned zerofill not null

			print ("\t",$decimal);			#decimal (253,253) unsigned zerofill not null

			print ("\t",$s_int);			#numeric not null

			print ("\t",$u_int);			#numeric unsigned not null

			print ("\t",$u_int);			#numeric zerofill not null

			print ("\t",$u_int);			#numeric unsigned zerofill not null

			print ("\t",$s_int);			#numeric(0) not null

			print ("\t",$s_int);			#numeric(254) not null

			print ("\n");

		}


		select ($oldhandle);

		close (FILE_INPUT);




# Tenth Table "tb10.txt"


		$file = 'memory_tb2.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);


		$enum = 1;

		$set = 1;

		$s_int = 0;	

		$u_int = 0;

		$s_time = "00:00:00";

		$s_date = "1970-01-01";

		$int=0;

		$current_time = "838:59:59";

		$current_date = "1000-01-00";

		$current_year = 1901;

		$u_current_float = 1.175494351e-38;

		$s_current_float = -1.175494351e-38;

		srand();


	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

			$char = &get_next_char($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$decimal = $count. "\." .$count;

			$int = &get_next_int($int);

			$enum = &get_next_enum($enum);

			$set = &get_next_set($set);

			$current_date = &get_next_date($current_date);

			$current_time = &get_next_time($current_time);

			$current_year = &get_next_year($current_year);

			$u_current_float = &get_next_float($u_current_float);

			$s_current_float = &get_next_float($s_current_float);

			$s_time = &get_next_s_time($s_time);

			$s_date = &get_next_s_date($s_date);

			$datetime = &get_next_datetime($s_date,$s_time);

			$timestamp = &get_next_timestamp($s_date,$s_time);


			print ($u_int);			#numeric (0) unsigned

			print ("\t",$u_int);			#numeric (254) unsigned

			print ("\t",$u_int);			#numeric (0) zerofill

			print ("\t",$u_int);			#numeric (254) zerofill

			print ("\t",$u_int);			#numeric (0) unsigned zerofill

			print ("\t",$u_int);			#numeric (254) unsigned zerofill

			print ("\t",$s_int);			#numeric (0,0)

			print ("\t",$decimal);			#numeric (253,253)

			print ("\t",$u_int);			#numeric (0,0) unsigned

			print ("\t",$decimal);			#numeric (253,253) unsigned

			print ("\t",$u_int);			#numeric (0,0) zerofill

			print ("\t",$decimal);			#numeric (253,253) zerofill

			print ("\t",$u_int);			#numeric (0,0) unsigned zerofill

			print ("\t",$decimal);			#numeric (253,253) unsigned zerofill

			print ("\t",$s_current_float);		#real

			print ("\t",$u_current_float);		#real unsigned

			print ("\t",$u_current_float);		#real zerofill

			print ("\t",$u_current_float);		#real unsigned zerofill

			print ("\t",$s_current_float);		#double

			print ("\t",$u_current_float);		#double unsigned

			print ("\t",$u_current_float);		#double zerofill

			print ("\t",$u_current_float);		#double unsigned zerofill

			print ("\t",$s_current_float);		#float not null

			print ("\t",$u_current_float);		#float unsigned not null

			print ("\t",$u_current_float);		#float zerofill not null

			print ("\t",$u_current_float);		#float unsigned zerofill not null

			print ("\t",$s_current_float);		#float(0) not null

			print ("\t",$s_current_float);		#float(23) not null

			print ("\t",$u_current_float);		#float(0) unsigned not null

			print ("\t",$u_current_float);		#float(23) unsigned not null

			print ("\t",$u_current_float);		#float(0) zerofill not null

			print ("\t",$u_current_float);		#float(23) zerofill not null

			print ("\t",$u_current_float);		#float(0) unsigned zerofill not null

			print ("\t",$u_current_float);		#float(23) unsigned zerofill not null

			print ("\t",$s_current_float);		#float(24) not null

			print ("\t",$s_current_float);		#float(53) not null

			print ("\t",$u_current_float);		#float(24) unsigned not null

			print ("\t",$u_current_float);		#float(53) unsigned not null

			print ("\t",$u_current_float);		#float(24) zerofill not null

			print ("\t",$u_current_float);		#float(53) zerofill not null

			print ("\t",$u_current_float);		#float(24) unsigned zerofill not null		

			print ("\t",$u_current_float);		#float(53) unsigned zerofill not null

			print ("\t",$current_date);		#date not null

			print ("\t",$current_time);		#time not null

			print ("\t",$datetime);			#datetime not null

			print ("\t",$timestamp);		#timestamp not null

			print ("\t",$current_year);		#year not null

			print ("\t",$current_year);		#year(3) not null

			print ("\t",$current_year);		#year(4) not null

			print ("\t",$enum);			#enum("1enum","2enum") not null

			print ("\t",$set);			#set("1set","2set") not null 

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);



# Eleventh Table "tb11.txt"


		$file = 'memory_tb3.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);

		$u_int = 0;

		srand();


	#Write data to file


		for ($count=0; $count < $ARGV[0]; $count++)

		{


			$u_int++;

			$char = &get_next_char($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;


			print ($char);			#char not null

			print ("\t",$char);			#char binary not null

			print ("\t",$char);			#char ascii not null

# OBN - Added two strings to address the missing f121, f122 used in trigger testing
#       as char(50). In MyISAM, innodb they are text and tinytext
			print ("\t",$char);			#char (50)
			print ("\t",$char);			#char (50)
# OBN - End of modification

			print ("\t",$char);			#binary not null

			print ("\t",&random(127));		#tinyint not null

			print ("\t",&random(255));		#tinyint unsigned not null

			print ("\t",&random(127));		#tinyint zerofill not null

			print ("\t",&random(255));		#tinyint unsigned zerofill not null

			print ("\t",&random(32767));		#smallint not null

			print ("\t",&random(65535));		#smallint unsigned not null

			print ("\t",&random(32767));		#smallint zerofill not null

			print ("\t",&random(65535));		#smallint unsigned zerofill not null

			print ("\t",$s_int);			#mediumint not null

			print ("\t",$u_int);			#mediumint unsigned not null

			print ("\t",$u_int);			#mediumint zerofill not null

			print ("\t",$u_int);			#mediumint unsigned zerofill not null

			print ("\t",$s_int);			#int not null 

			print ("\t",$u_int);			#int unsigned not null

			print ("\t",$u_int);			#int zerofill not null

			print ("\t",$u_int);			#int unsigned zerofill not null

			print ("\t",$s_int);			#bigint not null

			print ("\t",$u_int);			#bigint unsigned not null

 			print ("\t",$u_int);			#bigint zerofill not null

			print ("\t",$u_int);			#bigint unsigned zerofill not null

			print ("\t",$s_int);			#decimal not null

			print ("\t",$u_int);			#decimal unsigned not null

			print ("\t",$u_int);			#decimal zerofill not null

			print ("\t",$u_int);			#decimal unsigned zerofill

			print ("\t",$s_int);			#decimal(0)

			print ("\t",$s_int);			#decimal(254)

			print ("\t",$u_int);			#decimal (0) unsigned 

			print ("\t",$u_int);			#decimal (254) unsigned

			print ("\t",$u_int);			#decimal(0) zerofill 

			print ("\t",$u_int);			#decimal(254) zerofill

			print ("\t",$u_int);			#decimal (0) unsigned zerofill

			print ("\t",$u_int);			#decimal (254) unsigned zerofill

			print ("\t",$s_int);			#decimal (0,0)

			print ("\t",$decimal);			#decimal(253,253)

			print ("\t",$u_int);			#decimal (0,0) unsigned 

			print ("\t",$decimal);			#decimal (253,253) unsigned 

			print ("\t",$u_int);			#decimal(0,0) zerofill 

			print ("\t",$decimal);			#decimal(253,253) zerofill

			print ("\t",$u_int);			#decimal (0,0) unsigned zerofill

			print ("\t",$decimal);			#decimal (253,253) unsigned zerofill

			print ("\t",$s_int);			#numeric 

			print ("\t",$u_int);			#numeric unsigned 

			print ("\t",$u_int);			#numeric zerofill 

			print ("\t",$u_int);			#numeric unsigned zerofill 

			print ("\t",$s_int);			#numeric(0) 

			print ("\t",$s_int);			#numeric(254) 

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);



# Twelfth Table "tb12.txt"


		$file = 'memory_tb4.txt';

		open ( FILE_INPUT, ">$file");

		$oldhandle = select(FILE_INPUT);


		$enum = 1;

		$ret_bit = 1;

		$set = 1;

		$s_int = 0;	

		$u_int = 0;

		$s_time = "00:00:00";

		$s_date = "1970-01-01";

		$int=0;

		$current_time = "838:59:59";

		$current_date = "1000-01-00";

		$current_year = 1901;

		$u_current_float = 1.175494351e-38;

		$s_current_float = -1.175494351e-38;

		srand();


	#Write data to file

		for ($count=0; $count < $ARGV[0]; $count++)

		{

			$u_int++;

#			$bit = &get_next_bit(64);

			$char = &get_next_char($count);

			$char_0 = &get_next_char_0($count);

			$char_55 = &get_next_char_55($count);

			$char_90 = &get_next_char_90($count);

			$char_100 = &get_next_char_100($count);

			$char_255 = &get_next_char_255($count);

			$varchar_500 = &get_next_varchar_500($count);

			$longtext = &get_next_longtext($count);

			$tinytext = &get_next_tinytext($count);

			$s_int = &get_next_s_int($count,$ARGV[0]);

			$decimal = $count. "\." .$count;

			$int = &get_next_int($int);

			$enum = &get_next_enum($enum);

			$set = &get_next_set($set);

			$current_date = &get_next_date($current_date);

			$current_time = &get_next_time($current_time);

			$current_year = &get_next_year($current_year);

			$u_current_float = &get_next_float($u_current_float);

			$s_current_float = &get_next_float($s_current_float);

			$s_time = &get_next_s_time($s_time);

			$s_date = &get_next_s_date($s_date);

			$datetime = &get_next_datetime($s_date,$s_time);

			$timestamp = &get_next_timestamp($s_date,$s_time);


			print ($u_int);			#numeric (0) unsigned not null

			print ("\t",$u_int);			#numeric (254) unsigned not null

			print ("\t",$u_int);			#numeric (0) zerofill not null

			print ("\t",$u_int);			#numeric (254) zerofill not null

			print ("\t",$u_int);			#numeric (0) unsigned zerofill not null

			print ("\t",$u_int);			#numeric (254) unsigned zerofill not null

			print ("\t",$s_int);			#numeric (0,0) not null

			print ("\t",$decimal);			#numeric (253,253) not null

			print ("\t",$u_int);			#numeric (0,0) unsigned not null

			print ("\t",$decimal);			#numeric (253,253) unsigned not null

			print ("\t",$u_int);			#numeric (0,0) zerofill not null

			print ("\t",$decimal);			#numeric (253,253) zerofill not null

			print ("\t",$u_int);			#numeric (0,0) unsigned zerofill not null

			print ("\t",$decimal);			#numeric (253,253) unsigned zerofill not null

			print ("\t",$s_current_float);		#real not null

			print ("\t",$u_current_float);		#real unsigned not null

			print ("\t",$u_current_float);		#real zerofill not null

			print ("\t",$u_current_float);		#real unsigned zerofill not null

			print ("\t",$s_current_float);		#double not null

			print ("\t",$u_current_float);		#double unsigned not null

			print ("\t",$u_current_float);		#double zerofill not null

			print ("\t",$u_current_float);		#double unsigned zerofill not null

			print ("\t",$s_current_float);		#float

			print ("\t",$u_current_float);		#float unsigned 

			print ("\t",$u_current_float);		#float zerofill 

			print ("\t",$u_current_float);		#float unsigned zerofill 

			print ("\t",$s_current_float);		#float(0) 

			print ("\t",$s_current_float);		#float(23) 

			print ("\t",$u_current_float);		#float(0) unsigned 

			print ("\t",$u_current_float);		#float(23) unsigned 

			print ("\t",$u_current_float);		#float(0) zerofill 

			print ("\t",$u_current_float);		#float(23) zerofill

			print ("\t",$u_current_float);		#float(0) unsigned zerofill 

			print ("\t",$u_current_float);		#float(23) unsigned zerofill

			print ("\t",$s_current_float);		#float(24) 

			print ("\t",$s_current_float);		#float(53) 

			print ("\t",$u_current_float);		#float(24) unsigned 

			print ("\t",$u_current_float);		#float(53) unsigned 

			print ("\t",$u_current_float);		#float(24) zerofill 

			print ("\t",$u_current_float);		#float(53) zerofill 

			print ("\t",$u_current_float);		#float(24) unsigned zerofill 			
							
			print ("\t",$u_current_float);		#float(53) unsigned zerofill 

			print ("\t",$current_date);		#date 

			print ("\t",$current_time);		#time 

			print ("\t",$datetime);			#datetime 

			print ("\t",$timestamp);		#timestamp 

			print ("\t",$current_year);		#year 

			print ("\t",$current_year);		#year(3) 

			print ("\t",$current_year);		#year(4) 

			print ("\t",$enum);			#enum("1enum","2enum") 

			print ("\t",$set);			#set("1set","2set")

			print ("\t",$char_90);			#char(95) unicode##

			print ("\t",$char_255);		#char(255) unicode##

			print ("\t",$char_100);		#char(130) binary##

			print ("\t",$varchar_500);		#varchar(25000) binary##

			print ("\t",$char_0);			#varbinary(0)##

			print ("\t",$varchar_500);		#varchar(1200) unicode##

#			print ("\t",$bit);			#Bit(64)##

			print ("\n");

		}		


		select ($oldhandle);

		close (FILE_INPUT);


	}


	else

	{

		&printusage;

	}


}



#Subroutines START HERE


sub get_next_geometry

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@',
	
'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0"@\0\0\0\0\0\0"@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0>@\0\0\0\0\0\0>@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0D@\0\0\0\0\0\0D@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0D@\0\0\0\0\0\0D@\0\0\0\0\0\0I@\0\0\0\0\0\0I@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0D@\0\0\0\0\0\0D@\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0N@\0\0\0\0\0\0N@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0V@\0\0\0\0\0V@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0[@\0\0\0\0\0[@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0[@\0\0\0\0\0[@\0\0\0\0\0\0^@\0\0\0\0\0\0^@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0i@\0\0\0\0\0\0i@\0\0\0\0\0�@\0\0\0\0\0�@\0\0\0\0\0\0y@\0\0\0\0\0\0y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�@\0\0\0\0\0\0Y@\0\0\0\0\0\0y@\0\0\0\0\0\0y@\0\0\0\0\0@@\0\0\0\0\0@@');


	return($ascii[$index]);

}



sub get_next_point

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0>@\0\0\0\0\0\0>@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0D@\0\0\0\0\0\0D@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0I@\0\0\0\0\0\0I@',

'\0\0\0\0\0\0\0\0\0\0\0\0\09@\0\0\0\0\0\0.@',

'\0\0\0\0\0\0\0\0\0\0\0\0A@\0\0\0\0\0\0.@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0\09@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0A@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0F@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0K@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0@P@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0�@',

'\0\0\0\0\0\0\0\0\0\0\0\0\09@\0\0\0\0\0\0.@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0@U@',

'\0\0\0\0\0\0\0\0\0\0\0\0K@\0\0\0\0\0�@',

'\0\0\0\0\0\0\0\0\0\0\0\0@P@\0\0\0\0\0�@',

'\0\0\0\0\0\0\0\0\0\0\0\0�@\0\0\0\0\0�@',

'\0\0\0\0\0\0\0\0\0\0\0\0@U@\0\0\0\0\0�@',

'\0\0\0\0\0\0\0\0\0\0\0\0�@\0\0\0\0\0�@');


	return($ascii[$index]);

}


sub get_next_linestring

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0>@\0\0\0\0\0\0>@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0D@\0\0\0\0\0\0D@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0D@\0\0\0\0\0\0D@\0\0\0\0\0\0I@\0\0\0\0\0\0I@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0D@\0\0\0\0\0\0D@\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0N@\0\0\0\0\0\0N@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0V@\0\0\0\0\0V@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0[@\0\0\0\0\0[@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0[@\0\0\0\0\0[@\0\0\0\0\0\0^@\0\0\0\0\0\0^@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0i@\0\0\0\0\0\0i@\0\0\0\0\0�@\0\0\0\0\0�@\0\0\0\0\0\0y@\0\0\0\0\0\0y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�@\0\0\0\0\0\0Y@\0\0\0\0\0\0y@\0\0\0\0\0\0y@\0\0\0\0\0@@\0\0\0\0\0@@');


	return($ascii[$index]);

}



sub get_next_polygon

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\04@\0\0\0\0\0\0\0@\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0\0@\0\0\0\0\0\0>@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0"@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0&@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0+@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0(@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0*@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\0,@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\0.@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\00@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\01@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\02@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\03@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\0,@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\04@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0\0\0\05@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\05@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\0\0\0\06@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\06@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0@@\0\0\0\0\0\02@\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\0�\0\0\0\0\0@@\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\0\0\0\07@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\07@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0@@\0\0\0\0\0\0�\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\03@\0\0\0\0\0@@\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\0\0\0\08@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\08@');


	return($ascii[$index]);

}


sub get_next_geometry_collection

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\04@\0\0\0\0\0\0\0@\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0\0@\0\0\0\0\0\0>@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0"@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0&@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0(@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0*@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\0,@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\0.@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\00@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\01@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\0,@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\02@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\03@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\04@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0\0\0\05@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\05@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\0\0\0\06@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\06@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0@@\0\0\0\0\0\02@\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\0�\0\0\0\0\0@@\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\0\0\0\07@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\07@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0@@\0\0\0\0\0\0�\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\03@\0\0\0\0\0@@\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\0\0\0\08@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\08@');


	return($ascii[$index]);

}


sub get_next_multipoint

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
	
'\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0*@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @',

'\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\04@',

'\0\0\0\0\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0>@\0\0\0\0\0\0>@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0D@\0\0\0\0\0\0D@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0I@\0\0\0\0\0\0I@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0N@\0\0\0\0\0\0N@',

'\0\0\0\0\0\0\0\0\0\0\0\0Q@\0\0\0\0\0Q@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0T@\0\0\0\0\0\0T@',

'\0\0\0\0\0\0\0\0\0\0\0\0V@\0\0\0\0\0V@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0i@\0\0\0\0\0\0i@',

'\0\0\0\0\0\0\0\0\0\0\0\0�@\0\0\0\0\0\0Y@');


	return($ascii[$index]);

}


sub get_next_multilinestring

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@',
	
'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0"@\0\0\0\0\0\0"@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0>@\0\0\0\0\0\0>@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0D@\0\0\0\0\0\0D@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0D@\0\0\0\0\0\0D@\0\0\0\0\0\0I@\0\0\0\0\0\0I@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0D@\0\0\0\0\0\0D@\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0N@\0\0\0\0\0\0N@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0V@\0\0\0\0\0V@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0[@\0\0\0\0\0[@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0[@\0\0\0\0\0[@\0\0\0\0\0\0^@\0\0\0\0\0\0^@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0i@\0\0\0\0\0\0i@\0\0\0\0\0�@\0\0\0\0\0�@\0\0\0\0\0\0y@\0\0\0\0\0\0y@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�@\0\0\0\0\0\0Y@\0\0\0\0\0\0y@\0\0\0\0\0\0y@\0\0\0\0\0@@\0\0\0\0\0@@');


	return($ascii[$index]);

}


sub get_next_multipolygon

{

	my($count) = @_;

	$index = $count % 6 ;


	@ascii = ('\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\04@\0\0\0\0\0\0�\0\0\0\0\0\0�\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0 @\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\04@\0\0\0\0\0\0\0@\0\0\0\0\0\0>@\0\0\0\0\0\0>@\0\0\0\0\0\0\0@\0\0\0\0\0\0>@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0"@\0\0\0\0\0\0@\0\0\0\0\0\0@',

'\N',


'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0I@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0&@\0\0\0\0\0\0"@\0\0\0\0\0\0"@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0N@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0(@\0\0\0\0\0\0$@\0\0\0\0\0\0$@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0Q@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0*@\0\0\0\0\0\0&@\0\0\0\0\0\0&@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0T@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0,@\0\0\0\0\0\0(@\0\0\0\0\0\0(@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0V@\0\0\0\0\0\0 @\0\0\0\0\0\0 @\0\0\0\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0.@\0\0\0\0\0\0*@\0\0\0\0\0\0*@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0Y@\0\0\0\0\0\0"@\0\0\0\0\0\0"@\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\00@\0\0\0\0\0\0,@\0\0\0\0\0\0,@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0i@\0\0\0\0\0\0$@\0\0\0\0\0\0$@\0\0\0\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\01@\0\0\0\0\0\0.@\0\0\0\0\0\0.@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0�@\0\0\0\0\0\0&@\0\0\0\0\0\0&@\0\0\0\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\02@\0\0\0\0\0\00@\0\0\0\0\0\00@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0y@\0\0\0\0\0\0(@\0\0\0\0\0\0(@\0\0\0\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\03@\0\0\0\0\0\01@\0\0\0\0\0\01@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0@@\0\0\0\0\0\0*@\0\0\0\0\0\0*@\0\0\0\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\04@\0\0\0\0\0\02@\0\0\0\0\0\02@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0@\0\0\0\0\0\0,@\0\0\0\0\0\0,@\0\0\0\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\05@\0\0\0\0\0\03@\0\0\0\0\0\03@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0�\0\0\0\0\0\0.@\0\0\0\0\0\0.@\0\0\0\0\0\0\0\0\04@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\06@\0\0\0\0\0\04@\0\0\0\0\0\04@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\0@\0\0\0\0\0\00@\0\0\0\0\0\00@\0\0\0\0\0\0\0\0\05@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\07@\0\0\0\0\0\05@\0\0\0\0\0\05@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0 @\0\0\0\0\0\01@\0\0\0\0\0\01@\0\0\0\0\0\0\0\0\06@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\08@\0\0\0\0\0\06@\0\0\0\0\0\06@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0@@\0\0\0\0\0\02@\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\0�\0\0\0\0\0@@\0\0\0\0\0\02@\0\0\0\0\0\02@\0\0\0\0\0\0\0\0\07@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\09@\0\0\0\0\0\07@\0\0\0\0\0\07@',

'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0@@\0\0\0\0\0\0�\0\0\0\0\0@@\0\0\0\0\0@@\0\0\0\0\0\03@\0\0\0\0\0@@\0\0\0\0\0\03@\0\0\0\0\0\03@\0\0\0\0\0\0\0\0\08@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\0:@\0\0\0\0\0\08@\0\0\0\0\0\08@');


	return($ascii[$index]);

}


sub get_next_int

{

	my($next_int) = @_;

	if (($next_int>= 0) && ($next_int<= 255))

	{

		$next_int++;

	}


	else

	{

		$next_int = 0;

	}

	return($next_int);

}



sub get_next_enum

{

	my($next_enum) = @_;

	if($next_enum == 1)

	{

		$next_enum = 2;

	}

	else

	{

 		$next_enum = 1;

	}

	

	return($next_enum);

}


sub get_next_set

{

	my($next_set) = @_;

	if($next_set == 3)

	{

		$next_set = 1;

	}

	else

	{

 		$next_set++;

	}

	

	return($next_set);

}


sub get_next_bit

{
	my($col_size) = @_;
	
	my($max_size) = $col_size / 8;
	
#	my($sp_char) = "\x01";
	
	my($max_no) = 9 x $max_size;

	if ($ret_bit < $max_no)
	{
	
		$ret_bit++;
	
	}
	
	else
	{
	
		$ret_bit = 1;
	
	}

	return($ret_bit);

}


sub get_next_char

{
	my($count) = @_;

	my($index) = $count % 72;

	@ascii = ('!','@','#','$','%','^','&','*','(',')','_','+','=','-','|','{','}','[',']',

'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',

'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u',

'v','w','x','y','z');


	return($ascii[$index]);
}



sub get_next_char_0

{
	my($ret_string) = '';

	return($ret_string);
}


sub get_next_char_55

{
	my($count) = @_;

	my($local_count) = $count % 15;

	my($ret_string) = '';

	my($index) = $count % 72;

	@ascii = ('!','@','#','$','%','^','&','*','(',')','_','+','=','-','|','{','}','[',']',

'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',

'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u',

'v','w','x','y','z');

	for($loop_count=0;$loop_count <= $local_count; $loop_count++)
	
	{
		
		$ret_string = $ret_string.$ascii[$index];

		$index = ($index + 1) % 72;

	}
	
	$ret_string = $count.$ret_string;

	return($ret_string);

}


sub get_next_char_90

{
	my($count) = @_;

	my($ret_string) = '';

	my($local_count) = $count % 25;

	my($index) = $count % 72;

	@ascii = ('!','@','#','$','%','^','&','*','(',')','_','+','=','-','|','{','}','[',']',

'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',

'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u',

'v','w','x','y','z');


	for($loop_count=0; $loop_count <= $local_count; $loop_count++)
	
	{
		
		$ret_string = $ret_string.$ascii[$index];

		$index = ($index + 1) % 72;
	}

	$ret_string = $count.$ret_string;
	
	return($ret_string);

}


sub get_next_char_100

{
	my($count) = @_;

	my($local_count) = $count % 50;

	my($ret_string) = '';

	my($index) = $count % 72;

	@ascii = ('!','@','#','$','%','^','&','*','(',')','_','+','=','-','|','{','}','[',']',

'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',

'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u',

'v','w','x','y','z');

	for($loop_count=0; $loop_count <= $local_count; $loop_count++)
	
	{
		
		$ret_string = $ret_string.$ascii[$index];

		$index = ($index + 1) % 72;
	}

	$ret_string = $count.$ret_string;

	return($ret_string);

}



sub get_next_char_255

{
	my($count) = @_;

	my($local_count) = $count % 125;

	my($ret_string) = '';

	my($index) = $count % 72;

	@ascii = ('!','@','#','$','%','^','&','*','(',')','_','+','=','-','|','{','}','[',']',

'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',

'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',

'w','x','y','z');

	for($loop_count=0; $loop_count<$local_count; $loop_count++)
	
	{
		
		$ret_string = $ret_string.$ascii[$index];

		$index = ($index + 1) % 72;
	}

	$ret_string = $count.$ret_string;
	
	return($ret_string);

}


sub get_next_varchar_500
{
	my($count) = @_;

	my($ret_string) = '';

	my($local_count) = $count % 100;

	my($index) = $count % 72;

	@ascii = ('!','@','#','$','%','^','&','*','(',')','_','+','=','-','|','{','}','[',']',

'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',

'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u',

'v','w','x','y','z');

	for($loop_count=0; $loop_count<$local_count; $loop_count++)
	
	{
		
		$ret_string = $ret_string.$ascii[$index];

		$index = ($index + 1) % 72;
	}

	$ret_string = $count.$ret_string;
	
	return($ret_string);

}



sub get_next_float

{

	my($next_float) = @_;

	$next_float = $next_float + 1e-47;

	return($next_float);

}


	

sub get_next_tinytext

{

	my($count) = @_;

	my($default_char) = "a";

	$index = $count % 20 ;


	@ascii = ("\140","\141","\142","\143","\144","\145","\146","\147","\136","\137","\150",

			  "\151","\152","\153","\154","\155","\156","\157","\134","\135");


	my($quotient) = $count / 100;

	$quotient =~ s/\.[0-9]*// ;

	$tinytext = $default_char.$ascii[$index].$quotient;


	return($tinytext);

}



sub get_next_longtext

{

	my($count) = @_;

	my($default_char) = "a";

	$index = $count % 20 ;

	$multp = $count % 100;

	@ascii = ("\140","\141","\142","\143","\144","\145","\146","\147","\136","\137","\150",

			  "\151","\152","\153","\154","\155","\156","\157","\134","\135");


	$longtext = $count.$ascii[$index].$default_char x $multp;

	return($longtext);

}



sub get_next_year

{

	my($next_year) = @_;

	if (($next_year >= 1901) && ($next_year < 2155))

	{

		$next_year++;

	}


	else

	{

		$next_year = 1901;

	}

	return($next_year);

}



sub get_next_datetime

{

	my($date,$time) = @_;

	$datetime = $date." ".$time;

	return($datetime);

}



sub get_next_s_int

{

	my($count,$maxsize) = @_;

	$s_int = int($count - ($maxsize/2));

	return($s_int);

}


	

sub get_next_timestamp

{

	my($date,$time) = @_;

	my($pattern1) = "-";

	my($pattern2) = ":";

	@split_date = split(/$pattern1/,$date);

	@split_time = split(/$pattern2/,$time);

	$timestamp = "$split_date[0]"."$split_date[1]"."$split_date[2]"."$split_time[0]"."$split_time[1]"."$split_time[2]";

	return($timestamp);
}



sub get_next_time

{

	my($time) = @_;

	$pattern = ":";

	@words = split(/$pattern/, $time);

	

	$hour = $words[0];

	$min = $words[1];

	$sec = $words[2];

	

#Boundary condition


	if(($hour == -838)&&($min == 0)&&($sec == 0))

	{

		$hour = 838;

		$min = 59;

		$sec = 59;

	}


	if($min > 0)

	{

		if($sec > 0)

		{

			$sec-- ;

		}

		else

		{

			$sec = 59;

			$min-- ;

		}

	}

	

	else

	{

		$hour-- ;

		$min = 59;

		$sec = 59;

	}


	if($hour !~ /[0-9][0-9]/)

	{

		$hour = "0".$hour;

	}


	if($min !~ /[0-9][0-9]/)

	{

		$min = "0".$min;

	}

	

	if($sec !~ /[0-9][0-9]/)

	{

		$sec = "0".$sec;

	}	


	$time = "$hour".":"."$min".":"."$sec";

	return($time);

}



sub get_next_date

{

	my($date) = @_ ;

	$pattern = '-';

	@words = split(/$pattern/, $date);


	$year = $words[0];

	$month = $words[1];

	$day = $words[2];


	if (($month == 2) )

	{

		if($day == 28)	

		{

			if($year =~ /[0-9][0-9]00/)

			{

				if($year % 400 == 0)

				{

					$day++;

				}

				else

				{

					$day = 01;

					$month++;

				}

			}

				

			else

			{

				if($year % 4 == 0)

				{

					$day++;

				}	

				else

				{

					$day = 01;

					$month++;

				}

			}

		}


		elsif($day == 29)

		{

			$day = 01;

			$month++;

		}

		

		else

		{	

			$day++;

		}

	}


	elsif($day == 30)

	{

		if (($month == 1) || ($month == 3) || ($month == 5) || ($month == 7) || ($month == 8) || ($month == 10) || ($month == 12))

		{

			$day++;

		}

		else

		{

			$day = 01;

			$month++;

		}

	}


	elsif($day == 31)

	{

		if ($month == 12)

		{

			$day = 01;

			$month = 01;

			

			if($year < 9999)

			{

				$year++;

			}

			else

			{

				$year = 1000;

			}

		}


		else

		{

			$day = 01;

			$month++;

		}

	}

	

	else

	{

		$day++;

	}


	if($month !~ /[0-9][0-9]/)

	{

		$month = "0".$month;

	}

	

	if($day !~ /[0-9][0-9]/)

	{

		$day = "0".$day;

	}	

	

	$date = "$year"."-"."$month"."-"."$day";	

	

	return($date);

}




sub get_next_s_date

{

	my($date) = @_ ;

	$pattern = '-';

	@words = split(/$pattern/, $date);


	$year = "$words[0]";

	$month = "$words[1]";

	$day = "$words[2]";


	if (($month == "02") )

	{

		if($day == "28")	

		{

			if($year =~ /[0-9][0-9]00/)

			{

				if($year % 400 == 0)

				{

					$day++;

				}

				else

				{

					$day = 01;

					$month++;

				}

			}

				

			else

			{

				if($year % 4 == 0)

				{

					$day++;

				}	

				else

				{

					$day = 01;

					$month++;

				}

			}		

		}



		elsif($day == "29")

		{

			$day = "01";

			$month++;

		}

		

		else

		{	

			$day++;

		}

	}


	elsif($day == "30")

	{

		if (($month == "01") || ($month == "03") || ($month == "05") || ($month == "07") || ($month == "08") || ($month == "10") || ($month == "12"))

		{

			$day++;

		}

		else

		{

			$day = "01";

			$month++;

		}

	}


	elsif($day == "31")

	{

		if ($month == "12")

		{

			$day = "01";

			$month = "01";

			

			if($year < "2036")

			{

				$year = $year + "0001";

			}

			else

			{

				$year = "1970";

			}

		}


		else

		{

			$day = "01";

			$month++;

		}

	}

	

	else

	{

		$day++;

	}


	if($month !~ /[0-9][0-9]/)

	{

		$month = "0".$month;

	}

	

	if($day !~ /[0-9][0-9]/)

	{

		$day = "0".$day;

	}	

	

	$date = "$year"."-"."$month"."-"."$day";	

	

	return($date);

}




sub get_next_s_time

{

	my($time) = @_;

	$pattern = ":";

	@words = split(/$pattern/, $time);

	

	$hour = "$words[0]";

	$min = "$words[1]";

	$sec = "$words[2]";

	

	#Boundary condition


	if(($hour == "23")&&($min == "59")&&($sec == "59"))

	{

		$hour = "00";

		$min = "00";

		$sec = "00";

	}


	if($min < "59")

	{

		if($sec < "59")

		{

			$sec = $sec + "01";

		}

		else

		{

			$sec = "00";

			$min = $min + "01" ;

		}

	}

	

	else

	{

		$hour = $hour + "01" ;

		$min = "00";

		$sec = "00";

	}


	if($hour !~ /[0-9][0-9]/)

	{

		$hour = "0".$hour;

	}


	if($min !~ /[0-9][0-9]/)

	{

		$min = "0".$min;

	}

	

	if($sec !~ /[0-9][0-9]/)

	{

		$sec = "0".$sec;

	}	


	$time = "$hour".":"."$min".":"."$sec";

	

	return($time);

}




sub printusage

{

	print "\n\nUsage: Perl DataGen.pl <Rowcount> <Storage Engine>";

	print "\n\n<Rowcount>: The number of rows in the table " .

	"\n<Storage Engine>: The storage engine parameter" .

	" will be of the following types:" .

	"\n\t\t  InnoDB | MyIsam | Memory" .

	"\n\nE.g. Perl DataGen.pl 5000 InnoDB" .

	"\n\nThis will generate 4 text files containing 5000" .

	" records each for the 4 types of InnoDB tables.\n\n";

}



sub random

{

	my($limit) = @_ ;

	$random = int(rand($limit));

	return($random);

}

