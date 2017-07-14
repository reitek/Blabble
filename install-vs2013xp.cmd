mkdir ..\..\..\..\env\Win32\bin
mkdir ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2

rem NOTE: Uncomment the following two lines (or execute them manually) only if the files need to be actually updated
rem xcopy /s /y ..\..\buildPluginSIPxp\firebreath\NativeMessageHost\RelWithDebInfo\*.exe ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2
rem xcopy /s /y ..\..\buildPluginSIPxp\firebreath\NativeMessageHost\RelWithDebInfo\*.pdb ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2
xcopy /s /y ..\..\buildPluginSIPxp\RelWithDebInfo\*.dll ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2
xcopy /s /y ..\..\buildPluginSIPxp\RelWithDebInfo\*.pdb ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2

copy ..\..\buildPluginSIPxp\gen\fwh-chrome-manifest.json ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2\com.reitek.pluginsip.json

mkdir ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2\NativeMessageExtension
xcopy /s /y ..\..\buildPluginSIPxp\NativeMessageExtension\*.* ..\..\..\..\env\Win32\bin\PluginSIPxp_FireBreath2\NativeMessageExtension