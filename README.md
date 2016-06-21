# libnavajo

Framework to develop easily web interfaces in your C++ applications

Libnavajo makes it easy to run an HTTP server into your own application, implement dynamic pages, and include all the web pages you want (local or compiled into your project)


**Specifications** :

* Implementation is HTTP 1.1 compliant with SSL, X509 and HTTP authentification, compression Zlib...
* Very fast with its fully configurable pthreads pool
* Support for IPv4 and IPv6
* Precompiler to include a web repository into your code
* Generate dynamic and static contents 
* Cookies and advanced session management
* HTML5 Websockets support
* Multipart's Content support
* Fully oriented object
* Many examples included

**Download and install** :

    git clone https://github.com/titi38/libnavajo.git

    mkdir build

    cd build

    cmake ..

    make

    sudo make install


**Notes**:

Libnavajo is released under the CeCILL-C licence ( http://www.cecill.info/licences/Licence_CeCILL-C_V1-en.txt ) 

If you want to support us or contribute, please contact support@libnavajo.org
