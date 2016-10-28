gn args out/Release --args="is_debug=false official_build=true enable_nacl=false"
ninja -C out/Release stable_deb
