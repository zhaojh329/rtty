# Build libev

    git clone https://github.com/enki/libev.git
    cd libev
    ./configure --host=arm-linux-gnueabi
    DESTDIR=/opt/rtty_install make install

# Build libuwsc

    git clone https://github.com/zhaojh329/libuwsc.git
    cd libuwsc
    cmake . -DCMAKE_C_COMPILER=arm-linux-gnueabi-gcc -DCMAKE_FIND_ROOT_PATH=/opt/rtty_install -DUWSC_SSL_SUPPORT=OFF
    DESTDIR=/opt/rtty_install make install

# Build rtty

    git clone https://github.com/zhaojh329/rtty.git
    cd rtty
    cmake . -DCMAKE_C_COMPILER=arm-linux-gnueabi-gcc -DCMAKE_FIND_ROOT_PATH=/opt/rtty_install
    DESTDIR=/opt/rtty_install make install
