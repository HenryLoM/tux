// main.c
//    basic c app launcher with fzf

// usefull info
//      activate   alternate screen with "\x1b[?1049h"
//      deactivate alternate screen with "\x1b[?1049l"

#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*
 *
 * Global Vars
 *
 */

static struct termios orig;

// State vars
int selected = 0;
int ui_change = 1;

// size vars
int termRows = 0;
int termCols = 0;

/*
 *
 * system functions
 *
 */

void actAltScr() {
  printf("\x1b[?1049h");
  printf("\x1b[2J");
  printf("\x1b[H");
  fflush(stdout);
}

void deactAltScr() { printf("\x1b[?1049l"); }

void deactRaw() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig); }

void actRaw() {
  struct termios raw;

  // get state of terminal and save it in 'orig'
  tcgetattr(STDIN_FILENO, &orig);
  atexit(deactRaw); // define function to be called on exit

  raw = orig;

  raw.c_lflag &=
      ~(ICANON | ECHO | ISIG); // disabling ICANON, ECHO and ISIG (linebuffer,
                               // auto-out, ctrl-c and co.)
  raw.c_iflag &= ~(IXON |      // diabling ctrl-s/q
                   ICRNL);     //  enabling normal return for return key

  raw.c_oflag &= ~(OPOST); // no auto output

  raw.c_cc[VMIN] = 1; // 1 byte to compute input
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void uiPrint(char *str, ...) {
  ui_change = 1;

  va_list ap;
  va_start(ap, str);
  vprintf(str, ap);
  va_end(ap);
}

// get window height
int getTermRows() {
  struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  return ws.ws_row;
}

// get window width
int getTermCols() {
  struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  return ws.ws_col; // return window width
}

/*
 *
 * ui components
 *
 */

int sizeChanged() {
  int cols = getTermCols();
  int rows = getTermRows();
  if (termCols == cols && termRows == rows)
    return 0;
  termCols = cols;
  termRows = rows;
  return 1;
}

void clearResUi() {
  for (int i = termRows - 3; i > 0; i--)
    printf("\x1b[%d;1H\x1b[2K", i);
}

void basicFrame() {
  printf("\x1b[%d;1H╭", termRows - 2);
  for (int i = 0; i < termCols - 2; i++)
    printf("─");
  printf("╮");
  printf("\x1b[%d;%dH\x1b[2K│ search...", termRows - 1, 1);
  printf("\x1b[%d;%dH│", termRows - 1, termCols);
  printf("\x1b[%d;1H╰", termRows);
  for (int i = 2; i < termCols; i++)
    printf("─");
  printf("╯");
  clearResUi();
  ui_change = 1;
}

void getGUIApps(char *d, char *n) {
  char path[512]; // string to store the path of .desktop file
  snprintf(path, sizeof(path), "%s/%s", d, n); // append filename to global path
  FILE *f = fopen(path, "r");                  // read file in path
  if (!f)
    return;         // error handling if file not readable
  char line[512];   // string for each line of the file
  int in_entry = 0; // bool to check if under [Desktop Entry]
  char nval[256];   // string to store name value
  char exec[256];   // string to store execution command of .desktop file
  int isTerm = 1;   // bool to log if app uses terminal

  // reads each line of file
  while (fgets(line, sizeof(line), f)) {
    // checks if in right sub sector and logs it in in_entry
    if (line[0] == '[') {
      in_entry = strstr(line, "Desktop Entry") != NULL;
      continue;
    }
    // skips rest of code if not in correct sub sector
    if (!in_entry)
      continue;

    char key[20];    // string to store key
    char value[256]; // string to store value
    // for each line -> gets key and value and stores name and exec values in
    // vars
    if (sscanf(line, "%19[^=]=%255[^\n]", key, value) == 2) {
      if (strcmp(key, "Name") == 0)
        strcpy(nval, value);
      if (strcmp(key, "Terminal") == 0)
        if (!(strcmp(value, "true") == 0))
          isTerm = 0;
      if (strcmp(key, "Exec"))
        strcpy(exec, value);
    }
  }
  fclose(f);
}

void search() {
  char *path = "/usr/share/applications/";
  DIR *dir;
  struct dirent *ent;

  char *token = strtok(path, ":");
  while (token != 0) {
    if ((dir = opendir(token)) != NULL) {
      while ((ent = readdir(dir)) != NULL) {
        getGUIApps(token, ent->d_name);
      }
    }
    token = strtok(NULL, ":");
  }
}

void onStartUp() {
  actRaw();
  actAltScr();

  termRows = getTermRows();
  termCols = getTermCols();
  basicFrame();
  // deactivate blocking behavior of read()
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  search();
}

void app() {
  onStartUp();

  for (;;) {
    char c;
    read(0, &c, 1);

    if (c == 'q') {
      deactAltScr();
      deactRaw();

      break;
    }

    if (sizeChanged()) {
      basicFrame();
    }

    if (ui_change) {
      fflush(stdout);
      ui_change = 0;
    }
  }
}
int main() { app(); }
