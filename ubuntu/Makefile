all:
	@echo "Please 'sudo make install'"

install:
	install -m0755 xterminal.init /etc/init.d/xterminal
	install -m0755 ../xterminal_broker.lua /usr/sbin/
	/etc/init.d/xterminal start
	
uninstall:
	/etc/init.d/xterminal stop
	rm -f /etc/init.d/xterminal /usr/sbin/xterminal_broker.lua
