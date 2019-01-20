#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/termios.h>

#include "input-buffering.h"

/* Struct for input buffering */
struct termios original_tio;

void disable_input_buffering() {

  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal) {
  restore_input_buffering();
  printf("\n");
  exit(-2);
}