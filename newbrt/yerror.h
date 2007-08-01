enum pma_errors { BRT_OK=0, BRT_ALREADY_THERE = -2, BRT_KEYEMPTY=-3 };

enum typ_tag { TYP_BRTNODE = 0xdead0001,
	       TYP_CACHETABLE, TYP_PAIR, /* for cachetables */
	       TYP_PMA };
