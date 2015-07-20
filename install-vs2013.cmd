mkdir ..\..\..\..\env\Win32\bin
mkdir ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath1

xcopy /s /y ..\..\buildPluginSIP\bin\PluginSIP\RelWithDebInfo\*.dll ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath1
xcopy /s /y ..\..\buildPluginSIP\bin\PluginSIP\RelWithDebInfo\*.pdb ..\..\..\..\env\Win32\bin\PluginSIP_FireBreath1