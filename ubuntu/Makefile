all:
	@echo "Please 'sudo make install'"

install:
	rm -rf /etc/xterminal_web
	install -m0755 xterminal.init /etc/init.d/xterminal
	install -m0755 ../xterminal_broker.lua /usr/sbin/
	cp xterminal.conf /etc
	cp -r ../www /etc/xterminal_web
	
uninstall:
	rm -rf /etc/init.d/xterminal /usr/sbin/xterminal_broker.lua /etc/xterminal.conf /etc/xterminal_web
