gn args out/Release --args="is_debug=false ffmpeg_branding=\"Chrome\" official_build=true enable_nacl=false"
ninja -C out/Release stable_deb
