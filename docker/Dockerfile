FROM acropolis-base
MAINTAINER Simeon van der Steen <simeon.vandersteen@bbc.co.uk>

ENV DEBIAN_FRONTEND noninteractive

ENV APACHE_RUN_USER	www-data
ENV APACHE_RUN_GROUP	www-data
ENV APACHE_LOG_DIR	/var/log/apache2
ENV APACHE_PID_FILE	/var/run/apache2.pid
ENV APACHE_RUN_DIR	/var/run/apache2
ENV APACHE_LOCK_DIR	/var/lock/apache2

# Install Spindle-specific dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
	liburi-dev libsparqlclient-dev \
	libtwine-dev libquilt-dev libsql-dev \
	libawsclient-dev twine quilt-fcgi \
	apache2 libapache2-mod-fcgid

RUN a2enmod rewrite

# Mount dir
COPY . /usr/local/src/

# Compile
RUN 	cd /usr/local/src && \
	autoreconf -i && \
	./configure --enable-debug && \
	make && \
	make install

# Adjust Apache config
RUN echo \ 
'<VirtualHost *:80>\n\
	DocumentRoot /usr/share/quilt/public\n\
	\n\
	CustomLog /var/log/apache2/quilt.access.log combined\n\
	ErrorLog /var/log/apache2/quilt.error.log\n\
	\n\
	Options FollowSymlinks ExecCGI\n\
	\n\
	AddHandler fcgid-script .fcgi\n\
	\n\
	RewriteEngine on\n\
	\n\
	RewriteCond %{LA-U:REQUEST_FILENAME} !-f\n\
	RewriteRule ^(.*)$ /index.fcgi [NS,L]\n\
</VirtualHost>' > /etc/apache2/sites-available/quilt
RUN a2dissite 000-default && a2ensite quilt
RUN	ln -sf /dev/stdout /var/log/apache2/quilt.access.log && \
	ln -sf /dev/stderr /var/log/apache2/error.log

# Adjust Quilt config
RUN 	sed -i -e 's|engine=file|engine=spindle|' /etc/quilt.conf && \
	sed -i -e 's|module=file.so|module=spindle.so|' /etc/quilt.conf && \
	echo '\n\
[spindle]\n\
graph=http://bbcarchdev.net/\n\
bucket=spindle\n\
db=pgsql://postgres:postgres@postgres/postgres\n\
\n\
[s3]\n\
endpoint=s3:4569\n\
access=x\n\
secret=x' >> /etc/quilt.conf

# Adjust Twine config
RUN 	sed -i -e 's|update=|; update=|' /etc/twine.conf && \
	sed -i -e 's|data=|; data=|' /etc/twine.conf && \
	sed -i -e 's|query=|; query=|' /etc/twine.conf && \
	echo '\n\
[plugins]\n\
module=rdf.so\n\
module=spindle.so\n\
\n\
[sparql]\n\
update=http://4store:8080/update/\n\
data=http://4store:8080/data/\n\
query=http://4store:8080/sparql/\n\
\n\
[spindle]\n\
graph=http://bbcarchdev.net/\n\
multigraph=yes\n\
bucket=spindle\n\
db=pgsql://postgres:postgres@postgres/postgres\n\
\n\
[s3]\n\
endpoint=s3:4569\n\
access=x\n\
secret=x' >> /etc/twine.conf

COPY docker/entrypoint.sh /

ENTRYPOINT ["/entrypoint.sh"]

EXPOSE 80

CMD ["spindle"]
