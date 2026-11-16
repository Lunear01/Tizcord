.PHONY: all server client cert clean

all: server client

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client

# Self-signed cert the client pins. IP= adds the LAN address the client dials.
IP ?= 127.0.0.1
cert:
	mkdir -p certs
	openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
	  -keyout certs/server.key -out certs/server.crt -days 3650 -nodes \
	  -subj "/CN=tizcord" \
	  -addext "subjectAltName=IP:127.0.0.1,IP:$(IP),DNS:localhost"

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean