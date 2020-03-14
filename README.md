# WinOverview
WinOverview is a reproduction of the GNOME Activities (Overview) for Microsoft Windows.

This project is still a work in progress. Main functionality is implemented, the program behaves like the original GNOME Activities, but there are still some quirks to be ironed out.

## Configuration
* Clone project
* Compile - I used Visual Studio 2019, so Build Tools v17, cl v19 and Windows 10 SDK version 10.0.18362.0
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
