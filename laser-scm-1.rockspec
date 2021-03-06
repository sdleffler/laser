package = "laser"
version = "scm-1"
source = {
   url = "https://github.com/sdleffler/laser.git"
}
description = {
   homepage = "https://github.com/sdleffler/laser",
   license = "MIT"
}
dependencies = {
    "lua >= 5.1, < 5.4",
    "busted 2.0.rc12-1",
    "ldoc >= 1.4.5",
}
build = {
   type = "builtin",
   modules = {
      globalize = "lib/globalize.lua";

      bitset = "c/lib/bitset.c";
   }
}
