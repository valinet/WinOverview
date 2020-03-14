# WinOverview
WinOverview is a reproduction of the GNOME Activities (Overview) for Microsoft Windows.

This project is still a work in progress. Main functionality is implemented, the program behaves like the original GNOME Activities, but there are still some quirks to be ironed out.

![Screenshot](/docs/screenshot.png?raw=true "Screenshot")

## Compiling
### Method 1 - using Visual Studio
Open solution in Visual Studio 2019 and hit compile.
### Method 2 - without Visual Studio, just Microsoft C++ compiler
* Install Build Tools for Visual Studio 2019 from: https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools&rel=16 - when installing, make sure to install a Windows 10 SDK as well
* After installation, hit Start and type to open Developer Command Prompt for Visual Studio 2019.
* Go to cloned folder and enter subdirectory "Overview" (where source files .cpp and header files .h will be present)
* First, compile the resources (icon and version info) into a resource file:
> rc /nologo Overview.rc
* Then, issue a command like the following (it will generate Overview.exe):
> cl /nologo /DUNICODE Overview.cpp workspace.cpp /FeOverview.exe /link Overview.res user32.lib gdi32.lib ole32.lib dwmapi.lib
* If you want the application to display an icon in the taskbar whwn running, compile like the following (define variable HIDE_TASKBAR_ICON):
> cl /nologo /DSHOW_TASKBAR_ICON /DUNICODE Overview.cpp workspace.cpp /FeOverview.exe /link Overview.res user32.lib gdi32.lib ole32.lib dwmapi.lib
* If it does not work, maybe the compiler does not see either the header path, the libraries path, or both; issue a command like the following - replace those paths with locations for Windows header files and libs:
> cl /nologo /DUNICODE /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\um" Overview.cpp workspace.cpp /FeOverview.exe /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.18362.0\um\x64" Overview.res user32.lib gdi32.lib ole32.lib dwmapi.lib
* You can change the app icon by replacing the "icon.ico" with your own icon file and recompiling - I included the default icon that Visual Studio suggests due to licensing issues

## Configuration
* Clone project
* Compile - I used Visual Studio 2019, so Build Tools v16, cl v19 and Windows 10 SDK version 10.0.18362.0
* Run app - when opening, it will automatically display the 'overview' screen; if you execute the app again, it will close the already opened overview, so you could configure a key to launch the application and it would toggle between overview and normal mode, just like on GNOME - I used AutoHotKey like so:
> LWin & vk07::Run "C:\Users\Valentin\Documents\Visual Studio 2019\Projects\Overview\x64\Release\Overview.exe"
>
> LWin::Run "C:\Users\Valentin\Documents\Visual Studio 2019\Projects\Overview\x64\Release\Overview.exe"
>
> RWin & vk07::Run "C:\Users\Valentin\Documents\Visual Studio 2019\Projects\Overview\x64\Release\Overview.exe"
>
> RWin::Run "C:\Users\Valentin\Documents\Visual Studio 2019\Projects\Overview\x64\Release\Overview.exe"
* For searching, the app works with the default shortcut for Wox (alternative window search program, powered by versatile Everything, because the Windows 10 search is always broken - https://github.com/Wox-launcher/Wox); that is, Alt + Space
* Pressing on a live thumbnail will bring that window to the foreground
* Pressing anywhere on the "desktop", or Escape will dismiss overview
* Developed and tested on Windows 10, but it should work on Windows 7 (with Windows Aero enabled, otherwise Windows 8) and newer.

## To do
* Improve performance (especially on animations)
* Clean up code
* Allow for window title to be shown
* Allow for windows to be closed from the overview
* Configuration UI

## Known issues
* Desktop background may not be displayed correctly on a multi monitor configuration; generally, I did not have the time to test multi monitor configurations
* Due to live thumbnail computations being done on the CPU, the animation may lag or skip frames when the CPU is under high load; otherwise, it performs better than Win+Tab which is always glitchy and does not open windows at the proper locations initially

## Acknowledgments
* Thanks to GNOME team for providing a great, easily portable code for displaying the live previews on screen - check out https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/master/js/ui/workspace.js
* Thanks to Microsoft for providing a powerful and versatile API (Win32)
