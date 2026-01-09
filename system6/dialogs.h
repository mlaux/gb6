#ifndef _DIALOGS_H
#define _DIALOGS_H

#define ALERT_NORMAL 0
#define ALERT_NOTE 1
#define ALERT_CAUTION 2
#define ALERT_STOP 3

#define DLOG_ABOUT 128
#define DLOG_KEY_MAPPINGS 129
#define DLOG_PREFERENCES 130

#define RES_KEYS_TYPE 'KEYS'
#define RES_KEYS_ID 128

#define RES_PREFS_TYPE 'PREF'
#define RES_PREFS_ID 128

int ShowOpenBox(void);
int SaveScreenshot(void);

void ShowAboutBox(void);
void ShowKeyMappingsDialog(void);
void ShowPreferencesDialog(void);

void LoadKeyMappings(void);
void SaveKeyMappings(void);

void LoadPreferences(void);
void SavePreferences(void);

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