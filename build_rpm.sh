#!/bin/bash

VERSION_FILE="VERSION"
BUILD_NUMBER=$1

export PATH=/opt/lzss/python/python-2.7/bin:$PATH

echo "Building RPM for build number $BUILD_NUMBER"

BASE_VERSION="$(cat $VERSION_FILE | cut -d '.' -f1,2)"
echo "Base Version: $BASE_VERSION (Read from $VERSION_FILE)"

NEW_VERSION=$BASE_VERSION.$BUILD_NUMBER
echo "New Version: $NEW_VERSION"

echo "Updating version file with new version. '$NEW_VERSION'"
echo $NEW_VERSION > VERSION

echo "Generating python distribution"
python setup.py sdist

mkdir $WORKSPACE/rpmbuild

rpmbuild --define "_topdir $WORKSPACE/rpmbuild" --define "__build_number $BUILD_NUMBER" --define "_sourcedir `pwd`/dist" -ba python-lzss.spec

cp rpmbuild/*RPMS/x86_64/*.rpm dist/
#cp rpmbuild/*RPMS/noarch/*.rpm /opt/lzss/src/zta
#cp rpmbuild/SRPMS/*.rpm /opt/lzss/src/zta
