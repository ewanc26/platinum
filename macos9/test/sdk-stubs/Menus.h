/* sdk-stubs/Menus.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_MENUS_H
#define PLATINUM_STUB_MENUS_H

#include <MacTypes.h>

typedef struct StubMenu {
    short width;
    short height;
    short id;
} MenuRecord;

typedef MenuRecord *MenuPtr;
typedef MenuPtr MenuRef;
typedef MenuPtr MenuHandle;

void InitMenus(void);
void InitCursor(void);
void DrawMenuBar(void);
long MenuSelect(Point pt);
void HiliteMenu(short menuID);

MenuHandle GetMenu(short menuID);
MenuHandle NewMenu(short menuID, Str255 title);
void AppendMenu(MenuHandle menu, Str255 itemStr);
void InsertMenu(MenuHandle menu, short beforeID);
/* The command-key equivalent of an item. */
void SetItemCmd(MenuHandle theMenu, short item, short cmdChar);
void DisableItem(MenuHandle menu, short itemID);
void DeleteMenu(short menuID);
void DisposeMenu(MenuHandle menu);

#endif
