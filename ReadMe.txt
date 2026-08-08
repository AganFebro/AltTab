
_________________________      ______  
___    |__  /_  /___  __/_____ ___  /_ 
__  /| |_  /_  __/_  /  _  __ `/_  __ \
_  ___ |  / / /_ _  /   / /_/ /_  /_/ /
/_/  |_/_/  \__/ /_/    \__,_/ /_.___/ 
                                       

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│ ProductName: AltTab                                                         │
│ ProductPage: http://alttab.sourceforge.net/                                 │
│                                                                             │
│      Author: Lokesh Govindu                                                 │
│       Email: lokeshgovindu@gmail.com                                        │
│    HomePage: https://github.com/lokeshgovindu                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘


Introduction
------------
AltTab is an alternative application for native windows switcher.

The switcher follows the Windows light/dark app theme by default and uses a
modern DPI-aware title-plus-process layout. Open Settings to select System,
Light, Dark, or Custom. Custom keeps the INI font, color, search-highlight, and
window-transparency settings used by older AltTab versions.

Existing AltTabSettings.ini files without an [Appearance] Mode entry load as
Custom automatically. New settings files and Reset use:

  [Appearance]
  Mode=System

The Settings dialog also provides a Switcher monitor dropdown. Automatic
(foreground application) keeps the existing behavior. Choosing a named display
locks the switcher to that physical monitor. If it is disconnected, AltTab uses
the primary monitor until the selected display reconnects.

- Use Alt+Tab / Alt+Shift+Tab / Alt+Backtick / Alt+Shift+Backtick to bring the
  main AltTab window, and use 

- Use Alt+Ctrl+Tab to bring the main AltTab window and remains open even Alt key
  is released.

  ┌─────────────┬───────────────────────────────────────────────────────────┐
  │ Key         │ Description                                               │
  ├─────────────┼───────────────────────────────────────────────────────────┤
  │ Tab         │ Select next row                                           │
  │ Shift+Tab   │ Select previous row                                       │
  │ Backtick (`)│ Select next similar process row                           │
  │ Shift+`     │ Select previous similar process row                       │
  │ F1          │ Open help file                                            │
  │ Shift+F1    │ Open About AltTab                                         │
  │ F2          │ Open settings                                             │
  │ Del         │ Close window                                              │
  │ Shift+Del   │ Terminate process                                         │
  │ Apps        │ Show context menu                                         │
  └─────────────┴───────────────────────────────────────────────────────────┘

  Please check the AltTab icon in system tray for some more settings:
  -------------------------------------------------------------------
  ┌───────────────────────────┐
  │ About AltTab              │
  ├───────────────────────────┤
  │ ReadMe                    │
  │ Help                      │
  │ Release Notes             │
  ├───────────────────────────┤
  │ Settings                  │
  │ Disable AltTab            │
  │ Check for Updates         │
  │ Rut at Startup            │
  │ Rut as Administrator      │
  ├───────────────────────────┤
  │ Close All Windows         │
  ├───────────────────────────┤
  │ Reload AltTabSettings.ini │
  │ Restart                   │
  │ Exit                      │
  └───────────────────────────┘


Installation
------------
Just execute the AltTab and try the following:
  Alt+Tab / Alt+Shift+Tab
  Alt+Ctrl+Tab / Alt+Ctrl+Shift+Tab
  Alt+Backtick(~) / Alt+Shift+Backtick(~)


Websites
--------
AltTab official site:
  http://alttab.sourceforge.net/

AltTab wiki:
  https://sourceforge.net/p/alttab/wiki/Home/

AltTab support:
  https://sourceforge.net/projects/alttab/support/


Known Limitations
-----------------
- When running any application as an administrator (as elevated permissions), 
  AltTab hotkeys does not work when the elevated applications are in focus.
  This can be addressed by also running AltTab as an administrator.


Known Issues
------------
- Available @ https://sourceforge.net/p/alttab/tickets/


Credits:
--------
- First thanks to God
- https://github.com/tmplt/fuzzywuzzy : fuzzywuzzy
- https://github.com/kerryland        : Contributions to AltTab
- https://www.helpndoc.com/           : Used for creating CHM help document.
- https://icons8.com/                 : Used for icons.
- Everyone :-)
