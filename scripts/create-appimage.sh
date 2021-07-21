#!/bin/env bash

if [ ! -e "src/api/api.h" ]; then
  echo "Please run this script from the root directory of Lite XL."
  exit 1
fi

ARCH="$(uname -m)"
APPIMAGETOOL=appimagetool

setup_appimagetool(){
  if ! which appimagetool > /dev/null ; then
    if [ ! -e appimagetool ]; then
      if ! wget -O appimagetool "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${ARCH}.AppImage" ; then
        echo "Could not download the appimagetool for the arch '${ARCH}'."
        exit 1
      else
        chmod 0755 appimagetool
        APPIMAGETOOL="./appimagetool"
      fi
    fi
  fi
}

download_appimage_apprun(){
  if [ ! -e AppRun ]; then
    if ! wget -O AppRun "https://github.com/AppImage/AppImageKit/releases/download/continuous/AppRun-${ARCH}" ; then
      echo "Could not download AppRun for the arch '${ARCH}'."
      exit 1
    else
      chmod 0755 AppRun
    fi
  fi
}

build_litexl(){
  if [ -e build ]; then
    rm -rf build
  fi
  
  echo "Build lite-xl..."
  sleep 1
  meson setup --prefix=/usr build
  meson compile -C build
}

generate_appimage(){
  if [ -e LiteXL.AppDir ]; then
    rm -rf LiteXL.AppDir
  fi
  
  echo "Creating LiteXL.AppDir..."
  
  mkdir LiteXL.AppDir
  DESTDIR="$(realpath LiteXL.AppDir)" meson install --skip-subprojects -C build
  mkdir -p LiteXL.AppDir/usr/lib/
  mkdir -p LiteXL.AppDir/usr/share/icons/hicolor/scalable/apps
  mkdir -p LiteXL.AppDir/usr/share/applications
  cp AppRun LiteXL.AppDir/
  cp resources/icons/lite-xl.svg LiteXL.AppDir/
  cp resources/icons/lite-xl.svg LiteXL.AppDir/usr/share/icons/hicolor/scalable/apps/
  cp resources/linux/lite-xl.desktop LiteXL.AppDir/
  cp resources/linux/lite-xl.desktop LiteXL.AppDir/usr/share/applications/
  
  echo "Copying libraries..."
  
  local allowed_libs=(
    libfreetype
    libpcre2
    libSDL2
    libsndio
    liblua
  )

  while read line; do
    local libname="$(echo $line | cut -d' ' -f1)"
    local libpath="$(echo $line | cut -d' ' -f2)"
    for lib in "${allowed_libs[@]}" ; do
      if echo "$libname" | grep "$lib" > /dev/null ; then
        cp "$libpath" LiteXL.AppDir/usr/lib/
        continue 2
      fi
    done
    echo "  Ignoring: $libname"
  done < <(ldd build/src/lite-xl | awk '{print $1 " " $3}')
  
  echo "Generating AppImage..."
  local version=""
  if [ "$1" != "" ]; then
    version="-$1"
  fi
    
  $APPIMAGETOOL LiteXL.AppDir LiteXL${version}-${ARCH}.AppImage
}

setup_appimagetool
download_appimage_apprun
build_litexl
generate_appimage $1
