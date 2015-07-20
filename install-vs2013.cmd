mkdir ..\..\..\..\env\Win32\bin
mkdir ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2

xcopy /s /y ..\..\buildPluginSIP\RelWithDebInfo\*.exe ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2
xcopy /s /y ..\..\buildPluginSIP\RelWithDebInfo\*.dll ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2
xcopy /s /y ..\..\buildPluginSIP\RelWithDebInfo\*.pdb ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2

copy ..\..\buildPluginSIP\gen\fwh-chrome-manifest.json ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2\com.reitek.PluginSIP.json

mkdir ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2\NativeMessageExtension
xcopy /s /y ..\..\buildPluginSIP\NativeMessageExtension\*.* ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath2\NativeMessageExtension