// includes

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

// explanations

// this program has a vi like hierarchy
// first terminal -> opens in raw mode -> chnages to editor mode

// canonical mode -> input goes after pressing enter
// raw mode -> program processes input as soon as it is typed

// read() returns the number of bytes read

// macros
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

typedef struct
{
	int size;
	char* chars;
} erow;

enum editorKey
{
	ARROW_UP = 1000, // 1000 so no ascii characters are mistakenly printed
	ARROW_DOWN,
	ARROW_LEFT,
	ARROW_RIGHT,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

// data
struct editorConfig
{
	int cursor_col, cursor_row;
	int screenrows;
	int screencols;
	int numrows;
	erow row;
	struct termios orig_termios; // this is the original struct of termios
};

struct editorConfig E;

// terminal

// exits the editor with a slightly descripting error message
void die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4); // clears tyhe screen
	write(STDOUT_FILENO, "\x1b[H", 3);	// sets the cursor to the home position
	perror(s);							// descripting error message
	exit(1);							// exits
}

// disables the non editor or raw mode
// goes into canonical mode
void disableRawMode()
{
	// while checking the condition it executes the operation needed
	// changes the terminal attributes using orig_termios properties using termios header functions
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}

// enables the non editor or raw mode
void enableRawMode()
{

	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
		die("tcgetattr");

	atexit(disableRawMode); // calls disableRawmode fn when the program exits normally

	// makes an instance of termios struct and assigns it the properties of orig_termios struct
	struct termios raw = E.orig_termios;

	// It’s turning off the BRKINT, ICRNL, INPCK, ISTRIP, and IXON flags.
	// This means the input bytes are not being translated or ignored,
	// and software flow control is disabled.
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

	// It’s turning off the OPOST flag, which disables output processing.
	raw.c_oflag &= ~(OPOST);

	// It’s turning on the CS8 flag, which sets the character size to 8 bits.
	raw.c_cflag |= (CS8);

	// It’s turning off the ECHO, ICANON, IEXTEN, and ISIG flags.
	// This means that echo is disabled,
	// canonical mode is disabled (allowing read to be satisfied immediately),
	// extended input processing is disabled, and signal characters are disabled
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	// This line sets the minimum number of characters for non-canonical read to 0.
	raw.c_cc[VMIN] = 0;
	// This line sets the timeout for non-canonical read in deciseconds to 1.
	raw.c_cc[VTIME] = 1;

	// changes terminal settings to raw settings
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

int editorReadKey()
{
	int nread;
	char c;
	// reads input and stores it in char c
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN)
			die("read");
	}

	// if c == "escape character"
	if (c == '\x1b')
	{
		char seq[3]; // make a char array of len 3 for any more escape sequences

		// reads input
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return '\x1b';

		if (seq[0] == '[')
		{
			if (seq[1] >= '0' && seq[1] <= 9)
			{
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';

				if (seq[2] == '~')
				{
					switch (seq[2])
					{
					case '1':
						return HOME_KEY; // (\x1b[1~)
					case '4':
						return END_KEY; // (\x1b[4~)
					case '3':
						return DEL_KEY; // (\x1b[4~)
					case '5':
						return PAGE_UP; // (\x1b[5~)
					case '6':
						return PAGE_DOWN; // (\x1b[6~)
					case '7':
						return HOME_KEY; // (\x1b[7~)
					case '8':
						return END_KEY; // (\x1b[8~)
					}
				}
			}
			else
			{
				switch (seq[1])
				{
				case 'A':
					return ARROW_UP; // (\x1b[A)
				case 'B':
					return ARROW_DOWN; // (\x1b[B)
				case 'C':
					return ARROW_RIGHT; // (\x1b[C)
				case 'D':
					return ARROW_LEFT; // (\x1b[D)
				case 'H':
					return HOME_KEY; // (\x1b[H)
				case 'F':
					return END_KEY; // (\x1b[F)
				}
			}
		}
		else if (seq[0] == 'O')
		{
			switch (seq[1])
			{
			case 'H':
				return HOME_KEY; // (\x1bOH)
			case 'F':
				return END_KEY; // (\x1bOF)
			}
		}

		return '\x1b';
	}
	// if there is no escape character then return c
	else
		return c;
}

