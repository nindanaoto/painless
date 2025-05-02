all:
	$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build"
kissat:
	$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build" kissat
tissat:
	$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build" tissat
clean:
	rm -f "/workspace/painless/solvers/kissat-inc"/makefile
	-$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build" clean
	rm -rf "/workspace/painless/solvers/kissat-inc/build"
coverage:
	$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build" coverage
indent:
	$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build" indent
test:
	$(MAKE) -C "/workspace/painless/solvers/kissat-inc/build" test
.PHONY: all clean coverage indent kissat test tissat
