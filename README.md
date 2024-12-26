See http://fcron.free.fr/ and http://fcron.free.fr/doc/en/.
The up-to-date documentation is also available at https://yo8192.github.io/fcron/.

Alternatively, if you downloaded fcron as a tarball, see:
* readme.txt in directory doc/en/txt/
* or readme.html in directory doc/en/HTML/.

If you cloned the git repo, you first need to build the documentation.
On a Debian based system:
```
$ sudo apt install -y autoconf build-essential docbook docbook-xsl docbook-xml docbook-utils manpages-dev
$ autoconf
$ ./configure  # optionally, use: --without-sendmail
$ make updatedoc
```
