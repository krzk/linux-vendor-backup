#!/bin/sh

# Find abi_version file
ksymver=`find ../../.. -name "Module.symvers"`

ABI_TOOL_LOCATION="/usr/local/bin"
ABI_TOOL_NAME="abi-checker"
ABI_TOOL="${ABI_TOOL_LOCATION}/${ABI_TOOL_NAME}"

if [ -x "${ABI_TOOL}" ]
then
        CMD="${ABI_TOOL}"
elif [ -x "./${ABI_TOOL_NAME}" ]
then
        CMD="./${ABI_TOOL_NAME}"
else
        CMD="${ABI_TOOL_NAME}"
fi

# compare abi fingerprint file with kernel abi file
"${CMD}" test-kernel "${ksymver}" "../data/abi_${1}_${2}"
rc="${?}"

# Test result
if [ ${rc} != "0" ]
then
	echo ""
	echo "----------------------------------------------------------------------------------------------------------------------------"
	echo ""
	echo "The kernel ABI/API has changed. Please update kernel version and add a new tools/abi-checker/data/abi_VERSION_ABIVER file "
	echo "(example tools/abi-checker/data/abi_${1}_${2} )."
	echo "The kernel sources build will abort."
	echo ""
	echo ""
	exit 1
fi

exit 0
