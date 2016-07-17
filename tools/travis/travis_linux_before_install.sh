#!/bin/bash
set -ev

# Increase the environment version to force a rebuild of the packages
# The version is writen to the cache file after every build of the dependencies
ENV_VERSION="1"
ENV_INSTALLED="none"
if [ -e "$LOCAL_PKG/.cached" ]; then
   read -r ENV_INSTALLED < "$LOCAL_PKG/.cached"
fi

# travis caches the $LOCAL_PKG dir. If it is loaded, we don't need to reinstall the packages
if [ "$ENV_VERSION" = "$ENV_INSTALLED" ]; then
    echo "\n=== The build environment is current ==="
    # Print version numbers
    clang --version
    g++ --version
    cppcheck --version
    valgrind --version
    exit 0
fi

echo "\n=== Updating the build environment ==="

# Clean up
rm -rf $LOCAL_PKG/*
rm -rf $LOCAL_PKG/.*

# Install newer valgrind
echo "\n=== Installing valgrind ==="
wget http://valgrind.org/downloads/valgrind-3.11.0.tar.bz2
tar xf valgrind-3.11.0.tar.bz2
cd valgrind-3.11.0
./configure --prefix=$LOCAL_PKG
make -s -j8 install
cd ..

# Install specific check version which is not yet in the apt package
echo "\n=== Installing check ==="
mkdir tmp_check
wget http://ftp.de.debian.org/debian/pool/main/c/check/check_0.10.0-3_amd64.deb
dpkg -x check_0.10.0-3_amd64.deb ./tmp_check
# change pkg-config file path
sed -i "s|prefix=/usr|prefix=${LOCAL_PKG}|g" .tmp_check/usr/lib/x86_64-linux-gnu/pkgconfig/check.pc
sed -i 's|libdir=.*|libdir=${prefix}/lib|g' ./tmp_check/usr/lib/x86_64-linux-gnu/pkgconfig/check.pc
# move files to globally included dirs
cp -R ./tmp_check/usr/lib/x86_64-linux-gnu/* $LOCAL_PKG/lib/
cp -R ./tmp_check/package/usr/include/* $LOCAL_PKG/include/
cp -R ./tmp_check/package/usr/bin/* $LOCAL_PKG/

# Install specific liburcu version which is not yet in the apt package
echo "\n=== Installing liburcu ==="
mkdir tmp_liburcu
wget https://launchpad.net/ubuntu/+source/liburcu/0.8.5-1ubuntu1/+build/6513813/+files/liburcu2_0.8.5-1ubuntu1_amd64.deb
wget https://launchpad.net/ubuntu/+source/liburcu/0.8.5-1ubuntu1/+build/6513813/+files/liburcu-dev_0.8.5-1ubuntu1_amd64.deb
dpkg -x liburcu2_0.8.5-1ubuntu1_amd64.deb ./tmp_liburcu
dpkg -x liburcu-dev_0.8.5-1ubuntu1_amd64.deb ./tmp_liburcu
# move files to globally included dirs
cp -R ./tmp_liburcu/usr/lib/x86_64-linux-gnu/* $LOCAL_PKG/lib/
cp -R ./tmp_liburcu/usr/include/* $LOCAL_PKG/include/

# Install newer cppcheck
echo "\n=== Installing cppcheck ==="
wget https://github.com/danmar/cppcheck/archive/1.73.tar.gz -O cppcheck-1.73.tar.gz
tar xf cppcheck-1.73.tar.gz
cd cppcheck-1.73
make PREFIX="$LOCAL_PKG" SRCDIR=build CFGDIR="$LOCAL_PKG/cppcheck-cfg" HAVE_RULES=yes CXXFLAGS="-O2 -DNDEBUG -Wall -Wno-sign-compare -Wno-unused-function" -j8
make PREFIX="$LOCAL_PKG" SRCDIR=build CFGDIR="$LOCAL_PKG/cppcheck-cfg" HAVE_RULES=yes install
cd ..

# Install python packages
echo "\n=== Installing python packages ==="
pip install --user cpp-coveralls
pip install --user sphinx
pip install --user sphinx_rtd_theme

# create cached flag
echo $ENV_VERSION > $LOCAL_PKG/.cached

# Print version numbers
echo "\n=== Installed versions are ==="
clang --version
g++ --version
cppcheck --version
valgrind --version
