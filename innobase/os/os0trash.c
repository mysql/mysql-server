/* Stores the old console mode when echo is turned off */
ulint	os_old_console_mode;

/********************************************************************
Turns off echo from console input. */

void
os_console_echo_off(void)
/*=====================*/
{
	GetConsoleMode(stdio, &os_old_console_mode);
	SetConsoleMode(stdio, ENABLE_PROCESSED_INPUT);
}

/********************************************************************
Turns on echo in console input. */

void
os_console_echo_on(void)
/*====================*/
{
	SetConsoleMode(stdio, &os_old_console_mode);
}

/********************************************************************
Reads a character from the console. */

char
os_read_console(void)
/*=================*/
{
	char	input_char;
	ulint	n_chars;

	n_chars = 0;

	while (n_chars == 0) {
		ReadConsole(stdio, &input_char, 1, &n_chars, NULL);
	}

	return(input_char);
}

