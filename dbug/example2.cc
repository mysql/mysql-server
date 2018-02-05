int debug = 0;

main(argc, argv) int argc;
char *argv[];
{
  /* printf ("argv = %x\n", argv) */
  if (debug) printf("argv[0] = %d\n", argv[0]);
    /*
     *	Rest of program
     */
#ifdef DEBUG
  printf("== done ==\n");
#endif
}
