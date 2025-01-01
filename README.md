# Getting started

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
# Project status

The project maintainer, @yo8192, has limited time to work on this project beyond maintenance. Contributions are very welcome, and changes and new features still get added via third party contributions. To ensure your work is as productive as possible and your PRs are accepted and merged smoothly, please get in touch by raising an issue to discuss what you'd like to do, get guidance and pointers etc, before spending a significant amount of time.
