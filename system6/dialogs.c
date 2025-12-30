/* Game Boy emulator for 68k Macs
   alerts.c - centered alert dialogs */

#ifndef UNITY_BUILD
#include <Dialogs.h>
#include <Resources.h>
#include <Menus.h>
#include "emulator.h"
#endif

static int LoadRom(Str63, short);

// return true to hide, false to show
static pascal Boolean RomFileFilter(CInfoPBRec *pb)
{
  StringPtr name;
  unsigned char len;

  // always show 'GBRM' files
  if (pb->hFileInfo.ioFlFndrInfo.fdType == 'GBRM') {
    return false;
  }

  // show files ending in .gb or .GB (for imports)
  name = pb->hFileInfo.ioNamePtr;
  len = name[0];
  if (len >= 3) {
    char c1 = name[len - 2];
    char c2 = name[len - 1];
    char c3 = name[len];
    if (c1 == '.' && (c2 == 'g' || c2 == 'G') && (c3 == 'b' || c3 == 'B')) {
      return false;
    }
  }

  return true;
}

static int ShowOpenBox(void)
{
  SFReply reply;
  Point pt = { 0, 0 };
  const int stdWidth = 348;

  pt.h = qd.screenBits.bounds.right / 2 - stdWidth / 2;

  SFGetFile(pt, NULL, RomFileFilter, -1, NULL, NULL, &reply);
  
  if(reply.good) {
    return LoadRom(reply.fName, reply.vRefNum);
  }
  
  return false;
}

static void ShowAboutBox(void)
{
  DialogPtr dp;
  EventRecord e;
  DialogItemIndex hitItem;
  
  dp = GetNewDialog(DLOG_ABOUT, 0L, (WindowPtr) -1L);
  
  ModalDialog(NULL, &hitItem);

  DisposeDialog(dp);
}

typedef short (*AlertProc)(short alertID);

static short AlertWrapper(short alertID) { return Alert(alertID, NULL); }
static short CautionAlertWrapper(short alertID) { return CautionAlert(alertID, NULL); }
static short NoteAlertWrapper(short alertID) { return NoteAlert(alertID, NULL); }
static short StopAlertWrapper(short alertID) { return StopAlert(alertID, NULL); }

static short ShowCenteredAlert(
    short alertID,
    const char *s0,
    const char *s1,
    const char *s2,
    const char *s3,
    AlertProc alertProc
) {
  Handle alrt;
  Rect *bounds;
  Rect screen;
  short width, height, dh, dv;
  
  ParamText(s0, s1, s2, s3);

  alrt = GetResource('ALRT', alertID);
  if (alrt == nil) {
    return alertProc(alertID);
  }

  bounds = (Rect *) *alrt;
  screen = qd.screenBits.bounds;
  screen.top += GetMBarHeight();

  width = bounds->right - bounds->left;
  height = bounds->bottom - bounds->top;

  /* center horizontally, position 1/4 down vertically */
  dh = ((screen.right - screen.left) - width) / 2 - bounds->left;
  dv = ((screen.bottom - screen.top) - height) / 4 - bounds->top + screen.top;

  OffsetRect(bounds, dh, dv);

  return alertProc(alertID);
}
