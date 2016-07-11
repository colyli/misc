default:
	g++ memmonitor.cpp -O2 -o memmonitor -lncurses -lpthread -lrt
	g++ memctrl.cpp -O2 -o memctrl
	g++ memtach.cpp -O2 -o memtach
	chmod +x pblaze_bug_report
clean:
	$(RM) memmonitor
	$(RM) memctrl
	$(RM) memtach
	chmod -x pblaze_bug_report
	
install:
	cp -f ./memmonitor /usr/bin/
	cp -f ./memctrl /usr/bin/
	cp -f ./memtach /usr/bin/
	cp -f ./pblaze_bug_report /usr/bin/

uninstall:
	rm -f /usr/bin/memmonitor
	rm -f /usr/bin/memctrl
	rm -f /usr/bin/memtach
	rm -f /usr/bin/pblaze_bug_report

