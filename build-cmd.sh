#!/bin/bash

cd "$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
set -x
# make errors fatal
set -e

PNG_VERSION="1.4"
PNG_SOURCE_DIR="openjpeg_v1_4_sources_r697"

if [ -z "$AUTOBUILD" ] ; then 
    fail
fi

if [ "$OSTYPE" = "cygwin" ] ; then
    export AUTOBUILD="$(cygpath -u $AUTOBUILD)"
fi

# load autbuild provided shell functions and variables
set +x
eval "$("$AUTOBUILD" source_environment)"
set -x

stage="$(pwd)/stage"
pushd "$PNG_SOURCE_DIR"
    case "$AUTOBUILD_PLATFORM" in
        "windows")
            load_vsvars
            
            build_sln "DllOpenJPEG.sln" "Release|Win32" "DllOpenJPEG"
            build_sln "DllOpenJPEG.sln" "Debug|Win32" "DllOpenJPEG"
            mkdir -p "$stage/lib/debug"
            mkdir -p "$stage/lib/release"
#            cp projects/vstudio/Release\ Library/libpng15.lib "$stage/lib/release/libpng15.lib"
#            cp projects/vstudio/libpng/Release\ Library/vc100*\.?db "$stage/lib/release/"
#            cp projects/vstudio/Debug\ Library/libpng15.lib "$stage/lib/debug/libpng15.lib"
#            cp projects/vstudio/libpng/Debug\ Library/vc100*\.?db "$stage/lib/debug/"
#            mkdir -p "$stage/include/libpng15"
#            cp {png.h,pngconf.h,pnglibconf.h} "$stage/include/libpng15"
        ;;
        "darwin")
            ./configure --prefix="$stage" --with-zlib-prefix="$stage/packages" --enable-png=no
            make
            make install
#	    mkdir -p "$stage/lib/release"
#	    cp "$stage/lib/libpng15.a" "$stage/lib/release/"
        ;;
        "linux")
	    CFLAGS="-m32" CXXFLAGS="-m32" ./configure --prefix="$stage" --enable-png=no
            make
            make install
#	    mkdir -p "$stage/lib/release"
#	    cp "$stage/lib/libpng15.a" "$stage/lib/release/"
        ;;
    esac
    mkdir -p "$stage/LICENSES"
    cp LICENSE "$stage/LICENSES/openjpeg.txt"
popd

pass
