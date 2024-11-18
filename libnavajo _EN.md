**libnavajo : Web Interfaces and REST APIs for C++ Projects**

### **Introduction**

In today’s world, where speed, accessibility, and seamless deployment are key, **web interfaces** and **REST APIs** have become essential. Yet, integrating these technologies into C++ projects often feels like navigating a maze of complex architectures and dependencies.

Enter **libnavajo**—a lightweight, high-performance C++ framework that lets you quickly build robust web servers and dynamic interfaces with minimal effort. Say goodbye to cumbersome middleware and intricate setups. With just a few lines of code, libnavajo empowers you to deploy secure HTTP/HTTPS servers, serve files, handle dynamic content, and implement authentication—all within your existing C++ applications.

Whether you’re modernizing legacy software or building something entirely new, libnavajo offers a straightforward path to leveraging the latest web technologies directly in C++. Dive in and discover how this powerful framework can simplify your development process while ensuring top-notch performance.

###  **1\. Concept and Implementation**

#### **1.1 Introduction**

There is undeniable value in developing web interfaces. Indeed, they are scalable, performant, and interactive. They do not impose any server load: part of the application logic and rendering is done by your browser. Moreover, they have a significant advantage: backward compatibility. Those who, like me, had to migrate Gtk2 interfaces to Gtk3, or more recently Qt4 to Qt5, will understand ;-)

Web interfaces are multi-user, and they have reliable and mature authentication mechanisms. They are easily accessible\! No more configuring VNC, or users fighting over mouse control\! Today, they have become powerful, efficient, and even responsive design, meaning they can adapt their appearance depending on the device used (computer, tablet, smartphone…).

It was with this in mind that I developed the libnavajo project \[1\]. The idea was to offer C/C++ developers, the real ones ;-), a complete framework that includes a fast and efficient web server, with its web repositories, dynamically generated pages, and all the mechanisms to handle sessions, cookies, parameter passing (post and get), content compression, various mime types, keep-alive, SSL encryption (https), X509 authentication, etc… and the ability to generate dynamic content directly by accessing the application objects without any additional layer. Of course, nothing prevents the use of additional middleware (CORBA \[2\], web services, or others…).

Since then, we have been using this framework regularly in our developments. It has been enriched and debugged over time. The current version is stable and mature. It has few dependencies and is relatively portable. It is distributed via GitHub under the LGPLv3 license \[3\].

#### **1.2 Compilation and Installation**

The packages `zlib-devel`, `openssl-devel`, `pam-devel`, `doxygen`, and `graphviz` must be installed to compile the libnavajo framework.

Next, let's retrieve the source code:

`$ git clone https://github.com/titi38/libnavajo.git`

Then, let's compile using the provided Makefile:

`$ make`

Generate the Doxygen documentation:

`$ make docs`

Compile the examples:

`$ make examples`

Finally, install the framework:

`$ sudo make install`

The scripts `dpkgBuild.sh` and `rpmbuild/libnavajo.el6.spec` are available to build packages for Debian- and RedHat-based distributions.

Running an example file allows you to verify that everything is working:

`$ cd examples/1_basic`

`$ ./example`  
`[2015-05-07 06:57:49] >  WebServer: Service is starting!`  
`[2015-05-07 06:57:49] >  WebServer: Listening on port 8080`  
`[2015-05-07 06:58:09] >  WebServer: Connection from IP: ::1`

At this point, you can connect to the server using your preferred browser by entering the URL:  
`http://localhost:8080`,  
`http://localhost:8080/dynpage.html`, or  
`http://localhost:8080/docs/`.

### **1.3 Log Management**

libnavajo is a comprehensive framework that includes its own centralized and efficient log management system. If not initialized, log messages are lost.

Three types of output are available. These inherit from the abstract class `LogOutput` and allow the addition of new log types to the standard output, a file, or the syslog journal.

`LogRecorder` is implemented as a singleton. It is accessed via the method `getInstance()`, which returns the unique instance of the object, creating it if it does not already exist.

*![][image1]*

*Fig. : Class Diagram of the Log Management System*

A macro `NVJ_LOG` simplifies logging calls by acting as a shortcut for `LogRecorder::getInstance()`. To initialize the log system, simply add instances of objects derived from `LogOutput`.

For displaying logs on the standard output, add the following at the beginning of your application:  
`NVJ_LOG->addLogOutput(new LogStdOutput);`

For writing logs to the file `navajo.log`:  
`NVJ_LOG->addLogOutput(new LogFile("navajo.log"));`

To add entries to the syslog with the identifier `navajo`:  
`NVJ_LOG->addLogOutput(new LogSyslog("navajo"));`

To write a message to all the added outputs, use the `append()` method, which takes two parameters: the message and a severity level.

There are six levels:  
`NVJ_DEBUG`, `NVJ_INFO`, `NVJ_WARNING`, `NVJ_ALERT`, `NVJ_ERROR`, and `NVJ_FATAL`.

**Example usage:**

`NVJ_LOG->append(NVJ_INFO, "This is an info log message");`

By default, `NVJ_DEBUG` messages are ignored. If you want to display them, set your `LogRecorder` to "debug" mode:  
`NVJ_LOG->setDebugMode(true);`

To release all resources, simply use:  
`LogRecorder::freeInstance();`

## **2\. The Web Server**

### **2.1 Basic Principle**

A web server implements the HTTP 1.1 protocol as defined in the RFC 2616 specifications \[4\]. Its role is to respond to resource requests from clients, which are addressed using a URL (Uniform Resource Locator).

The `WebServer` object in libnavajo fully manages the operation of one or more HTTP servers.  
The `WebServer` creates listening sockets—one per IP protocol (i.e., two for IPv4 and IPv6)—and manages the lifecycle of connections.

Each instance of the `WebServer` object is designed to start in its own execution thread. It then creates a connection pool (20 threads by default).

The web server receives resource requests and must tailor the response to the different protocol versions and features supported by the client.

