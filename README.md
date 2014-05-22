
Library-level Fault Injection Toolkit
=====================================

LFI automatically identifies the errors exposed by shared libraries, finds potentially buggy error recovery code in program binaries, and produces corresponding injection scenarios. LFI injects the desired faults – in the form of error return codes and corresponding side effects – at the boundary between shared libraries and applications. 

Installation
------------
    git clone https://github.com/dslab-epfl/lfi.git
    cd lfi && make

###Dependencies
* gcc/clang
* libxml-dev (or libxml-devel)
* [optional] libelf-dev and libdwarf

###Operating system
LFI was tested on 64-bit MacOS and Linux systems. It *should* also work on their 32-bit counterparts.

Usage
-----

    ./libfi <configuration file> [-t <subject executable>]

LFI comes with several example fault injection plans; we can use this one for a quick test:

###Basic scenario #1
    ./libfi scenarios/sampleplan.xml -t /bin/ls

If everything is OK you should see <tt>ls</tt> running and failing with a "Bad file descriptor" error.

###Basic scenario #2 (MacOS)

    ./libfi -t /Applications/Safari.app/Contents/MacOS/Safari scenarios/macos.simple.xml

###Scenarion #3

Next we will try to inject a fault in the communication between the PostgreSQL client and the database. First, install PostgreSQL if you don't have it already (e.g., apt-get install postgresql on Ubuntu) and start the server. Start the PostgreSQL client by typing "psql" and set up a database with some data:

<code><pre>
CREATE TABLE friends ( name VARCHAR(30), age INT );
INSERT INTO friends VALUES ('John', 24), ('Mary', 33), ('Bob', 42);
SELECT * FROM friends;
 name | age 
------+-----
 John |  24
 Mary |  33
 Bob  |  42
(3 rows)
\q
</pre></code>

Quit the client and create a fault injection plan as follows (you can name it *myplan.xml*):


    <?xml version="1.0" encoding="UTF-8"?>
    <plan>
      <trigger id="module_libpq" class="CallStackTrigger">
        <args>
          <frame>
            <module>libpq.so.5.1</module>
          </frame>
        </args>
      </trigger>
      <trigger id="cc1" class="CallCountTrigger">
        <args>
          <callcount>3</callcount>
        </args>
      </trigger>

      <function name="recv" retval="-1" errno="EBADF">
        <triggerx ref="module_libpq" />
        <triggerx ref="cc1" />
      </function>
    </plan>

This plan tells LFI to intercept the <tt>recv()</tt> function (which is a libc API call) and, on the 3rd call made by libpq to the function, inject a fault that returns value -1 and sets errno to <tt>EBADF</tt>. The scenario uses two triggers:

* The callstack trigger *module_libpq* that makes the fault be injected only if the call is made from the <tt>libpq</tt> module. We use the <tt>libpq.so</tt> library here because the PostreSQL client uses <tt>libpq</tt> to communicate with the database.
* The call count trigger *cc1* that allows the injection to occur only at the 3rd call to the <tt>recv()</tt> function.

Now run LFI as follows

    ./libfi myplan.xml -t /usr/bin/psql

You should witness something like this

<pre><code>
% psql
Welcome to psql 8.3.7, the PostgreSQL interactive terminal.

Type:  \copyright for distribution terms
       \h for help with SQL commands
       \? for help with psql commands
       \g or terminate with semicolon to execute query
       \q to quit

postgres=# select * from friends;
 name | age 
------+-----
 John |  24
 Mary |  33
 Bob  |  42
(3 rows)

postgres=# select * from friends;
could not receive data from server: Bad file descriptor

postgres=# select * from friends;
 name | age 
------+-----
 John |  24
 Mary |  33
 Bob  |  42
 </code></pre>

What is seen here is a fault that interferes with the PostgreSQL's client ability to receive the data from the server.

If you're wondering why the fault occurs on the 2nd call, it's because psql makes one call to <tt>recv()</tt> during startup; if you add that in, you will see that it is the 3rd time we use <tt>recv()</tt> that the fault is observed.

For further details about LFI, the available triggers and how to write your own, see the [documentation](https://github.com/dslab-epfl/lfi/wiki).
