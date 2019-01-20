#ifndef _INPUTBUFFERING
#define _INPUTBUFFERING

void disable_input_buffering();
void restore_input_buffering();
void handle_interrupt(int signal);

#endif
