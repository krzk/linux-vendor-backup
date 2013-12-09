#!/bin/sh

# Find abi_version file
ksymver=`find ../.. -name "Module.symvers"`

# compare abi fingerprint file with kernel abi file
./abi-checker test-kernel "${ksymver}" "../data/abi_${1}_${2}"
rc="${?}"

# Test result
if [ ${rc} != "0" ]
then
	echo ""
	echo "----------------------------------------------------------------------------------------------------------------------------"
	echo ""
	echo "The kernel ABI/API has changed. Please update kernel version and add a new abi-checker/data/abi_VERSION_ABIVER file "
	echo "(example abi-checker/data/abi_${1}_${2} )."
	echo "The kernel sources build will abort."
	echo ""
	echo ""
	exit 1
fi

exit 0
