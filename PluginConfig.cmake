#  Configuration file for Reitek PluginSIP

set(PLUGIN_NAME "PluginSIP")
set(PLUGIN_PREFIX "PS")
set(COMPANY_NAME "Reitek S.p.A.")
set(COMPANY_REVDOMAIN "com.reitek")
# NOTE: If you are going to deploy your own extension you *must* change the
#       key and ID to your own key and matching ID.  You can use cmake/makeid.py
#       (https://gist.github.com/taxilian/d42b8768b2e3ef5c6998) to generate them
set(PLUGIN_CRX_NAME "${COMPANY_REVDOMAIN}.pluginsip")
set(PLUGIN_CRX_KEY "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDVd2HTm3jc05eJiizFvPK7Ta7dG4iguLggWzfdW4/hQCnzs/YWF0J8Cjclzw4gvegEBPKYkq/e1BaujqKMd0QA/Xk/5d8hFMeaVtQYuEe76wB2vdJn7mT+vhpkuIwBvy0aLRpRISlH3Xzu6k8nBBK0TWzuBwbeuRJ4S98Gn+2FVwIDAQAB")
set(PLUGIN_CRX_ID "igpaogmmhnicmonampjhibhcdpgjoffo")
set(PLUGIN_CRX_WYRMHOLE_NAME "${COMPANY_REVDOMAIN}.pluginsip")
set(PLUGIN_CRX_NATIVEHOST_NAME "${COMPANY_REVDOMAIN}.pluginsip")
set(PLUGIN_CRX_NATIVEHOST_DESCRIPTION "${COMPANY_REVDOMAIN}.pluginsip Native Messaging Host")

# ActiveX constants:
set(FBTYPELIB_NAME PjsipJsLib)
set(FBTYPELIB_DESC "${PLUGIN_NAME} 1.0 Type Library")
set(IFBControl_DESC "${PLUGIN_NAME} Control Interface")
set(FBControl_DESC "${PLUGIN_NAME} Control Class")
set(IFBComJavascriptObject_DESC "${PLUGIN_NAME} IComJavascriptObject Interface")
set(FBComJavascriptObject_DESC "${PLUGIN_NAME} ComJavascriptObject Class")
set(IFBComEventSource_DESC "${PLUGIN_NAME} IFBComEventSource Interface")
set(AXVERSION_NUM "1")

# NOTE: THESE GUIDS *MUST* BE UNIQUE TO YOUR PLUGIN/ACTIVEX CONTROL!  YES, ALL OF THEM!
set(FBTYPELIB_GUID 30ad4ef6-ba0e-55ac-9f7a-369ae9e238ab)
set(IFBControl_GUID 8d09154d-9148-7a72-bcf5-0e6aa9d2a9b9)
set(FBControl_GUID 9c8b7f38-92f5-3b13-9762-4dfb17273372)
set(IFBComJavascriptObject_GUID 8ad9d45e-5d2f-538c-a8d8-546f5816d3ee)
set(FBComJavascriptObject_GUID 07844a61-6898-5d1f-b4bd-d2c8af435632)
set(IFBComEventSource_GUID 54562b94-3be8-531b-8635-153405a1da62)
if ( FB_PLATFORM_ARCH_32 )
    set(FBControl_WixUpgradeCode_GUID fa742487-87ad-59e9-a765-1b4c8dbbc531)
else ( FB_PLATFORM_ARCH_32 )
    set(FBControl_WixUpgradeCode_GUID bf084e45-0d0e-5f63-ab52-9138eeeaa131)
endif ( FB_PLATFORM_ARCH_32 )

# these are the pieces that are relevant to using it from Javascript
set(ACTIVEX_PROGID "Reitek.${PLUGIN_NAME}")
set(MOZILLA_PLUGINID "Reitek@${PLUGIN_NAME}.com")

# strings
set(FBSTRING_CompanyName "${COMPANY_NAME}")
set(FBSTRING_PLUGIN_VERSION "1.0.16.0")
set(FBSTRING_LegalCopyright "Copyright 2014 ${COMPANY_NAME} & (Blabble ONLY) Copyright 2010-2013 Andrew Ofisher")
set(FBSTRING_PluginFileName "np${PLUGIN_NAME}.dll")
set(FBSTRING_ProductName "${PLUGIN_NAME}")
set(FBSTRING_PluginName "${PLUGIN_NAME}")  # No 32bit postfix to maintain backward compatability.
set(FBSTRING_FileExtents "")
set(FBSTRING_PluginDescription "JS Bridge for PJSIP based on Blabble")
set(FBSTRING_MIMEType "application/x-pluginSIP")

# If you want to register per-machine on Windows, uncomment this line
#set (FB_ATLREG_MACHINEWIDE 1)

# Uncomment this next line if you're not planning on your plugin doing
# any drawing:

set (FB_GUI_DISABLED 1)

# Mac plugin settings. If your plugin does not draw, set these all to 0
set(FBMAC_USE_QUICKDRAW 0)
set(FBMAC_USE_CARBON 0)
set(FBMAC_USE_COCOA 0)
set (FBMAC_USE_QUICKDRAW                 0)
set(FBMAC_USE_COREGRAPHICS 0)
set(FBMAC_USE_COREANIMATION 0)
set(FBMAC_USE_INVALIDATINGCOREANIMATION 0)
