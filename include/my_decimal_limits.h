#ifndef my_decimal_limits_h
#define my_decimal_limits_h

#define DECIMAL_LONGLONG_DIGITS 22
#define DECIMAL_LONG_DIGITS 10
#define DECIMAL_LONG3_DIGITS 8

/** maximum length of buffer in our big digits (uint32). */
#define DECIMAL_BUFF_LENGTH 9

/* the number of digits that my_decimal can possibly contain */
#define DECIMAL_MAX_POSSIBLE_PRECISION (DECIMAL_BUFF_LENGTH * 9)


/**
  maximum guaranteed precision of number in decimal digits (number of our
  digits * number of decimal digits in one our big digit - number of decimal
  digits in one our big digit decreased by 1 (because we always put decimal
  point on the border of our big digits))
*/
#define DECIMAL_MAX_PRECISION (DECIMAL_MAX_POSSIBLE_PRECISION - 8*2)
#define DECIMAL_MAX_SCALE 30
#define DECIMAL_NOT_SPECIFIED 31

/**
  maximum length of string representation (number of maximum decimal
  digits + 1 position for sign + 1 position for decimal point)
*/
#define DECIMAL_MAX_STR_LENGTH (DECIMAL_MAX_POSSIBLE_PRECISION + 2)

#endif
