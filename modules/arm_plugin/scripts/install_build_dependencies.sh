#!/bin/bash

# Copyright (C) 2018-2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set -e

export WORK_DIR=$(pwd)
# Move into contrib install_build_dependencies.sh
sudo apt --assume-yes install scons crossbuild-essential-arm64 libprotoc-dev libhiredis-dev
sudo apt --assume-yes install protobuf-compiler default-jdk libssl-dev zip libzstd-dev python-dev autoconf libtool
# Speed up build
wget https://github.com/ninja-build/ninja/releases/download/v1.10.2/ninja-linux.zip
unzip ninja-linux.zip
sudo cp -v ninja /usr/local/bin/

#### Build python for host
wget https://www.python.org/ftp/python/"$PYTHON_ARM_VERSION"/Python-"$PYTHON_ARM_VERSION".tgz
tar -xzf Python-"$PYTHON_ARM_VERSION".tgz
mv Python-"$PYTHON_ARM_VERSION" Python-"$PYTHON_ARM_VERSION"-host
cd Python-"$PYTHON_ARM_VERSION"-host || exit
./configure --enable-optimizations
make -j "$NUM_PROC" python Parser/pgen
sudo make -j "$NUM_PROC" install
cp python "$BUILD_PYTHON"
cp -r Parser/pgen "$BUILD_PYTHON"
curl https://bootstrap.pypa.io/get-pip.py | "$BUILD_PYTHON"/python - --no-cache-dir numpy cython
cd "$WORK_DIR" || exit

#### Build python for ARM
wget https://www.python.org/ftp/python/"$PYTHON_ARM_VERSION"/Python-"$PYTHON_ARM_VERSION".tgz
tar -xzf Python-"$PYTHON_ARM_VERSION".tgz
cd Python-"$PYTHON_ARM_VERSION" || exit
CC=aarch64-linux-gnu-gcc \
CXX=aarch64-linux-gnu-g++ \
AR=aarch64-linux-gnu-ar \
READELF=aarch64-linux-gnu-readelf \
RANLIB=aarch64-linux-gnu-ranlib \
    ./configure \
    --build=x86_64-linux-gnu \
    --host=aarch64-linux-gnu \
    --prefix="$INSTALL_PYTHON" \
    --disable-ipv6 ac_cv_file__dev_ptmx=no ac_cv_file__dev_ptc=no ac_cv_have_long_long_format=yes \
    --enable-shared
make -j "$NUM_PROC" \
    HOSTPYTHON="$BUILD_PYTHON"/python \
    HOSTPGEN="$BUILD_PYTHON"/Parser/pgen \
    CROSS-COMPILE=aarch64-linux-gnu- CROSS_COMPILE_TARGET=yes HOSTARCH=aarch64-linux BUILDARCH=aarch64-linux-gnu
make -j "$NUM_PROC" install
cd "$WORK_DIR" || exit
sudo /usr/local/bin/"$PYTHON_EXEC" -m pip install --upgrade pip
sudo /usr/local/bin/"$PYTHON_EXEC" -m pip install numpy cython

# hwloc install
wget https://download.open-mpi.org/release/hwloc/v1.11/hwloc-1.11.13.tar.gz -P $WORK_DIR
wget https://download.open-mpi.org/release/hwloc/v2.5/hwloc-2.5.0.tar.gz -P $WORK_DIR

tar -xzf $WORK_DIR/hwloc-1.11.13.tar.gz -C $WORK_DIR
tar -xzf $WORK_DIR/hwloc-2.5.0.tar.gz -C $WORK_DIR

git clone --recursive https://github.com/open-mpi/hwloc $WORK_DIR/hwloc
$WORK_DIR/hwloc/autogen.sh

declare -a StringArray=("hwloc" "hwloc-1.11.13" "hwloc-2.5.0")
for HWLOC_VERSION in ${StringArray[@]}
do
   [ -d $WORK_DIR/install_hwloc/$HWLOC_VERSION ] || mkdir -p $WORK_DIR/install_hwloc/$HWLOC_VERSION

   cd $WORK_DIR/$HWLOC_VERSION
   CC=aarch64-linux-gnu-gcc \
   CXX=aarch64-linux-gnu-g++ \
   ./configure \
        --host=aarch64 \
        --prefix=$WORK_DIR/install_hwloc/$HWLOC_VERSION \
        --with-pic=yes

   make -j $(nproc --all)
   make install

   cd $WORK_DIR/install_hwloc/$HWLOC_VERSION/lib
   aarch64-linux-gnu-ar -x libhwloc.a
   aarch64-linux-gnu-g++ -shared *.o -o  libhwloc.so
done
cd $WORK_DIR || exit

