#ifndef YERROR_H
#define YERROR_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

enum pma_errors { BRT_OK=0, BRT_ALREADY_THERE = -2, BRT_KEYEMPTY=-3 };

enum typ_tag { TYP_BRTNODE = 0xdead0001,
	       TYP_CACHETABLE, TYP_PAIR, /* for cachetables */
	       TYP_PMA,
               TYP_TOKULOGGER,
	       TYP_TOKUTXN
};
#endif
