/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include "ftdefs.h"

static void get_options(int argc,char *argv[]);
static void usage(char *argv[]);
static void complain(int val);

static int count=0, stats=0, dump=0, verbose=0;
static char *query=NULL;

#define MAX (HA_FT_MAXLEN+10)
#define HOW_OFTEN_TO_WRITE 1000

int main(int argc,char *argv[])
{
  int error=0;
  uint keylen, inx, doc_cnt=0;
  float weight;
  double gws, min_gws=0, avg_gws=0;
  MI_INFO *info;
  char buf[MAX], buf2[MAX], buf_maxlen[MAX], buf_min_gws[MAX];
  ulong total=0, maxlen=0, uniq=0, max_doc_cnt=0;
#ifdef EVAL_RUN
  uint	       cnt;
  double       sum, sum2, suml;
#endif /* EVAL_RUN */
  struct { MI_INFO *info; } aio0, *aio=&aio0; /* for GWS_IN_USE */

  MY_INIT(argv[0]);
  get_options(argc,argv);
  if (count || dump)
    verbose=0;
  else
    stats=1;

  if (verbose)
    setbuf(stdout,NULL);

  if (argc-optind < 2)
    usage(argv);

  if (!(info=mi_open(argv[optind],2,HA_OPEN_ABORT_IF_LOCKED)))
    goto err;

  inx=atoi(argv[optind+1]);
  *buf2=0;
  aio->info=info;

  if ((inx >= info->s->base.keys) || !(info->s->keyinfo[inx].flag & HA_FULLTEXT))
  {
    printf("Key %d in table %s is not a FULLTEXT key\n", inx, info->filename);
    goto err;
  }

  if (query)
  {
    FT_DOCLIST *result;
    int i;

    ft_init_stopwords(ft_precompiled_stopwords);

    result=ft_init_search(info,inx,query,strlen(query),1);
    if(!result)
      goto err;

    if (verbose)
      printf("%d rows matched\n",result->ndocs);

    for(i=0 ; i<result->ndocs ; i++)
      printf("%9qx %20.7f\n",result->doc[i].dpos,result->doc[i].weight);

    ft_close_search(result);
  }
  else
  {
    info->lastpos= HA_OFFSET_ERROR;
    info->update|= HA_STATE_PREV_FOUND;

    while (!(error=mi_rnext(info,NULL,inx)))
    {
      keylen=*(info->lastkey);

#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
#ifdef EVAL_RUN
      mi_float4get(weight,info->lastkey+keylen+2);
#else /* EVAL_RUN */
      mi_float4get(weight,info->lastkey+keylen+1);
#endif /* EVAL_RUN */
#else
#error
#endif

#ifdef EVAL_RUN
      cnt=*(byte *)(info->lastkey+keylen);
#endif /* EVAL_RUN */

      snprintf(buf,MAX,"%.*s",(int) keylen,info->lastkey+1);
      casedn_str(buf);
      total++;

      if (count || stats)
      {
        doc_cnt++;
#ifdef EVAL_RUN
        sum +=cnt;
        sum2+=cnt*cnt;
        suml+=cnt*log(cnt);
#endif /* EVAL_RUN */
        if (strcmp(buf, buf2))
        {
          if (*buf2)
          {
            uniq++;
            avg_gws+=gws=GWS_IN_USE;
            if (count)
              printf("%9u %20.7f %s\n",doc_cnt,gws,buf2);
            if (maxlen<keylen)
            {
              maxlen=keylen;
              strcpy(buf_maxlen, buf2);
            }
            if (max_doc_cnt < doc_cnt)
            {
              max_doc_cnt=doc_cnt;
              strcpy(buf_min_gws, buf2);
              min_gws=gws;
            }
          }
          strcpy(buf2, buf);
#ifdef EVAL_RUN
          sum=sum2=suml=
#endif /* EVAL_RUN */
          doc_cnt=0;
        }
      }
      if (dump)
        printf("%9qx %20.7f %s\n",info->lastpos,weight,buf);

      if(verbose && (total%HOW_OFTEN_TO_WRITE)==0)
        printf("%10ld\r",total);
    }

    if (stats)
      printf("Total rows: %qu\nTotal words: %lu\n"
             "Unique words: %lu\nLongest word: %lu chars (%s)\n"
             "Average global weight: %f\n"
             "Most common word: %lu times, weight: %f (%s)\n",
             (ulonglong)info->state->records, total, uniq, maxlen, buf_maxlen,
             avg_gws/uniq, max_doc_cnt, min_gws, buf_min_gws);
  }

err:
  if (error && error != HA_ERR_END_OF_FILE)
    printf("got error %d\n",my_errno);
  if (info)
    mi_close(info);
  return 0;
}

const char *options="dscve:h";

static void get_options(int argc, char *argv[])
{
  int c;

  while ((c=getopt(argc,argv,options)) != -1)
  {
    switch(c) {
    case 'd': dump=1; complain(count || query); break;
    case 's': stats=1; complain(query!=0); break;
    case 'v': verbose=1; break;
    case 'c': count=1; complain(dump || query); break;
    case 'e': query=my_strdup(optarg,MYF(MY_FAE)); complain(dump || count || stats); break;
    case '?':
    case 'h':
    default:
      usage(argv);
    }
  }
  return;
} /* get options */

static void usage(char *argv[])
{
  printf("Use: %s [-%s] <table_name> <key_no>\n", *argv, options);
  exit(1);
}

static void complain(int val) /* Kinda assert :-)  */
{
  if (val)
  {
    printf("You cannot use these options together!\n");
    exit(1);
  }
}

