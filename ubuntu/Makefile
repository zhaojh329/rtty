all:
	@echo "Please 'sudo make install'"

install:
	rm -rf /etc/xterminal
	install -m0755 xterminal.init /etc/init.d/xterminal
	install -m0755 ../xterminal_broker.lua /usr/sbin/xterminal_broker
	update-rc.d xterminal defaults
	mkdir /etc/xterminal
	cp xterminal.conf /etc/xterminal
	cp -r ../www /etc/xterminal/www
	cp server.pem /etc/xterminal
	cp server.key /etc/xterminal
	
uninstall:
	update-rc.d xterminal remove
	rm -rf /etc/init.d/xterminal /usr/sbin/xterminal_broker.lua /etc/xterminal
