BASESRCDIR="../../buildPluginSIP"
BASEDESTDIR="${ENVDIR}/bin/PluginSIP_FireBreath2"

mkdir -p "${BASEDESTDIR}"

cp -f "${BASESRCDIR}/firebreath/bin/PluginSIP/FireWyrmNativeMessageHost" "${BASEDESTDIR}"
cp -f "${BASESRCDIR}/firebreath/bin/PluginSIP/npPluginSIP.so" "${BASEDESTDIR}"

cp -f "${BASESRCDIR}/gen/fwh-chrome-manifest.json" "${BASEDESTDIR}/com.reitek.pluginsip.json"
cp -f "${BASESRCDIR}/gen/fwh-mozilla-manifest.json" "${BASEDESTDIR}/com.reitek.pluginsip_firefox.json"

cp -fr "${BASESRCDIR}/NativeMessageExtension" "${BASEDESTDIR}"
