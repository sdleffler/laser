# Laser: miscellaneous odds and ends for Lua development

Intended to eventually be a collection of miscellaneous C-accelerated Lua
libraries. Documentation is available through LDoc.

Laser is intended to be compatible with LuaJIT, specifically for use with Love2D.

Current modules:
- `bitset`: a C-accelerated bitset type.
- `globalize`: a little trick to allow Lua modules to quickly infect the global namespace.

### Installation/Usage

1. Install luarocks and LuaJIT from the [Torch luajit-rocks repository.](https://github.com/torch/luajit-rocks)
2. Clone this repository (no `luarocks install` yet, sorry.)
3. Run `luarocks make`.

### Tests

Tests can be run by running `make test`. Uses `busted` for testing, and runs
busted with `--lua=luajit`.

### License

Copyright (c) 2016 Sean Leffler

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
