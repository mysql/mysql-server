void outputbyte(unsigned char ch, bool plaintext)
{
   if (plaintext) {
      if (ch != '\n' && isprint(ch)) {
         switch (ch) {
            case ('\\'): {
               printf("\\\\");
               break;
            }
            default:
            {
               printf("%c", ch);
               break;
            }
         }
      }
      else {
         printf(
            "\\%c%c",
            "0123456789abcdef"[(ch & 0xf0) >> 4],
            "0123456789abcdef"[ch & 0x0f]
         );
      }
   }
   else {
      printf(
         "%c%c",
         "0123456789abcdef"[(ch & 0xf0) >> 4],
         "0123456789abcdef"[ch & 0x0f]
      );
   }
}
/*
The above code is too long, since printf isn't being used effectively.
*/
void outputbyte(unsigned char ch, bool plaintext)
{
   if (plaintext) {
      if (ch != '\n' && isprint(ch)) {
         switch (ch) {
            case ('\\'): {
               printf("\\\\");
               break;
            }
            default:
            {
               printf("%c", ch);
               break;
            }
         }
      }
      else {
	  printf("%02x", ch);
      }
   }
   else {
       printf("%02x", ch);
   }
}
/*
I don't understand the control flow for the plaintext case, since \n isn't a printable character.
Also using a switch for a two-way branch seems overkill.
Switch is quite dangerous because one can forget the break, and control falls through.
For one-liner conditionsals, there is no need to put in { }
*/
void outputbyte(unsigned char ch, bool plaintext)
{
   if (plaintext) {
       if (ch == '\\') printf("\\\\");
       else if (isprint(ch)) printf("%c", ch);
       else printf("%02x", ch);
   } else {
       printf("%02x", ch);
   }
}
