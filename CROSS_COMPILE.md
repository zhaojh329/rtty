# Build libev

    git clone https://github.com/enki/libev.git
    cd libev
    ./configure --host=arm-linux-gnueabi
    DESTDIR=/tmp/rtty_install make install

# Build rtty

    git clone --recursive https://github.com/zhaojh329/rtty.git
    cd rtty
    cmake . -DCMAKE_C_COMPILER=arm-linux-gnueabi-gcc -DCMAKE_FIND_ROOT_PATH=/tmp/rtty_install
    DESTDIR=/tmp/rtty_install make install

# Copy these files to your device's corresponding directory

    /tmp/rtty_install/
    └── usr
        └── local
            ├── bin
            │   └── rtty
            └── lib
                ├── libev.so -> libev.so.4.0.0
                ├── libev.so.4 -> libev.so.4.0.0
                ├── libev.so.4.0.0
