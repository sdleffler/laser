package = "laser"
version = "scm-1"
source = {
   url = "*** please add URL for source tarball, zip or repository here ***"
}
description = {
   homepage = "*** please enter a project homepage ***",
   license = "MIT"
}
dependencies = {
    "lua >= 5.1, < 5.4"
}
build = {
   type = "builtin",
   modules = {
      globalize = "lib/globalize.lua";

      bitset = "src/bitset.c";
   }
}
