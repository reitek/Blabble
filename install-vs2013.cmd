mkdir ..\..\..\..\env\Win32\bin
mkdir ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2

rem NOTE: Uncomment the following two lines (or execute them manually) only if the files need to be actually updated
rem xcopy /s /y ..\..\buildPluginSIP\firebreath\NativeMessageHost\RelWithDebInfo\*.exe ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2
rem xcopy /s /y ..\..\buildPluginSIP\firebreath\NativeMessageHost\RelWithDebInfo\*.pdb ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2
xcopy /s /y ..\..\buildPluginSIP\RelWithDebInfo\*.dll ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2
xcopy /s /y ..\..\buildPluginSIP\RelWithDebInfo\*.pdb ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2

copy ..\..\buildPluginSIP\gen\fwh-chrome-manifest.json ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2\com.reitek.pluginsip.json

mkdir ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2\NativeMessageExtension
xcopy /s /y ..\..\buildPluginSIP\NativeMessageExtension\*.* ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2\NativeMessageExtension