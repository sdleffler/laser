docs:
	ldoc .

test:
	busted --lua=luajit spec/*_spec.lua

benchmark:
	busted spec/*_tsc_benchmark.lua

all:
	test
