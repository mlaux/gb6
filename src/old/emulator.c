/* Game Boy emulator for 68k Macs
   Compiled with Symantec THINK C 5.0
   (c) 2013 Matt Laux
   
   emulator.c - entry point */

#include <stdio.h>
#include <Windows.h>
#include <Quickdraw.h>
#include <StandardFile.h>
#include <Dialogs.h>
#include <Menus.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <Memory.h>

#include "gb_types.h"
#include "z80.h"
#include "emulator.h"

WindowPtr g_wp;
unsigned char g_running;

static Point windowPt = { WINDOW_Y, WINDOW_X };

static Rect windowBounds = { WINDOW_Y, WINDOW_X, WINDOW_Y + WINDOW_HEIGHT, WINDOW_X + WINDOW_WIDTH };

emu_state theState;

void InitEverything(void)
{	
	Handle mbar;
	
	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(0L);
	InitCursor();
	
	mbar = GetNewMBar(MBAR_DEFAULT);
	SetMenuBar(mbar);
	DrawMenuBar();
	
	g_running = 1;
}

void Render(void)
{
	MoveTo(10, 10);
	DrawString("\pTest 123");
}

void StartEmulation(void)
{
	g_wp = NewWindow(0, &windowBounds, WINDOW_TITLE, true, 
				noGrowDocProc, (WindowPtr) -1, true, 0);
	SetPort(g_wp);
	
	theState.cpu = z80_create();
	theState.cpu->regs->pc = 0x100;
	z80_run(theState.cpu);
}

bool LoadRom(FSSpec *fp)
{
	int err;
	short fileNo;
	long amtRead;
	
	if(theState.rom != NULL) {
		// unload existing ROM
		free((char *) theState.rom);
		theState.romLength = 0;
	}
	
	err = FSpOpenDF(fp, fsRdWrPerm, &fileNo);
	
	if(err != noErr) {
		return false;
	}
	
	GetEOF(fileNo, (long *) &theState.romLength);
	theState.rom = (unsigned char *) malloc(theState.romLength);
	if(theState.rom == NULL) {
		Alert(ALRT_NOT_ENOUGH_RAM, NULL);
		return false;
	}
	
	amtRead = theState.romLength;
	
	FSRead(fileNo, &amtRead, theState.rom);
	return true;
}

// -- DIALOG BOX FUNCTIONS --

bool ShowOpenBox(void)
{
	StandardFileReply reply;
	Point pt = { 0, 0 };
	
	StandardGetFile(NULL, -1, NULL, &reply);
	
	if(reply.sfGood) {
		return LoadRom(&reply.sfFile);
	}
	
	return false;
}

void ShowAboutBox(void)
{
	DialogPtr dp;
	EventRecord e;
	
	dp = GetNewDialog(DLOG_ABOUT, 0L, (WindowPtr) -1L);
	
	DrawDialog(dp);
	
	while(!GetNextEvent(mDownMask, &e));
	while(WaitMouseUp());
	
	DisposeDialog(dp);
}

// -- EVENT FUNCTIONS --

void OnMenuAction(long action)
{
	short menu, item;
	
	if(action <= 0)
		return;

	HiliteMenu(0);
	
	menu = HiWord(action);
	item = LoWord(action);
	
	if(menu == MENU_APPLE) {
		if(item == APPLE_ABOUT) {
			ShowAboutBox();
		}	
	}
	
	else if(menu == MENU_FILE) {
		if(item == FILE_OPEN) {
			if(ShowOpenBox())
				StartEmulation();
		}
		else if(item == FILE_QUIT) {
			g_running = 0;
		}
	}
}

void OnMouseDown(EventRecord *pEvt)
{
	short part;
	WindowPtr clicked;
	long action;
	
	part = FindWindow(pEvt->where, &clicked);
	
	switch(part) {
		case inDrag:
			DragWindow(clicked, pEvt->where, &qd.screenBits.bounds);
			break;
		case inGoAway:
			if(TrackGoAway(clicked, pEvt->where))
				DisposeWindow(clicked);
			break;
		case inContent:
			if(clicked != FrontWindow())
				SelectWindow(clicked);
			break;
		case inMenuBar:
			action = MenuSelect(pEvt->where);
			OnMenuAction(action);
			break;
	}
}

// -- ENTRY POINT --

int main(int argc, char *argv[])
{
	EventRecord evt;
	
	InitEverything();
	
	while(g_running) {
		if(WaitNextEvent(everyEvent, &evt, 10, 0) != nullEvent) {
			switch(evt.what) {
				case mouseDown:
					OnMouseDown(&evt);
					break;
				case updateEvt:
					BeginUpdate((WindowPtr) evt.message);
					Render();
					EndUpdate((WindowPtr) evt.message);
					break;
			}
		}
	}
	
	return 0;
}
