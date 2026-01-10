#ifndef _SETTINGS_H
#define _SETTINGS_H

extern int cycles_per_exit;
extern int frame_skip;
extern int video_mode;  // 0=dither direct, 1=dither CopyBits, 2=indexed

#define VIDEO_DITHER_DIRECT 0
#define VIDEO_DITHER_COPYBITS 1
#define VIDEO_INDEXED 2

#endif