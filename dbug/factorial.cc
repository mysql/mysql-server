#ifdef DBUG_OFF /* We are testing dbug */

int factorial(int value) {
  if (value > 1) {
    value *= factorial(value - 1);
  }
  return value;
}

#else

int factorial(int value) {
  DBUG_ENTER("factorial");
  DBUG_PRINT("find", ("find %d factorial", value));
  if (value > 1) {
    value *= factorial(value - 1);
  }
  DBUG_PRINT("result", ("result is %d", value));
  DBUG_RETURN(value);
}

#endif
