#ifndef _DIALOGS_H
#define _DIALOGS_H

#define ALERT_NORMAL 0
#define ALERT_NOTE 1
#define ALERT_CAUTION 2
#define ALERT_STOP 3

int ShowOpenBox(void);

void ShowAboutBox(void);

typedef short (*AlertProc)(short alertID);

short ShowCenteredAlert(
    short alertID,
    const char *s0,
    const char *s1,
    const char *s2,
    const char *s3,
    int alertType
);

#endif