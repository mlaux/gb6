/* platform.h - Platform abstraction for GB emulator */

#ifndef PLATFORM_H
#define PLATFORM_H

/*
 * Set the status bar text displayed below the LCD.
 * On System 6, this replaces the FPS counter.
 * On CLI/ImGui, this is a no-op (debug info shown elsewhere).
 */
void set_status_bar(const char *str);

#endif
