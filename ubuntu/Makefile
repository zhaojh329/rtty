all:
	@echo "Please 'sudo make install'"

install:
	install -m0755 xterminal.init /etc/init.d/xterminal
	install -m0755 ../xterminal_broker.lua /usr/sbin/
	cp -r ../www /etc/xterminal_web
	
uninstall:
	rm -f /etc/init.d/xterminal /usr/sbin/xterminal_broker.lua
	rm -rf /etc/xterminal_web