int getCursorPosition(int *rows, int *cols)
{
	char buf[32]; // to store the response from the termina about cursor position
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) // prompts the terminal to get cursor position
		return -1;

	for (; i < sizeof(buf) - 1; i++)
	{
		if (read(STDIN_FILENO, &buf[i], 1) != 1) // takes input from std input
			break;
		if (buf[i] == 'R') // breaks loop if char = R
			break;
	}

	// now the itrator is at the last index hence it assigns the null character 
	// at the end to depict it is a string
	buf[i] = '\0'; 

	if (buf[0] != '\x1b' || buf[1] != '[') // checks for escape sequences
		return -1;

	// it parses the two integers separated by ; into rows and cosl
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) 
		return -1;

	return 0;
}

int getWindowSize(int *rows, int *cols)
{
	struct winsize ws; // using winsize struct

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B]", 12) != 12)
			return -1;
		return getCursorPosition(rows, cols);
	}
	else
	{
		*rows = ws.ws_row;
		*cols = ws.ws_col;
	}
	return 0;
}

// file i/o

void editorOpen(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) die("fopen");

	char *line;
	size_t linecap = 0;
	ssize_t linelen;

	linelen = getline(&line, &linecap, fp);

	if (linelen != -1)
	{
		while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
		{
			linelen--;
		}

		E.row.size = linelen;
		E.row.chars = malloc(linelen + 1);
		memcpy(E.row.chars, line, linelen);
		E.row.chars[linelen] = '\0';
		E.numrows = 1;
	}
	free(line);
	fclose(fp);
}

// append buffer
struct abuf
{
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL)
		return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab)
{
	free(ab->b);
}

// output

void editorDrawRows(struct abuf *ab)
{
	int y;

	for (y = 0; y < E.screenrows; y++)
	{
		if (y >= E.numrows)
		{

		if (E.numrows == 0 && y == E.screenrows / 3) 
		{
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),
									  "Kilo editor --version %s", KILO_VERSION);

			if (welcomelen > E.screencols)
				welcomelen = E.screencols;

			int padding = (E.screencols - welcomelen) / 2;
			if (padding)
			{
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--)
			{
				abAppend(ab, " ", 1);
			}

			abAppend(ab, welcome, welcomelen);
		}
		else
		{
			abAppend(ab, "~", 1);
		}
		}
		else
		{
			int len = E.row.size;
			if (len > E.screencols) len = E.screencols;
			abAppend(ab, E.row.chars, len);
		}

		abAppend(ab, "\x1b[K", 3);

		if (y < E.screenrows - 1)
		{
			abAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen()
{
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[K", 3);
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_row + 1, E.cursor_col + 1);
	// if error occurs check here below (sizeof -> strlen)
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

// input

void editorMoveCursor(int key)
{
	switch (key)
	{
	case ARROW_LEFT:
		if (E.cursor_col != 0)
			E.cursor_col--;
		break;
	case ARROW_UP:
		if (E.cursor_row != 0)
			E.cursor_row--;
		break;
	case ARROW_DOWN:
		if (E.cursor_row != E.screenrows - 1)
			E.cursor_row++;
		break;
	case ARROW_RIGHT:
		if (E.cursor_col != E.screencols - 1)
			E.cursor_col++;
		break;
	}
}

void editorProcessKeypress()
{
	int c = editorReadKey();

	switch (c)
	{
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		exit(0);
		break;

	case PAGE_DOWN:
	case PAGE_UP:
	{
		int times = E.screenrows;
		while (times--)
		{
			editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
		}
	}
	break;

	case HOME_KEY:
		E.cursor_col = 0;
		break;
	case END_KEY:
		E.cursor_col = E.screencols - 1;
		break;

	case ARROW_UP:
	case ARROW_LEFT:
	case ARROW_RIGHT:
	case ARROW_DOWN:
		editorMoveCursor(c);
		break;
	}
}

// init

void initEditor()
{
	E.cursor_col = 0;
	E.cursor_row = 0;
	E.numrows = 0;
	if (getWindowSize(&E.screenrows, &E.screencols) == -1)
	{
		die("getWIndowSize");
	}
}

int main(int argc, char *argv[])
{
	enableRawMode();
	initEditor();
	if (argc >= 2) editorOpen(argv[1]);
	while (1)
	{
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
