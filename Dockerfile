# ---- Build stage ----
FROM alpine:3.19 AS builder

RUN apk add --no-cache gcc musl-dev make

WORKDIR /build
COPY axiom.c Makefile ./

ARG VERSION=dev
RUN make VERSION=${VERSION}

# ---- Runtime stage ----
FROM alpine:3.19

RUN apk add --no-cache libgcc

WORKDIR /root
COPY --from=builder /build/axiom /usr/local/bin/axiom

ENTRYPOINT ["axiom"]