# oneTBB install
git clone --recursive https://github.com/oneapi-src/oneTBB.git $ONETBB_REPO_DIR
cmake -GNinja \
      -DCMAKE_HWLOC_1_11_LIBRARY_PATH=$WORK_DIR/install_hwloc/hwloc-1.11.13/lib/libhwloc.so \
      -DCMAKE_HWLOC_1_11_INCLUDE_PATH=$WORK_DIR/install_hwloc/hwloc-1.11.13/include \
      -DCMAKE_HWLOC_2_5_LIBRARY_PATH=$WORK_DIR/install_hwloc/hwloc-2.5.0/lib/libhwloc.so \
      -DCMAKE_HWLOC_2_5_INCLUDE_PATH=$WORK_DIR/install_hwloc/hwloc-2.5.0/include \
      -DCMAKE_HWLOC_2_LIBRARY_PATH=$WORK_DIR/install_hwloc/hwloc/lib/libhwloc.so \
      -DCMAKE_HWLOC_2_INCLUDE_PATH=$WORK_DIR/install_hwloc/hwloc/include \
      -D CMAKE_BUILD_TYPE=Release \
      -D CMAKE_TOOLCHAIN_FILE="$OPENVINO_REPO_DIR"/cmake/arm64.toolchain.cmake \
      -D CMAKE_INSTALL_PREFIX="$INSTALL_ONETBB" \
      -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
      -D CMAKE_C_COMPILER_LAUNCHER=ccache \
      -S $ONETBB_REPO_DIR \
      -B $BUILD_ONETBB
export CCACHE_DIR=$ONETBB_CCACHE_DIR
ninja -C $BUILD_ONETBB
ninja -C $BUILD_ONETBB install

touch "$INSTALL_ONETBB"/setupvars.sh
printf "export TBB_DIR=\$INSTALLDIR/extras/oneTBB/cmake/TBB;" >> "$INSTALL_ONETBB"/setupvars.sh
printf "export LD_LIBRARY_PATH=\$INSTALLDIR/extras/oneTBB/lib:\$LD_LIBRARY_PATH" >> "$INSTALL_ONETBB"/setupvars.sh
cd "$WORK_DIR" || fail 11 "oneTBB build failed. Stopping"

# OpenCV install
git clone https://github.com/opencv/opencv.git --depth 1 "$OPENCV_REPO_DIR"
cmake -G Ninja \
      -D CMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -D TBB_DIR="$INSTALLDIR"/extras/oneTBB/cmake/TBB \
      -D BUILD_opencv_python2=OFF \
      -D BUILD_opencv_python3=ON \
      -D OPENCV_SKIP_PYTHON_LOADER=OFF \
      -D PYTHON3_LIMITED_API=ON \
      -D PYTHON3_INCLUDE_PATH="$INSTALL_PYTHON"/include/"$PYTHON_EXEC" \
      -D PYTHON3_LIBRARIES="$INSTALL_PYTHON"/lib \
      -D PYTHON3_NUMPY_INCLUDE_DIRS=/usr/local/lib/"$PYTHON_EXEC"/site-packages/numpy/core/include \
      -D CMAKE_USE_RELATIVE_PATHS=ON \
      -D CMAKE_SKIP_INSTALL_RPATH=ON \
      -D OPENCV_SKIP_PKGCONFIG_GENERATION=ON \
      -D OPENCV_BIN_INSTALL_PATH=bin \
      -D OPENCV_PYTHON3_INSTALL_PATH=python \
      -D OPENCV_INCLUDE_INSTALL_PATH=include \
      -D OPENCV_LIB_INSTALL_PATH=lib \
      -D OPENCV_CONFIG_INSTALL_PATH=cmake \
      -D OPENCV_3P_LIB_INSTALL_PATH=3rdparty \
      -D OPENCV_SAMPLES_SRC_INSTALL_PATH=samples \
      -D OPENCV_DOC_INSTALL_PATH=doc \
      -D OPENCV_OTHER_INSTALL_PATH=etc \
      -D OPENCV_LICENSES_INSTALL_PATH=etc/licenses \
      -D CMAKE_TOOLCHAIN_FILE="$OPENVINO_REPO_DIR"/cmake/arm64.toolchain.cmake \
      -D WITH_GTK_2_X=OFF \
      -D OPENCV_ENABLE_PKG_CONFIG=ON \
      -D ENABLE_CCACHE=ON \
      -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
      -D CMAKE_C_COMPILER_LAUNCHER=ccache \
      -D PKG_CONFIG_EXECUTABLE=/usr/bin/aarch64-pkg-config \
      -D CMAKE_INSTALL_PREFIX="$INSTALL_OPENCV" \
      -S "$OPENCV_REPO_DIR" \
      -B "$BUILD_OPENCV"
export CCACHE_DIR=$OPENCV_CCACHE_DIR
ninja -C "$BUILD_OPENCV"
ninja -C "$BUILD_OPENCV" install
touch "$INSTALL_OPENCV"/setupvars.sh
printf "export OpenCV_DIR=\$INSTALLDIR/extras/opencv/cmake;" >> "$INSTALL_OPENCV"/setupvars.sh
printf "export LD_LIBRARY_PATH=\$INSTALLDIR/extras/opencv/lib:\$LD_LIBRARY_PATH" >> "$INSTALL_OPENCV"/setupvars.sh
mkdir -p "$INSTALL_OPENVINO"/python/python3
cp -r "$INSTALL_OPENCV"/python/cv2 "$INSTALL_OPENVINO"/python/python3
