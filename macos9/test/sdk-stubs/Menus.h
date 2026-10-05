/* sdk-stubs/Menus.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_MENUS_H
#define PLATINUM_STUB_MENUS_H

#include <MacTypes.h>

typedef struct StubMenuItem {
    short width;
    short height;
    unsigned char enabled;
} MenuItemRec;

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
void HiliteMenu(long menuID);
SInt16 MenuBarHitTest(short x, short y);

MenuHandle GetMenu(short menuID);
MenuHandle NewMenu(short menuID, Str255 title);
void AppendMenu(MenuHandle menu, Str255 itemStr);
void InsertMenuItem(MenuHandle menu, short itemID, Str255 itemStr, short after);
void AddMenuLine(MenuHandle menu, Str255 itemStr);
void SetItemCmdChar(MenuHandle menu, short itemID, char cmdChar);
void GetItemCmdChar(MenuHandle menu, short itemID, char *cmdChar);
void DisableItem(MenuHandle menu, short itemID);
void EnableItem(MenuHandle menu, short itemID);
void DeleteMenu(short menuID);
void DisposeMenu(MenuHandle menu);
short CountItems(MenuHandle menu);
void GetItem(MenuHandle menu, short itemID, MenuItemRec *item);
void SetItemMark(MenuHandle menu, short itemID, unsigned char mark);
void InsertMenu(MenuHandle menu, short beforeID);
void InsertMenuItem(MenuHandle menu, short itemID, Str255 itemStr, short after);
void AppendMenuItem(MenuHandle menu, Str255 itemStr);
void DeleteMenuItem(MenuHandle menu, short itemID);
void SetMenuItemText(MenuHandle menu, short itemID, Str255 itemStr);
void GetMenuItemText(MenuHandle menu, short itemID, Str255 itemStr);
short CountMenuItems(MenuHandle menu);

void EnableMenuItem(MenuHandle menu, short itemID);
void DisableMenuItem(MenuHandle menu, short itemID);
void CheckMenuItem(MenuHandle menu, short itemID);
void UncheckMenuItem(MenuHandle menu, short itemID);

short GetMenuItemCmdKey(MenuHandle menu, short itemID);
void SetMenuItemCmdKey(MenuHandle menu, short itemID, short keyCode);

#endif
