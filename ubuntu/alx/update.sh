#
# Update from the upstream repository.
#

### github:mcgrof == Luis R. Rodriguez
# ALX_REPO="git://github.com/mcgrof/alx.git"
### github:erikarn == Adrian Chadd
ALX_REPO="git://github.com/erikarn/alx.git"

ALX_AUTHOR="Qualcomm Atheros, Inc. <nic-devel@qualcomm.com>"
set -e

git clone "$ALX_REPO"
descr="UBUNTU: SAUCE: alx: Update to `(cd alx; git describe --all; echo; git remote -v |grep fetch; echo -n "    "; git log --pretty=oneline -1)`"

rsync -av alx/src/ alx/LICENSE alx/README.md .
rm -rf alx

git add .
git commit --author="$ALX_AUTHOR" -s -m"$descr"

git log -1