For example, when my browser (Firefox) attempts to connect to the page  
[`http://localhost:8080/dynpage.html?param1=34`](http://localhost:8080/dynpage.html?param1=34),

the generated HTTP request is as follows:

GET /dynpage.html?param1=34 HTTP/1.1  
Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,\*/\*;q=0.8  
Accept-Encoding:gzip, deflate, sdch  
Accept-Language:fr-FR,fr;q=0.8,en-US;q=0.6,en;q=0.4  
Cache-Control:max-age=0  
Connection:keep-alive  
Host:localhost:8080  
...

### When a client (browser) makes a request, the server must generate a response based on the metadata contained in the request headers.

For example, `Accept-Encoding: gzip` will be interpreted by the server, which will automatically compress the content of responses if they are large enough. Connections will use the keep-alive mechanism, which is the default mode in HTTP 1.1. The `WebServer` will therefore use persistent TCP connections, allowing it to respond to multiple requests using the same socket. Otherwise, the connection would have been closed after each response.

An instance of `WebServer` has a list of repositories that reference the available resources. These are `WebRepository` objects. It queries them one by one until it finds the requested resource, in this case: `/dynpage.html`. If it does not belong to any repository, the server will respond with a standardized error message: **"404 \- Not Found"**.

In the case of our sample application, the code **"200 OK"** is returned, followed by a new server header and the resource, here in uncompressed HTML format.

### **2.2 Lifecycle of WebServer**

As a picture is worth a thousand words, I invite you to refer to this **figure**:

![][image2]

*Fig.: Sequence Diagram of a libnavajo Application*

A **libnavajo application** always begins by instantiating a `WebServer` object. It then passes its configuration parameters and adds repositories to it.

Finally, you need to activate the service by calling the method `startService()`. The server will fully initialize and start responding to incoming requests.

When the service is stopped, the server processes any pending requests before shutting down and releasing resources.

### **2.3 Creating and Configuring a WebServer**

#### **2.3.1 Creation**

Implementing a Navajo web server within an application only requires a few lines of code:  
`// Include all the prototypes and predicates from libnavajo`  
`#include "libnavajo/libnavajo.hh"`

`// Create an instance of WebServer`  
`auto webServer = std::make_unique<WebServer>();`

And that's it\! We have created our first web server that listens on port 8080\!

#### **2.3.2 HTTPS Configuration**

We will now start our server on a different port. Port 8443 is more suitable for an HTTPS server:  
`webServer->listenTo(8443);`

*✍️ Creating a socket on a TCP port \< 1024 is reserved for the root user on Unix systems.*

To start the server in HTTPS mode, you will need a key/certificate pair. If you do not have one for your machine, you can generate a self-signed pair. To do this, use the following Unix `openssl` commands:  
`$ openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout mykey.pem -out mycert.pem`

⚠️ This certificate is not signed by a recognized certificate authority, so your browser will display a warning message.

We need to concatenate the key and certificate:  
`$ cat mykey.pem >> mycert.pem`

To enable SSL mode on our web server, simply add:  
`webServer->setUseSSL(true, "mycert.pem");`

This function activates SSL mode. Congratulations\! You've just created an HTTPS server\!

**2.3.3 X509 Authentication**

When SSL encryption is enabled, it is possible to activate X509 authentication based on Distinguished Names (DN).

To do this, simply provide your web server with a file containing all the concatenated public keys of the certificate authorities, followed by a list of authorized DNs:  
`webServer->setAuthPeerSSL(true, "ca-cnrs.pem");`

`webServer->addAuthPeerDN("/C=FR/O=CNRS/OU=UMR5821/CN=Thierry Descombes/emailAddress=...");`

The browser will then request a certificate when trying to access the page. Access to the resource is granted only if the certificate's DN is valid.

###  **2.3.4 Other Authentication Methods**

The framework also allows enabling HTTP authentication:

Either by specifying login/password pairs using the function, repeated as many times as there are accounts to authorize:  
`webServer->addLoginPass("login", "password");`

Or by using the system's PAM (Pluggable Authentication Module) authentication:  
`webServer->usePamAuth("/etc/pam.d/login");`

And optionally limiting access to specific accounts:  
`webServer->addAuthPamUser("descombes");`  
`webServer->addAuthPamUser("znets");`

⚠️ Credential theft over HTTP is possible. These methods should be reserved for HTTPS servers.

### **2.3.5 Access Restrictions**

You can restrict access to your server to specific networks or machines. This can be done using the `IpNetwork` object from libnavajo, which allows easy manipulation of IPv4 and IPv6 network addresses:  
`webServer->addHostsAllowed(IpNetwork(string("127.0.0.0/8")));`  
`webServer->addHostsAllowed(IpNetwork(string("192.168.0.0/24")));`

The server will only respond to requests from the `192.168.0.0/24` network or the loopback network (`localhost`).

The methods `listenIpV4only` and `listenIpV6only` allow you to restrict access to either an IPv4 or IPv6 network by creating only one listening socket.

### **2.3.6 Adding Repositories**

Adding web repositories (`WebRepository` objects), which we'll cover in more detail later, is done via calls to the `addRepository` method:  
`webServer->addRepository(myRepo);`

### **2.4 Starting and Stopping**

The `WebServer` starts responding to requests after calling the `startService` method:  
`webServer->startService();`

The process starts in a new thread. You can then use the `wait` method, which will block as long as the web server is running.

The `WebServer` stops after calling `stopService`:  
`webServer->stopService();`

The function does not return control until the complete shutdown of the web server. The object can then be destroyed.

---

## **3\. Web Repositories (`WebRepository`)**

A `WebRepository` defines a set of resources that can be returned to the client.

The `WebRepository` class is abstract and only contains two pure virtual methods: `getFile`, which returns the resource (URL) if found, and `freeFile`, which releases memory.

As shown in next **figure**, in the current version of libnavajo, there are three types of `WebRepository` to manage:

1. **LocalRepository**: For resources stored locally on disk.  
2. **PrecompiledRepository**: For pre-compiled resources.  
3. **DynamicRepository**: For dynamically generated content.

*![][image3]*  
*Fig. : The Three Types of WebRepository*

*✍️ When a requested URL ends with a `/`, the web server will attempt to return the `index.html` file.*  
*URLs are case-sensitive, regardless of the repository.*

### ***3.1 Local Repositories***

The `LocalRepository` object allows you to add local directories from which the libnavajo server will serve files.

First, create an instance of the object:  
`LocalRepository myLocalRepo;`

Then, add a resource directory, which will be recursively traversed through all its subdirectories:  
`myLocalRepo.addDirectory("/docs", "../docs/html");`

In our example program, `../docs/html` corresponds to the relative path to the project's Doxygen documentation, previously generated using the `make docs` command. The entire repository is attached to the `docs` folder located at the root of the web hierarchy.

The repository can then be added to the repositories served by the server:  
`webServer->addRepository(&myLocalRepo);`

Accessing `http://myServer:8080/docs/` will refer to the file `../docs/html/index.html`.

*✍️ You can, of course, add multiple directories to serve through your `LocalRepository`.*

### ***3.2 Precompiled Repositories***

The `PrecompiledRepository` class allows you to include your repositories directly in the application code, producing only a single binary. This has multiple advantages: you can create compact applications that are easy to deploy while ensuring the integrity of your interface (there’s little risk of it being modified if you do not provide the source code, especially since your web files can also be compressed).

The `navajoPrecompiler` tool, which is part of the framework, generates an implementation of the `PrecompiledRepository` class containing your precompiled repository. It takes the root directory (relative or absolute) as an argument and generates an implementation of the `PrecompiledRepository` class, which you must compile with your application.

In our example program, the command:  
`navajoPrecompiler exampleRepository > PrecompiledRepository.cc`

generates a file `PrecompiledRepository.cc` that implements the `PrecompiledRepository` class.

You can now simply create an instance of `PrecompiledRepository` and add it to the server's repositories:  
`PrecompiledRepository myPrecompiledRepo("/");`  
`webServer->addRepository(&myPrecompiledRepo);`

Your repository is then attached to the root `/` of the server.

*✍️ In the current implementation, there can be only one precompiled repository per application.*  
*When a file is compressed (with a `.gz` extension) in a precompiled repository, the libnavajo framework will decompress it on the fly or return the resource as-is to the client if it supports compression. This feature allows for a smaller repository, optimizing memory usage.*

### ***3.3 Dynamic Repositories***

Unlike the previous sections, it is possible to create a repository of dynamic pages, which consists of web pages generated on demand. Implementing these pages allows you to handle interactivity in your interface, analyze form values, or display information related to the operation of your application.

A dynamic repository is easily instantiated and requires no parameters:  
`DynamicRepository myRepo;`

It consists exclusively of dynamic pages (`DynamicPage`), which we will cover in another article. These pages are added with their absolute URL using the `add()` method:  
`myRepo.add("/dynpage.html", &page1);`

As you may have noticed, there are no constraints on the extension or the name of a dynamic page (here, `.html`).

As with other repositories, adding it to the server is done with:  
`webServer->addRepository(&myRepo);`

# 

### **4\. Dynamic Content and Web Application Design**

Now, let's dive into the mechanisms and objects that enable interactivity, specifically how to dynamically generate a page using the libnavajo framework.

#### **4.1 The `HttpRequest` and `HttpResponse` Objects**

Each request received by our `WebServer` instance generates two objects:

* **`HttpRequest`**: Contains all the request parameters, accessible via various accessors such as the requested URL, request type (GET, POST, etc.), cookies, and more.  
* **`HttpResponse`**: Manages all the parameters for the response to be sent back to the client, including response content, its size, MIME type, cookies, etc.

These objects are then passed to the `WebRepository` through calls to the `getFile()` method. The `DynamicRepository`, which handles dynamic pages, is queried. If it contains the requested resource, it passes references to the `HttpRequest` and `HttpResponse` objects to the `DynamicPage` object, which uses them to dynamically generate the response.

![][image4]

*Fig.: Sequence Diagram of Dynamic Page Access*

#### **4.2 Creating Dynamic Pages**

Every dynamic page must be instantiated at application startup. Let's look at an example of a dynamic libnavajo page that implements an access counter:

`// Exemple : Création d'une page dynamique simple`  
`class MyFirstDynamicPage : public DynamicPage {`  
    `int nbUsed = 0;`  
    `bool getPage(HttpRequest* request, HttpResponse* response) override {`  
        `std::stringstream ss;`  
        `ss << “<html><body>This page has been accessed "`  
 	     `<< ++nbUsed << " times</body></html>";`  
        `return fromString(ss.str(), response);`  
    `}`  
`} myFirstDynamicPage;`

In this example, we use the `fromString()` function (inherited from `DynamicPage`) to generate the response content from a string. The response could also be in binary format (e.g., an image or a downloadable file).

The page can then be added to our dynamic repository:

`DynamicRepository myRepo;`  
`myRepo.add("/index.html", &myFirstDynamicPage);`

Note that you can use any extension (`.html`, `.txt`, etc.) to serve dynamic content. The HttpResponse object determines the MIME type based on the file extension by default, but you can override it using:

`response->setMimeType("text/html");`

Dynamic content is automatically deallocated by the `WebServer` after the resource is served to the client.

#### **4.3 Handling HTTP Parameters**

There are two main ways to send parameters to the server: **GET** and **POST**.

* **GET** parameters are passed directly in the URL (e.g., `http://myserver/mypage.html?param1=value1&param2=value2`). The URL length is limited, so it is not recommended for long data.  
* **POST** parameters are sent within the HTTP request body, making them invisible in the URL. There is no theoretical size limit.

Regardless of the method, you can retrieve parameter values using the `HttpRequest` object:

`std::vector<std::string> myParams = request->getParameterNames();`  
`std::string param1 = request->getParameter("param1");`

To convert a parameter value to a different type:

`int paramValue = 0;`  
`try {`  
  `if (!request->getParameter("param1", paramValue)) {`   
	`response->setStatusCode(400);`   
	`return fromString("Bad Request: Missing 'param1'", response); }`  
  `paramValue = getValue<int>(request->getParameter("param1"));`  
`} catch (...) {`  
  `// Handle std::bad_cast exception`  
`}`

#### **4.4 Using Cookies**

HTTP cookies are small pieces of data stored in the browser and sent with every request. They help add statefulness to the otherwise stateless HTTP protocol (defined in RFC6265). Cookies are commonly used to manage session data, user preferences, and other browser-side information.

To create cookies, use the `addCookie()` method of the `HttpResponse` object:

`response->addCookie("username", "JohnDoe", 3600, 0, "/", "", true, true);`

* **Name/Value**: The key-value pair for the cookie.  
* **maxAge**: Validity period in seconds.  
* **expiresTime**: Expiration date (in Unix timestamp).  
* **path**: Limits the scope of the cookie to a specific path.  
* **domain**: Limits the cookie to a specific domain.  
* **secure**: Sends the cookie only over HTTPS.  
* **httpOnly**: Restricts access to HTTP requests only (not accessible via JavaScript).

When receiving a request, use the following methods to retrieve cookies:

`std::vector<std::string> cookieNames = request->getCookiesNames();`  
`std::string cookieValue = request->getCookie("username");`

*✍️ Always treat cookies as potentially insecure and avoid storing sensitive data in them.*

#### **4.5 Managing Sessions**

Cookies are essential for managing sessions, which allow the server to identify each user. Libnavajo automatically handles sessions using cookies, associating a randomly generated session key with each client.

The `HttpRequest` object provides methods for session management:

`std::vector<std::string> attrNames = request->getSessionAttributeNames();`  
`void* myAttr = request->getSessionAttribute("myAttr");`  
`request->setSessionAttribute("myAttr", (void*)data);`  
`request->removeSession();`

Here’s an example that counts the number of accesses for each user:

`class MySecondDynamicPage: public DynamicPage {`  
  `bool getPage(HttpRequest* request, HttpResponse* response) {`  
    `int *count = (int*)request->getSessionAttribute("accessCount");`  
    `if (count == nullptr) {`  
      `count = new int(0);`  
      `request->setSessionAttribute("accessCount", count);`  
    `}`  
    `(*count)++;`  
      
    `std::stringstream ss;`  
    `ss << "<HTML><BODY>Welcome back! You've visited this page "`   
	  `<< *count << " times.</BODY></HTML>";`  
    `/response->setMimeType("text/html");`  
    `return fromString(ss.str(), response);`  
  `}`  
`} mySecondDynamicPage;`

*✍️* *Do not store object instances as session attributes due to automatic deallocation.*

---

### **5\. Developing libnavajo Applications**

#### **5.1 Architecture: Moving Towards Full JavaScript Client Applications**

Today’s powerful JavaScript and CSS frameworks enable the creation of fully autonomous **Web 2.0 applications** that communicate with the server asynchronously via AJAX requests. This approach allows for a clear separation between the **presentation layer** (handled by JavaScript) and the **business logic** (written in C++).

A common approach is to use **AJAX polling** to repeatedly send requests to the server and update the UI based on the responses. The preferred format for data exchange is usually **JSON** due to its simplicity and efficiency.

####  **5.2 A Bit of Practice 😉**

Now that you are fully familiar with the libnavajo framework (yes, you are\! 😉), let’s dive into a concrete example: the **“2\_gauge”** project, which I’ve added to the available sources.

I’ve developed a gauge to monitor the server’s CPU usage. For rendering, I used a dedicated graphical API (there are many available). The gauge is refreshed every second using AJAX polling.

Below is the sequence diagram illustrating how the application functions.

![][image5]  
*Fig.: Sequence Diagram of the Application*

###  **Front-End Code (JavaScript)**

`<!-- We include jQuery for AJAX calls and Google Charts for drawing the gauge →`  
`<script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>`  
`<script src="https://www.gstatic.com/charts/loader.js"></script>`

`<script>`  
`// Load Google Charts and initialize the gauge`  
`google.charts.load('current', { packages: ['gauge'] });`  
`google.charts.setOnLoadCallback(initChart);`

`function initChart() {`  
    `// Creating the chart object and setting initial data`  
    `const chart = new`  
`google.visualization.Gauge(document.getElementById('chart_div'));`  
    `const data = google.visualization.arrayToDataTable([`  
        `['Label', 'Value'],`  
        `['CPU', 0]`  
    `]);`

    `// Chart options for colors and sizes`  
    `const options = {`  
        `width: 400, height: 120,`  
        `redFrom: 80, redTo: 100,`  
        `yellowFrom: 60, yellowTo: 80,`  
        `minorTicks: 5`  
    `};`

    `// Automatically refresh the gauge every second with AJAX polling`  
    `setInterval(() => updateAjax(chart, data, options), 1000);`  
`}`

`// Function to update the gauge with new data`  
`function updateChart(value, chart, data, options) {`  
    `data.setValue(0, 1, value);`  
    `chart.draw(data, options);`  
`}`

`// Function to fetch CPU usage from the server via AJAX`  
`function updateAjax(chart, data, options) {`  
    `$.ajax({`  
        `url: 'cpuvalue.txt',`  
        `success: function (value) {`   
            `// If the server returns a valid value, update the chart`  
            `if (value > 0) updateChart(value, chart, data, options);`  
        `},`  
        `error: function () {`  
            `console.error("Failed to fetch CPU value");`  
        `}`  
    `});`  
`}`  
`</script>`

`<!-- HTML element where the gauge will be rendered →`  
`<div id="chart_div"></div>`

---

### **Back-End Code (C++)**

#### **Step 1: Retrieving CPU Load**

`#include <fstream>`  
`#include <sstream>`  
`#include <string>`

`// Function to read CPU usage from /proc/stat`  
`// This function calculates the CPU load as a percentage`  
`int getCpuLoad() {`  
    `std::ifstream file("/proc/stat");`  
    `std::string line;`  
    `std::getline(file, line);`  
    `std::istringstream iss(line);`

    `// Read the CPU statistics`  
    `std::string cpu;`  
    `long user, nice, system, idle;`  
    `iss >> cpu >> user >> nice >> system >> idle;`

    `// Calculate the CPU load difference compared to the previous reading`  
    `static long prevTotal = 0, prevIdle = 0;`  
    `long total = user + nice + system + idle;`  
    `long totalDiff = total - prevTotal;`  
    `long idleDiff = idle - prevIdle;`

    `prevTotal = total;`  
    `prevIdle = idle;`

    `// Return CPU usage as a percentage`  
    `return totalDiff > 0 ? (100 * (totalDiff - idleDiff) / totalDiff) : 0;`  
`}`

---

#### **Step 2: Creating a Custom `DynamicRepository`**

`#include "libnavajo/libnavajo.hh"`

`// Base class to manage sessions`  
`class MyDynamicPage : public DynamicPage {`  
`protected:`  
    `// Check if the session is valid by looking for the "username" attribute`  
    `bool isValidSession(HttpRequest* request) {`  
        `return request->getSessionAttribute("username") != nullptr;`  
    `}`  
`};`

---

#### **Step 3: Authentication Page**

`// This page handles user authentication`  
`class Auth : public MyDynamicPage {`  
  `public:`  
    `bool getPage(HttpRequest* request, HttpResponse* response) override {`  
      `std::string login, password;`  
          
      `// Validate login and password from request parameters`  
      `if (request->getParameter("login", login)`   
	  `&& request->getParameter("pass", password)`   
	  `&& ((login == "libnavajo" && password == "libnavajo")`   
	   `|| AuthPAM::authentificate(login.c_str(), password.c_str(),`  
						`"/etc/pam.d/login"))) {`

        `// If valid, create a session attribute for the user`  
        `auto username = std::make_shared<std::string>(login);`  
        `request->setSessionAttribute("username", username);`

        `// Return "authOK" if successful`  
        `return fromString("authOK", response);`  
      `}`  
      `// Return "authBAD" if authentication fails`  
      `return fromString("authBAD", response);`  
    `}`  
`} auth;`

---

#### **Step 4: CPU Value Page**

`// This page returns the current CPU usage as a plain text response`  
`class CpuValue : public MyDynamicPage {`  
  `public:`  
    `bool getPage(HttpRequest* request, HttpResponse* response) override {`  
        `// Check if the session is valid`  
        `if (!isValidSession(request)) {`  
            `return fromString("ERR", response);`  
        `}`

        `// Retrieve the current CPU load and send it as a response`  
        `int cpuLoad = getCpuLoad();`  
        `std::ostringstream ss;`  
        `ss << cpuLoad;`  
        `return fromString(ss.str(), response);`  
    `}`  
`} cpuValue;`

---

#### **Step 5: Controller for Redirection**

`// This controller handles redirection based on session status`  
`class Controller : public MyDynamicPage {`  
`public:`  
    `bool getPage(HttpRequest* request, HttpResponse* response) override {`  
        `// Handle user logout by removing the session`  
        `if (request->hasParameter("disconnect")) {`  
            `request->removeSession();`  
        `}`

        `// If the session is valid, forward to the gauge page; otherwise, to the login page`  
        `if (isValidSession(request)) {`  
            `response->forwardTo("gauge.html");`  
        `} else {`  
            `response->forwardTo("login.html");`  
        `}`  
        `return true;`  
    `}`  
`} controller;`

---

#### **Step 6: Creating the `MyDynamicRepository`**

`// Custom dynamic repository to handle different resources`  
`class MyDynamicRepository : public DynamicRepository {`  
  `public:`  
    `MyDynamicRepository() {`  
        `add("/auth.txt", &auth);`  
        `add("/cpuvalue.txt", &cpuValue);`  
        `add("/index.html", &controller);`  
    `}`  
`} myRepo;`

---

### **Step 7: Main Server Implementation**

`#include "libnavajo/libnavajo.hh"`  
`#include <memory>`

`// Main function to set up and run the web server`  
`int main() {`  
    `auto webServer = std::make_unique<WebServer>();`  
    `webServer->listenTo(8080);`

    `// Add local repository to serve static files`  
    `LocalRepository myLocalRepo;`  
    `myLocalRepo.addDirectory("/", "./html");`  
    `webServer->addRepository(&myLocalRepo);`

    `// Add the custom dynamic repository`  
    `webServer->addRepository(&myRepo);`

    `// Start the web server and wait for it to finish`  
    `webServer->startService();`  
    `webServer->wait();`

    `// Clean up logging resources`  
    `LogRecorder::freeInstance();`  
    `return 0;`  
`}`

---

### **Project Structure**

* **gauge.html** → Contains the front-end code for displaying the gauge.  
* **login.html** → A simple login form for user authentication.  
* **cpuvalue.txt** → Provides the CPU usage as a percentage in text format.

### **Explanation of the Workflow**

* **AJAX Polling**: The gauge is updated every second by sending an AJAX request to `cpuvalue.txt`.  
* **Authentication**: A simple session-based authentication system.  
* **Redirection**: The controller redirects the user to the appropriate page based on the session status.

To illustrate libnavajo’s capabilities, consider a real-world example: a CPU usage gauge that updates every second using AJAX polling.

This project uses jQuery for AJAX calls and Google Charts for the graphical gauge. The server provides the data via dynamically generated content, protected with session-based authentication.

## **6\. HTML5 WebSockets**

Modern browsers support WebSockets. With this new protocol and its standardized API, JavaScript applications can communicate instantly with their servers.

### **6.1 WebSocket Protocol: Functionality and Standards**

The WebSocket protocol is defined by the IETF in RFC6455. It enables the establishment of **bi-directional** and **persistent** connections between a client and a server.

The server is referenced by a URI that follows the ABNF syntax:

`ws-URI = "ws:" "//" host [ ":" port ] path [ "?" query ]`

The protocol also allows the use of "secure WebSockets" based on SSL encapsulation, similar to HTTPS. The URIs for secure WebSockets look like:

`wss-URI = "wss:" "//" host [ ":" port ] path [ "?" query ]`

The default ports are **80** and **443**, similar to HTTP and HTTPS.

#### **6.1.1 WebSocket Handshake**

The client initiates the connection by sending a standard HTTP request. This allows the protocol to take advantage of HTTP headers, including authentication mechanisms. It also ensures compatibility with most proxies and firewalls.

The WebSocket connection request contains the headers: `Upgrade: websocket` and `Connection: Upgrade`. Here’s an example of the handshake:

makefile

Copier le code

`GET /chat HTTP/1.1`  
`Host: server.example.com`  
`Upgrade: websocket`  
`Connection: Upgrade`  
`Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==`  
`Origin: http://example.com`  
`Sec-WebSocket-Protocol: chat, superchat`  
`Sec-WebSocket-Version: 13`

The server responds with:

`HTTP/1.1 101 Switching Protocols`  
`Upgrade: websocket`  
`Connection: Upgrade`  
`Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`  
`Sec-WebSocket-Protocol: chat`

The client sends a randomly generated key in the `Sec-WebSocket-Key` header. The server responds with `Sec-WebSocket-Accept`, which is derived from the client’s key.

**Note**: Managing WebSocket server resources can be challenging. Since WebSockets are persistent, their number can grow indefinitely, potentially overwhelming the server. It is crucial to implement security measures, such as:

* Only allowing authenticated clients (validated session cookies).  
* Limiting each session to a single WebSocket connection.  
* Using encrypted protocols (wss://).  
* Validating the origin header (`Origin`).

---

### **6.2 The WebSocket API**

Modern JavaScript engines implement the standardized WebSocket API as defined by the W3C. It provides a persistent, bi-directional connection between a client and server.

The interface is as follows:

`const ws = new WebSocket("ws://localhost/test");`

`ws.onopen = function(evt) { console.log("Connection opened"); };`  
`ws.onclose = function(evt) { console.log("Connection closed", evt.reason); };`  
`ws.onmessage = function(evt) { console.log("Message received:", evt.data); };`  
`ws.onerror = function(evt) { console.error("Error:", evt.data); };`

This simple implementation demonstrates how a client connects to a WebSocket server and handles events.

---

## **7\. Developing WebSocket Applications with libnavajo**

### **7.1 Implementing a WebSocket Server with libnavajo**

Libnavajo is a C++ web server that also supports WebSockets. It can manage web pages, cookies, sessions, and WebSockets seamlessly.

To get started, download and install libnavajo:

`$ git clone https://github.com/titi38/libnavajo.git`  
`$ make`  
`$ sudo make install`

#### **Handling WebSocket Connections**

Libnavajo processes HTTP requests in a connection pool. When it receives a WebSocket connection request, it handles it specifically. If the connection is established, the socket becomes persistent.

Each client is referenced by an `HttpRequest` object, which is created from the WebSocket request. It persists as long as the WebSocket connection is active.

---

### **Example: Minimal WebSocket Server**

`#include "libnavajo/WebSocket.hh"`  
`#include <iostream>`

`// Define a WebSocket class that handles opening and receiving messages`  
`class MyWebSocket : public WebSocket {`  
  `public:`  
    `// Handle the opening handshake`  
    `bool onOpening(HttpRequest* request) override {`  
      `std::cout << "New WebSocket from host: "`   
		    `<< request->getPeerIpAddress().str()`  
                `<< " - socketId=" << request->getClientSockData()->socketId`   
		    `<< std::endl;`  
        `return true;`  
    `}`

    `// Handle receiving a text message`  
    `void onTextMessage(HttpRequest* request, const std::string& message, const bool fin) override {`  
        `std::cout << "Message received: '" << message << "' from host "`  
                  `<< request->getPeerIpAddress().str() << "\n";`  
        `sendTextMessage(request, "The message has been received!");`  
    `}`  
`} myWebSocket;`

`// Adding the WebSocket to the server`  
`int main() {`  
    `auto webServer = std::make_unique<WebServer>();`  
    `webServer->addWebSocket("/test", &myWebSocket);`  
    `webServer->listenTo(8080);`  
    `webServer->startService();`  
    `webServer->wait();`  
    `return 0;`  
`}`

This minimal WebSocket server responds to incoming messages with a confirmation message.

---

### **7.2 A More Complex Example: Chat Application**

Let’s create a chat application with authentication. The project is split into two files: an HTML page and a C++ application.

#### **7.2.1 Authentication**

##### **JavaScript Client Code for Authentication**

`function connect() {`  
    `$.ajax({`  
        `type: "POST",`  
        `url: "connect.txt",`  
        `data: {`  
            `login: $("#login-username").val(),`  
            `pass: $("#login-password").val()`  
        `},`  
        `success: function(response) {`  
            `if (response === 'authOK') connectWS();`  
            `else alert("Invalid username or password.");`  
        `},`  
        `beforeSend: function() {`  
            `console.log("Please wait...");`  
        `}`  
    `});`  
`}`

---

##### **C++ Server Code for Authentication**

`class MyDynamicRepository : public DynamicRepository {`  
  `public:`  
    `// Handle user login`  
    `class Connect : public DynamicPage {`  
      `public:`  
        `bool getPage(HttpRequest* request, HttpResponse* response) override {`  
            `std::string login, password;`  
            `if (request->getParameter("login", login) && request->getParameter("pass", password) &&`  
                `((login == "libnavajo" && password == "libnavajo") ||`  
                 `AuthPAM::authentificate(login.c_str(), password.c_str(), "/etc/pam.d/login"))) {`

                `request->setSessionAttribute("username", std::make_shared<std::string>(login));`  
                `request->setSessionAttribute("wschat", std::make_shared<bool>(false));`  
                `return fromString("authOK", response);`  
            `}`  
            `return fromString("authBAD", response);`  
        `}`  
    `} connect;`

    `// Handle user logout`  
    `class Disconnect : public DynamicPage {`  
      `public:`  
        `bool getPage(HttpRequest* request, HttpResponse* response) override {`  
            `request->removeSession();`  
            `return noContent(response);`  
        `}`  
    `} disconnect;`

    `MyDynamicRepository() {`  
        `add("connect.txt", &connect);`  
        `add("disconnect.txt", &disconnect);`  
    `}`  
`} myDynRepo;`

---

### **7.2.2 Handling WebSocket Messages**

`class ChatWebSocket : public WebSocket {`  
  `public:`  
    `bool onOpening(HttpRequest* request) override {`  
        `if (!isValidSession(request)) return false;`  
        `setSessionIsConnected(request, true);`  
        `return true;`  
    `}`

    `void onClosing(HttpRequest* request) override {`  
        `setSessionIsConnected(request, false);`  
    `}`

    `void onTextMessage(HttpRequest* request, const std::string& message, const bool fin) override {`  
        `if (checkMessage(request, message))`  
            `sendBroadcastTextMessage(message);`  
        `else`  
            `sendCloseCtrlFrame(request, "Invalid message format");`  
    `}`

  `private:`

    `bool isValidSession(HttpRequest* request) {`  
        `return request->getSessionAttribute("username") != nullptr;`  
    `}`  
`} chatWebSocket;`

The server accepts WebSocket connections, manages sessions, and broadcasts chat messages.

---

### **8\. Deploying WebSocket Applications**

### **8.1 Apache Configuration for WebSocket**  To deploy a libnavajo WebSocket application behind an Apache proxy:

`# Enable necessary Apache modules`  
`$ sudo a2enmod proxy`  
`$ sudo a2enmod proxy_http`  
`$ sudo a2enmod proxy_wstunnel`

In the Apache configuration:

`ProxyPass "/chat/wschat" "ws://localhost:8080/wschat"`  
`ProxyPass /chat http://localhost:8080/`  
`ProxyPassReverse /chat http://localhost:8080/`

Ensure the order of directives to avoid conflicts between WebSocket and HTTP connections.

### **8.2 Nginx Configuration for WebSocket**

Edit the Nginx configuration file (usually located at `/etc/nginx/sites-available/default` or `/etc/nginx/nginx.conf`) to include the following configuration:

`# Nginx configuration for WebSocket and HTTP traffic`  
`server {`  
    `listen 80;`  
    `server_name your-domain.com;`

    `# Proxy WebSocket connections`  
    `location /wschat {`  
        `proxy_pass http://localhost:8080/wschat;`  
        `proxy_http_version 1.1;`  
        `proxy_set_header Upgrade $http_upgrade;`  
        `proxy_set_header Connection "Upgrade";`  
        `proxy_set_header Host $host;`  
        `proxy_set_header X-Real-IP $remote_addr;`  
        `proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;`  
        `proxy_set_header X-Forwarded-Proto $scheme;`  
    `}`

    `# Proxy regular HTTP connections`  
    `location /chat {`  
        `proxy_pass http://localhost:8080/;`  
        `proxy_set_header Host $host;`  
        `proxy_set_header X-Real-IP $remote_addr;`  
        `proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;`  
        `proxy_set_header X-Forwarded-Proto $scheme;`  
    `}`

    `# Redirect HTTP to HTTPS (optional)`  
    `listen 443 ssl;`  
    `ssl_certificate /etc/ssl/certs/your-cert.pem;`  
    `ssl_certificate_key /etc/ssl/private/your-key.pem;`

    `ssl_protocols TLSv1.2 TLSv1.3;`  
    `ssl_ciphers HIGH:!aNULL:!MD5;`  
`}`

### **8.3 Explanation of Configuration:**

* **WebSocket Proxy (`/wschat`)**:  
  * The `proxy_pass` directive forwards WebSocket connections to your `libnavajo` WebSocket server running on port 8080\.  
  * The headers `Upgrade` and `Connection` are set to ensure WebSocket compatibility.  
* **HTTP Proxy (`/chat`)**:  
  * This location handles regular HTTP requests and forwards them to your `libnavajo` server.  
* **HTTPS Setup** (Optional):  
  * If you have an SSL certificate, you can enable HTTPS by specifying the certificate and key files.

  


[image1]: <data:image/png;base64,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>

[image2]: <data:image/png;base64,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>

[image3]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAUQAAACFCAYAAAAjKYFjAAA3h0lEQVR4Xu2dCbhN1fvHq3/1azaFSMiQWcmUIfPQXJo1RyFkyJCERJlVSEVUpiiFJEWmikoyhsyzZB5L8/s/n/fc91h3O/dyr+mea32fZz3nnL33WnsN7/qu77vW2vucIR4eHh4eijOCBzw8PDxOV3hC9PDw8IiDJ0QPDw+POHhC9PDw8IiDJ0SPVIP//vvPh9M4HA94QvTw8Eg1OFZi9IToEdOwDrB9+3ZZuHChLFq0SJYsWeLDaRIWL16snz/88IMcPHgwnk0kB54QPWIaf/zxh34+/PDDcumll8o555wjZ599tg+nSaC9zz//fMmaNasMHjxY/v77b0+IHqcvfv/9d/0sUKCA3HDDDfLFF1/I9OnTZdq0afrpQ+oNtPHkyZNlxowZki5dOmnVqpX89ddfAQtJGjwhesQ0zE26+uqrpWnTpoGzHqcL8ubNK61bt/YK0eP0hkuI9evXlwMHDsi///4r//zzj376kHoDbYyHwPcrr7xSnn32WSXEY4EnRI+YhkuIDRo0iLjQx6ISPGIDtLHNIefKlcsToofH8SREdz9bcuKnFJzKMnBPFNvJyIMnRA+PAJJCiHYMV2vLli2yfv16DZs2bZJ9+/Yddl1CcDt7Ur8Hj7m/CX/++ads3bpVO7p73A1gz549snHjxnhl+O233+Kl6X4P5sE97p6LFqKl4Qbqc/fu3YkuaATjBz+Dx44GXOsJ0cPDQVIIEeUCduzYIXfffbfceeedUrt2bXnwwQfl6aeflokTJyoh2fxjsMPa3JV9dz8NCZ0Lzn8Fr+GeYOXKlToX+tNPP+lvjrv3J3+gR48eWgbyf99998lDDz0kbdq00ZXXaHkIlid4PFoZQPC8m3fLG2TYu3dvWbdunRIS+0H37t0bL9/ud7tHMB9uHt172/VBcMwTooeHg6QQopHOhg0b5IorrpDnnntOBgwYIN26dZPbbrtNLrroIhk4cGAg1tHBJY0gEjoeBNeh+nr27CmrV6+OGs8IsVatWvLEE0/IW2+9Jf369VOCLF26tFx88cUyb968QKxDiJam4UhlSOjc/v37ZeTIkapsIaQ0adLIjz/+qOeiEZSRYmJw75XQfT0hengEkBRCtI4IIV533XWyefPmeOfZtpExY0Z1n2fNmhUhUICLzVMwENUvv/yiKmjMmDEydepUVUgGvnOMc999953+5r4rVqxQsiPeuHHjZPbs2Zp3fo8dO1bVFaCD//rrr5GOvnbtWvn444/1GlSjHUfdfvnll5H7AsqTOXNm6dChg5af9OfMmSOjR4+WKVOmqGoDkCpPeFCWTz75RD7//HN90gcQD3f8q6++kmHDhqlqRlEDykE+Ofbhhx9qGgAS4hrqnrSow1deeUV27typ54kzatQoLTflsTyYy08c8sk5Kx9YtmyZki2I1p6eED08AkguIRYvXlw7NPNexKFjffvtt3LmmWfqZt8zzjhDVQ7psJXnqaeekhIlSsg999wjVatWlUqVKslVV12l16E06bgoJNzdnDlzSuHChfV88+bN9Z5sGi9btqxUrFhR1emFF16o8a6//npVVKVKldJ8fP/993L55ZcrIZE3zvM7T548+vnRRx9pehAiJAkgF679+uuvpWDBghGVi9LMli2bbknhnuQNkiKfPN1Rp04dPZ4hQwZ54IEHIuTTokULffKjaNGi+slTQMxRgpo1a8q1116r6UJC06dPl127dml+qD+U61lnnaVpzpw5U+NVq1ZNN85nz55dKleurAQJgdatW1fTomw333yzlCxZUn7++WetcwYjystv2s0dnAyeED08AkguIUJuKBBAZyNAfKib+fPnyx133KHzigBlhqKEjNj8fckll6iCBLis+fLlUzeV52mZ1zOgytKmTat5Im+QBEQHmPuDTCdNmqTqERJFWZKnQoUKyYIFCzRApLZY0ahRI53vBMwZkua7774r/fv3ly5duiihFCtWTO8HuWXJkkXGjx8fyU/69OnlzTffVNV67rnnqjIDlC9HjhxKpNQnZOfOYZLvXr16KdmQpgFXvW3btlqvjzzySKQ+Uak8XwxuvPFGueWWWyJp8f3WW2/V35AwdYn6hiCpe0ietqM+IVBAnqK1pydED48AkkuIKERTI3QiVBafdGY6Lm5mpkyZtKP26dNHn4QAKCEeEcOtJr2lS5dqZ0QN4ZL27dtX81KhQgVVd6gfyOX+++9X5WUudOPGjeXRRx9V9xsiqVGjhj6Ly6IKaor0cDs/++yzSB6BlRfi7dq1q16HMoRoKT9ktmbNGs0XZIPKu+aaa3R+ERJE7S1fvlxJF5hCpkyUDRcWVcuKNffk3g0bNtT7oQQhcsiT+zDFwCCC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>

[image4]: <data:image/png;base64,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>

[image5]: <data:image/png;base64,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onadcgtL3FGiisOJEydOnKI32e05A6LcxRJ3lKjiDB48WIoWLerUU0RERJmXqF2n3MISd+RScfbt2+cUGBERUea5tOuUO1jijpKpOGZgZAdDdloiIsqMZNp1yn4scUepVBwERn69RURElHmptOuUvVjijlKtOH630YiIKPNSbdcpO7HEHeW34ujAyCUtERGlX37bdcouLHFHrDhERNmF7TqZWOKOWHGIiLIL23UyscQdseIQEWUXtutkYok7YsUh7dKlS3Lu3Dl7NoXA6NGj5fTp0/bslBTktiic2K6TiSXuiBUnN6xfv15+8IMfyP79++1Fsm3bNuncubOUKFFCFi9ebC+mEChZsqQcOnTInq3cuHHDnhVXvG1RdmC7TiaWuCNWnOx36tQpueOOO6R69eq+AdGJEyfk5MmT0qFDBwZEIRUviNmzZ489K65426LswHadTCxxR6w42a9v374yd+5cueuuu3wDIs0OiHr37i0bNmwwUvxDnTp15Pbbb5ehQ4dKx44dvfmDBg2Sxo0by5tvvqm+o/eidu3acv78efW9X79+Xlrsy9yOtnz5cmnUqJG0bdtWZs+e7c03NWnSRN5++21p2LChLFu2zJuP/bdu3TomD/Z+dH6D9oHj9qO3Y27D3p99vGvXro05Zr98AI7ZLy8agpj58+dL8+bNpX79+rJ69Wo1/4UXXlDHMWLECPUd52XevHnSokULlY/hw4dL1apVpX///jHbYkCU3diuk4kl7ogVJ/s99dRT6t9kAyL0PJw9e9ZI8b3r16+rW3BXrlyRF198UV3ktY8//li2bt2qLtqAAAF/R3o77du397aBB3qa29HpcSE/fPiwGs+EXq3Lly9/v3EDLuq42CNN+fLl1fYA+8c2dB789oP8Ik3QPvx6XMztmMds788+XgRr5jHb+QB9zH550XC8PXr0UL1506ZNk27dunnLzPwi3cCBA1VAhrFC7777rtoegqIdO3Z4aRgQZTe262RiiTtixclun376qbpoIxBC78G6det8L7hgB0RBzF6j7du3xwREuNiPGzdOihUrpr7bAYIODrANHTQBtgMbN26UihUryrBhw9SECznybDMv6pUqVZItW7aoz9g/ghOdB7/9IL/YT6J9mMztmMds788+XjMgCjpv5jEH5QXHq4NZbAc9UpodEH355Zfq81tvveXNRzC8cuVK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>