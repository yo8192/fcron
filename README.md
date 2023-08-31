See http://fcron.free.fr/ and http://fcron.free.fr/doc/en/.

Alternatively, if you downloaded fcron as a tarball, see:
* readme.txt in directory doc/en/txt/
* or readme.html in directory doc/en/HTML/.

If you cloned the git repo, you first need to build the documentation.
On a Debian based system:
```
$ sudo apt install -y docbook docbook-xsl docbook-xml docbook-utils manpages-dev
$ autoconf
$ ./configure  # optionally, use: --without-sendmail
$ make updatedoc
```
