#!/bin/bash

cd "$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
set -x
# make errors fatal
set -e

OPENJPEG_VERSION="1.5.1-polarity"
OPENJPEG_SOURCE_DIR="openjpeg-1.5.1"

if [ -z "$AUTOBUILD" ] ; then 
    fail
fi

if [ "$OSTYPE" = "cygwin" ] ; then
    export AUTOBUILD="$(cygpath -u $AUTOBUILD)"
fi

# load autobuild provided shell functions and variables
set +x
eval "$("$AUTOBUILD" source_environment)"
set -x

stage="$(pwd)/stage"

echo "${OPENJPEG_VERSION}" > "${stage}/VERSION.txt"

build_output="artifacts/${AUTOBUILD_PLATFORM}"
    case "$AUTOBUILD_PLATFORM" in
        windows*)
            case "$AUTOBUILD_PLATFORM" in
                "windows64")
                platform="x64"
                template="Visual Studio 15 Win64"
            ;;
                "windows")
                platform="x86"
                template="Visual Studio 15"
            ;;
            esac
            cmake -B${build_output} -H"${OPENJPEG_SOURCE_DIR}" -G"${template}" -DCMAKE_INSTALL_PREFIX=$stage -DCMAKE_SYSTEM_VERSION="10.0.14393.0" -DOPENJPEG_VERSION="${OPENJPEG_VERSION}"
            msbuild.exe "$(cygpath -m "${build_output}/OPENJPEG.sln")" /p:Configuration="Release" /p:Platform="${platform}" /m
            msbuild.exe "$(cygpath -m "${build_output}/OPENJPEG.sln")" /p:Configuration="Debug" /p:Platform="${platform}" /m
            mkdir -p "$stage/lib/debug"
            mkdir -p "$stage/lib/release"
            cp ${build_output}/bin/Release/openjpeg{.dll,.lib} "$stage/lib/release"
            cp ${build_output}/bin/Debug/openjpeg.dll "$stage/lib/debug/openjpegd.dll"
            cp ${build_output}/bin/Debug/openjpeg.lib "$stage/lib/debug/openjpegd.lib"
            cp ${build_output}/bin/Debug/openjpeg.pdb "$stage/lib/debug/openjpegd.pdb"
            mkdir -p "$stage/include/openjpeg"
            cp ${OPENJPEG_SOURCE_DIR}/libopenjpeg/openjpeg.h "$stage/include/openjpeg"
        ;;
        "darwin")
        pushd "$OPENJPEG_SOURCE_DIR"
        cmake . -GXcode -DCMAKE_OSX_ARCHITECTURES:STRING=x86_64 \
            -DBUILD_SHARED_LIBS:BOOL=ON -DBUILD_CODEC:BOOL=ON -DUSE_LTO:BOOL=ON \
            -DCMAKE_OSX_DEPLOYMENT_TARGET=10.8 -DCMAKE_INSTALL_PREFIX=$stage
        xcodebuild -configuration Release -sdk macosx10.11 \
            -target openjpeg -project openjpeg.xcodeproj
        xcodebuild -configuration Release -sdk macosx10.11 \
            -target install -project openjpeg.xcodeproj
        install_name_tool -id "@executable_path/../Resources/libopenjpeg.dylib" "${stage}/lib/libopenjpeg.5.dylib"
            mkdir -p "${stage}/lib/release"
        cp "${stage}"/lib/libopenjpeg.* "${stage}/lib/release/"
            mkdir -p "${stage}/include/openjpeg"
        cp "libopenjpeg/openjpeg.h" "${stage}/include/openjpeg"
        popd
      
        ;;
        "linux")
            pushd "$OPENJPEG_SOURCE_DIR"
            JOBS=`cat /proc/cpuinfo | grep processor | wc -l`
            HARDENED="-fstack-protector-strong -D_FORTIFY_SOURCE=2"
            CFLAGS="-m32 -O3 -ffast-math $HARDENED" CPPFLAGS="-m32" LDFLAGS="-m32" ./configure --prefix="$stage" \
                --enable-png=no --enable-lcms1=no --enable-lcms2=no --enable-tiff=no
            make -j$JOBS
            make install

            mv "$stage/include/openjpeg-1.5" "$stage/include/openjpeg"

            mv "$stage/lib" "$stage/release"
            mkdir -p "$stage/lib"
            mv "$stage/release" "$stage/lib"
            popd
        ;;
        "linux64")
            pushd "$OPENJPEG_SOURCE_DIR"
            JOBS=`cat /proc/cpuinfo | grep processor | wc -l`
            HARDENED="-fstack-protector-strong -D_FORTIFY_SOURCE=2"
            CFLAGS="-m64 -O3 -ffast-math $HARDENED" CPPFLAGS="-m64" LDFLAGS="-m64" ./configure --prefix="$stage" \
                --enable-png=no --enable-lcms1=no --enable-lcms2=no --enable-tiff=no
            make -j$JOBS
            make install

            mv "$stage/include/openjpeg-1.5" "$stage/include/openjpeg"

            mv "$stage/lib" "$stage/release"
            mkdir -p "$stage/lib"
            mv "$stage/release" "$stage/lib"
            popd
        ;;
    esac
    mkdir -p "$stage/LICENSES"
    cp ${OPENJPEG_SOURCE_DIR}/LICENSE "$stage/LICENSES/openjpeg.txt"
